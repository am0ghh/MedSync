#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include "esp_camera.h"
#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "config.h"
#include "secrets.h"

// ── BLE globals ───────────────────────────────────────────────────────────────
static portMUX_TYPE   bleMux            = portMUX_INITIALIZER_UNLOCKED;
static String         bleCmd            = "";   // written by BLE task, read by loop()
static volatile bool  bleClientConnected = false;
static BLECharacteristic* pStatusChar   = nullptr;
static unsigned long  lastBleNotify     = 0;

// ── Shared NeoPixel ───────────────────────────────────────────────────────────
static Adafruit_NeoPixel pixel(NEOPIXEL_COUNT, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

static void pixelSet(uint8_t r, uint8_t g, uint8_t b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

// ═════════════════════════════════════════════════════════════════════════════
// BLE
// ═════════════════════════════════════════════════════════════════════════════

class BLEServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer*)    override { bleClientConnected = true; }
  void onDisconnect(BLEServer* s) override {
    bleClientConnected = false;
    s->startAdvertising();
  }
};

class CmdCharCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    String val = c->getValue().c_str();
    val.trim();
    portENTER_CRITICAL(&bleMux);
    bleCmd = val;
    portEXIT_CRITICAL(&bleMux);
  }
};

static void bleInit() {
  BLEDevice::init("MedSync");
  BLEServer*  server  = BLEDevice::createServer();
  server->setCallbacks(new BLEServerCB());

  BLEService* service = server->createService("4FAFC201-1FB5-459E-8FCC-C5C9C331914B");

  BLECharacteristic* cmdChar = service->createCharacteristic(
    "BEB5483E-36E1-4688-B7F5-EA07361B26A8",
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  cmdChar->setCallbacks(new CmdCharCB());

  pStatusChar = service->createCharacteristic(
    "BEB5483E-36E1-4688-B7F5-EA07361B26A9",
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pStatusChar->addDescriptor(new BLE2902());

  service->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID("4FAFC201-1FB5-459E-8FCC-C5C9C331914B");
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("[ble] advertising as 'MedSync'");
}

// ═════════════════════════════════════════════════════════════════════════════
// DISPLAY
// ═════════════════════════════════════════════════════════════════════════════

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA
);

const char* DAY_LABELS[8] = {
  "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY",
  "FRIDAY", "SATURDAY", "SUNDAY", "LOAD DAY"
};

enum DoseStatus { DOSE_PENDING, DOSE_DISPENSED, DOSE_MISSED };

static void displayInit() {
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

static void displayHome(int daySlot, const char* timeStr, DoseStatus status) {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_4x6_tr);
  char hdr[16];
  snprintf(hdr, sizeof(hdr), "NEXT: %02d:%02d", DOSE_HOUR, DOSE_MINUTE);
  u8g2.drawStr((128 - u8g2.getStrWidth(hdr)) / 2, 7, hdr);
  u8g2.drawHLine(0, 9, 128);

  const char* label = DAY_LABELS[daySlot < 8 ? daySlot : 0];
  if (strlen(label) <= 6) u8g2.setFont(u8g2_font_helvB18_tr);
  else                    u8g2.setFont(u8g2_font_helvB12_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(label)) / 2, 30, label);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(timeStr)) / 2, 42, timeStr);

  const char* badge;
  if      (status == DOSE_PENDING)   badge = "[ PENDING ]";
  else if (status == DOSE_DISPENSED) badge = "[ DISPENSED ]";
  else                               badge = "[ MISSED ]";

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(badge)) / 2, 52, badge);

  u8g2.drawHLine(0, 54, 128);
  int startX = (128 - 6 * 16) / 2;
  for (int i = 0; i < 7; i++) {
    int dx = startX + i * 16;
    if      (i < daySlot)  u8g2.drawDisc(dx, 61, 3);
    else if (i == daySlot) { u8g2.drawCircle(dx, 61, 3); u8g2.drawCircle(dx, 61, 1); }
    else                   u8g2.drawCircle(dx, 61, 1);
  }

  u8g2.sendBuffer();
}

static void displayLoadingMode() {
  u8g2.clearBuffer();

  u8g2.drawBox(0, 0, 128, 16);
  u8g2.drawBox(0, 48, 128, 16);

  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(10, 11, "> > > > > > > > > > >");
  u8g2.drawStr(10, 61, "< < < < < < < < < < <");
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_helvB14_tr);
  const char* msg = "LOADING";
  u8g2.drawStr((128 - u8g2.getStrWidth(msg)) / 2, 33, msg);
  u8g2.setFont(u8g2_font_6x10_tr);
  const char* sub = "MODE";
  u8g2.drawStr((128 - u8g2.getStrWidth(sub)) / 2, 45, sub);

  u8g2.sendBuffer();
}

static void displayMessage(const char* line1, const char* line2 = nullptr) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB12_tr);
  int y1 = line2 ? 26 : 34;
  u8g2.drawStr((128 - u8g2.getStrWidth(line1)) / 2, y1, line1);
  if (line2) {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr((128 - u8g2.getStrWidth(line2)) / 2, 44, line2);
  }
  u8g2.sendBuffer();
}

static void displaySplash() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB18_tr);
  const char* title = "MedSync";
  u8g2.drawStr((128 - u8g2.getStrWidth(title)) / 2, 34, title);
  u8g2.setFont(u8g2_font_6x10_tr);
  const char* sub = "by PeterCorp";
  u8g2.drawStr((128 - u8g2.getStrWidth(sub)) / 2, 50, sub);
  u8g2.sendBuffer();
}

// ═════════════════════════════════════════════════════════════════════════════
// STEPPER
// ═════════════════════════════════════════════════════════════════════════════

static Preferences prefs;
static int currentSlot = 0;
static int stepIdx     = 0;

static const int HALF_STEP[8][4] = {
  {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
  {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1}
};

static void setCoils(const int c[4]) {
  digitalWrite(PIN_IN1, c[0]);
  digitalWrite(PIN_IN2, c[1]);
  digitalWrite(PIN_IN3, c[2]);
  digitalWrite(PIN_IN4, c[3]);
}

static void deenergize() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
}

static void doHalfStep(int dir) {
  stepIdx = (stepIdx + dir + 8) % 8;
  setCoils(HALF_STEP[stepIdx]);
  delay(1);
}

static void stepperInit() {
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  deenergize();

  prefs.begin("stepper", true);
  currentSlot = prefs.getInt("slot", 0);
  prefs.end();

  Serial.printf("[stepper] restored slot %d from NVS\n", currentSlot);
}

static void stepperMoveTo(int targetSlot) {
  if (targetSlot == currentSlot) return;

  int slotsToMove = ((targetSlot - currentSlot) + TOTAL_SLOTS) % TOTAL_SLOTS;
  int steps       = slotsToMove * STEPS_PER_SLOT;

  Serial.printf("[stepper] moving %d slot(s) CW → slot %d\n", slotsToMove, targetSlot);

  for (int i = 0; i < steps; i++) doHalfStep(1);
  deenergize();

  currentSlot = targetSlot;

  prefs.begin("stepper", false);
  prefs.putInt("slot", currentSlot);
  prefs.end();
}

static void stepperMoveToLoad() {
  stepperMoveTo(SLOT_LOAD);
}

static void stepperReturnFromLoad(int daySlot) {
  stepperMoveTo(daySlot);
}

// ═════════════════════════════════════════════════════════════════════════════
// SCHEDULER
// ═════════════════════════════════════════════════════════════════════════════

static int wdayToSlot(int wday) {
  return (wday == 0) ? SLOT_SUNDAY : (wday - 1);
}

static void schedulerInit() {
  Serial.printf("[wifi] connecting to %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++attempts > 40) {
      Serial.println("\n[wifi] FAILED — continuing without network");
      return;
    }
  }
  Serial.printf("\n[wifi] connected, IP: %s\n", WiFi.localIP().toString().c_str());

  configTime(0, 0, NTP_SERVER);
  setenv("TZ", TIMEZONE_POSIX, 1);
  tzset();

  Serial.print("[ntp] syncing");
  time_t now = 0;
  struct tm ti;
  for (int i = 0; i < 20; i++) {
    time(&now);
    localtime_r(&now, &ti);
    if (ti.tm_year > 120) break;
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[ntp] time: %04d-%02d-%02d %02d:%02d:%02d\n",
    ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
    ti.tm_hour, ti.tm_min, ti.tm_sec);
}

static void schedulerGetTimeStr(char* buf, size_t len) {
  time_t now;
  struct tm ti;
  time(&now);
  localtime_r(&now, &ti);
  snprintf(buf, len, "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
}

static int schedulerCurrentDaySlot() {
  time_t now;
  struct tm ti;
  time(&now);
  localtime_r(&now, &ti);
  return wdayToSlot(ti.tm_wday);
}

static bool schedulerIsDoseWindow() {
  time_t now;
  struct tm ti;
  time(&now);
  localtime_r(&now, &ti);

  int currentMinutes = ti.tm_hour * 60 + ti.tm_min;
  int doseStart      = DOSE_HOUR  * 60 + DOSE_MINUTE;
  int doseEnd        = doseStart + DOSE_WINDOW_MIN;

  return (currentMinutes >= doseStart && currentMinutes < doseEnd);
}

static bool schedulerIsDoseWindowExpired() {
  time_t now;
  struct tm ti;
  time(&now);
  localtime_r(&now, &ti);

  int currentMinutes = ti.tm_hour * 60 + ti.tm_min;
  int doseEnd        = DOSE_HOUR  * 60 + DOSE_MINUTE + DOSE_WINDOW_MIN;

  return (currentMinutes >= doseEnd);
}

static time_t schedulerNow() {
  time_t now;
  time(&now);
  return now;
}

static long schedulerSecondsSince(time_t t) {
  return (long)(schedulerNow() - t);
}

// ═════════════════════════════════════════════════════════════════════════════
// ALERTS
// ═════════════════════════════════════════════════════════════════════════════

static void alertsInit() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pixel.begin();
  pixel.setBrightness(80);
  pixelSet(255, 235, 200);  // warm white — stays on permanently as illumination
}

static void alertsDoseDue() {
  static unsigned long lastToggle = 0;
  static bool alertState = false;

  if (millis() - lastToggle > 1000) {
    lastToggle = millis();
    alertState = !alertState;
    if (alertState) {
      digitalWrite(PIN_BUZZER, HIGH);
      delay(80);
      digitalWrite(PIN_BUZZER, LOW);
    }
  }
}

static void alertsMissed() {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(1000);
  digitalWrite(PIN_BUZZER, LOW);
}

static void alertsDispensing() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(PIN_BUZZER, HIGH); delay(80);
    digitalWrite(PIN_BUZZER, LOW);  delay(80);
  }
  delay(300);
}

// ═════════════════════════════════════════════════════════════════════════════
// STATUS LEDs  (TX = GPIO43, RX = GPIO44)
// ═════════════════════════════════════════════════════════════════════════════

static void statusLedInit() {
  pinMode(PIN_LED_TX, OUTPUT);
  pinMode(PIN_LED_RX, OUTPUT);
  digitalWrite(PIN_LED_TX, LOW);
  digitalWrite(PIN_LED_RX, LOW);
}

// ═════════════════════════════════════════════════════════════════════════════
// SUPABASE CLIENT
// ═════════════════════════════════════════════════════════════════════════════

static void addSupabaseHeaders(HTTPClient& http) {
  http.addHeader("apikey",        SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Content-Type",  "application/json");
}

static String supabaseInsertDoseEvent(const char* day, time_t scheduledTime, const char* status) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(SUPABASE_URL) + "/rest/v1/dose_events";
  http.begin(client, url);
  addSupabaseHeaders(http);
  http.addHeader("Prefer", "return=representation");

  struct tm ti;
  gmtime_r(&scheduledTime, &ti);
  char iso[30];
  strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &ti);

  JsonDocument doc;
  doc["day"]            = day;
  doc["scheduled_time"] = iso;
  doc["status"]         = status;
  String body;
  serializeJson(doc, body);

  int code = http.POST(body);
  String result = "";

  if (code == 201) {
    String resp = http.getString();
    JsonDocument respDoc;
    deserializeJson(respDoc, resp);
    if (respDoc.is<JsonArray>() && respDoc.as<JsonArray>().size() > 0) {
      result = respDoc[0]["id"].as<String>();
    }
    Serial.printf("[supabase] dose event inserted, id=%s\n", result.c_str());
  } else {
    Serial.printf("[supabase] insert failed, HTTP %d: %s\n", code, http.getString().c_str());
  }

  http.end();
  return result;
}

static bool supabaseUpdateDoseEvent(const String& id, const char* status,
                                    const char* photoUrl, int pillsDetected) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(SUPABASE_URL) + "/rest/v1/dose_events?id=eq." + id;
  http.begin(client, url);
  addSupabaseHeaders(http);

  JsonDocument doc;
  doc["status"] = status;
  if (photoUrl && strlen(photoUrl) > 0) doc["photo_url"] = photoUrl;
  if (pillsDetected == 0) doc["pills_detected"] = false;
  if (pillsDetected == 1) doc["pills_detected"] = true;
  if (strcmp(status, "dispensed") == 0) {
    time_t now;
    time(&now);
    struct tm ti;
    gmtime_r(&now, &ti);
    char iso[30];
    strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &ti);
    doc["dispensed_at"] = iso;
  }

  String body;
  serializeJson(doc, body);

  int code = http.PATCH(body);
  bool ok = (code == 200 || code == 204);
  if (!ok) Serial.printf("[supabase] update failed, HTTP %d\n", code);
  http.end();
  return ok;
}

static String supabaseUploadImage(const uint8_t* data, size_t len, const String& filename) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(SUPABASE_URL) + "/storage/v1/object/" +
               SUPABASE_BUCKET + "/" + filename;
  http.begin(client, url);
  http.addHeader("apikey",        SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Content-Type",  "image/jpeg");

  int code = http.POST((uint8_t*)data, len);

  String publicUrl = "";
  if (code == 200 || code == 201) {
    publicUrl = String(SUPABASE_URL) + "/storage/v1/object/public/" +
                SUPABASE_BUCKET + "/" + filename;
    Serial.printf("[supabase] image uploaded: %s\n", publicUrl.c_str());
  } else {
    Serial.printf("[supabase] image upload failed, HTTP %d: %s\n",
                  code, http.getString().c_str());
  }

  http.end();
  return publicUrl;
}

static String pendingCmdId = "";

static String supabaseCheckCommand() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(SUPABASE_URL) +
               "/rest/v1/commands?executed=eq.false&order=created_at.asc&limit=1";
  http.begin(client, url);
  http.addHeader("apikey",        SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);

  int code = http.GET();
  String cmd = "";

  if (code == 200) {
    String resp = http.getString();
    JsonDocument doc;
    deserializeJson(doc, resp);
    if (doc.is<JsonArray>() && doc.as<JsonArray>().size() > 0) {
      cmd          = doc[0]["command"].as<String>();
      pendingCmdId = doc[0]["id"].as<String>();
      Serial.printf("[supabase] command received: %s\n", cmd.c_str());
    }
  }

  http.end();
  return cmd;
}

static bool supabaseMarkCommandDone(const String& id) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(SUPABASE_URL) + "/rest/v1/commands?id=eq." + id;
  http.begin(client, url);
  addSupabaseHeaders(http);

  int code = http.PATCH("{\"executed\":true}");
  http.end();
  return (code == 200 || code == 204);
}

// ═════════════════════════════════════════════════════════════════════════════
// CAMERA
// ═════════════════════════════════════════════════════════════════════════════

static bool cameraInit() {
  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0       = CAM_PIN_Y2;
  cfg.pin_d1       = CAM_PIN_Y3;
  cfg.pin_d2       = CAM_PIN_Y4;
  cfg.pin_d3       = CAM_PIN_Y5;
  cfg.pin_d4       = CAM_PIN_Y6;
  cfg.pin_d5       = CAM_PIN_Y7;
  cfg.pin_d6       = CAM_PIN_Y8;
  cfg.pin_d7       = CAM_PIN_Y9;
  cfg.pin_xclk     = CAM_PIN_XCLK;
  cfg.pin_pclk     = CAM_PIN_PCLK;
  cfg.pin_vsync    = CAM_PIN_VSYNC;
  cfg.pin_href     = CAM_PIN_HREF;
  cfg.pin_sccb_sda = CAM_PIN_SIOD;
  cfg.pin_sccb_scl = CAM_PIN_SIOC;
  cfg.pin_pwdn     = CAM_PIN_PWDN;
  cfg.pin_reset    = CAM_PIN_RESET;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_RGB565;  // raw pixels for on-device CV
  cfg.frame_size   = FRAMESIZE_QVGA;   // 320x240 = 150KB, needs PSRAM
  cfg.fb_count     = 1;
  cfg.fb_location  = CAMERA_FB_IN_PSRAM;
  cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

  Serial.println("[camera] initialising...");
  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("[camera] init failed: %s\n", esp_err_to_name(err));
    return false;
  }

  for (int i = 0; i < 3; i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
    delay(100);
  }

  Serial.println("[camera] ready");
  return true;
}

// On-device pill detection using raw RGB565 frames.
// Iterates every pixel, converts to approximate saturation, and counts pixels
// that are neither white (cup interior) nor dark (shadows/background).
// A ratio above 1.5% of the frame being coloured indicates pills are present.
static bool detectPillsOnDevice(camera_fb_t* fb) {
  if (!fb || fb->format != PIXFORMAT_RGB565) return false;

  uint32_t colored = 0;
  uint32_t total   = (uint32_t)fb->width * fb->height;
  const uint8_t* p = fb->buf;

  for (uint32_t i = 0; i < total; i++, p += 2) {
    // OV2640 RGB565 byte order: high byte first
    uint16_t px = ((uint16_t)p[0] << 8) | p[1];
    uint8_t r = ((px >> 11) & 0x1F) << 3;
    uint8_t g = ((px >>  5) & 0x3F) << 2;
    uint8_t b = ( px        & 0x1F) << 3;

    uint8_t maxc = max(r, max(g, b));
    uint8_t minc = min(r, min(g, b));

    if (maxc < 40)  continue;   // too dark — shadow or background
    if (minc > 180) continue;   // near-white — cup interior
    uint8_t sat = (uint8_t)(((uint32_t)(maxc - minc) * 255) / maxc);
    if (sat > 50) colored++;
  }

  float ratio = (float)colored / (float)total;
  Serial.printf("[cv] coloured ratio: %.4f (%u/%u px)\n", ratio, colored, total);
  return ratio > 0.015f;
}

static bool cameraCaptureAndDetect() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[camera] frame capture failed");
    return false;
  }
  Serial.printf("[camera] captured %ux%u RGB565 frame (%u bytes)\n",
                fb->width, fb->height, fb->len);
  bool result = detectPillsOnDevice(fb);
  esp_camera_fb_return(fb);
  Serial.printf("[cv] result: %s\n", result ? "PILLS DETECTED" : "CUP EMPTY");
  return result;
}

// ═════════════════════════════════════════════════════════════════════════════
// STATE MACHINE
// ═════════════════════════════════════════════════════════════════════════════

enum State {
  STATE_IDLE,
  STATE_DOSE_DUE,
  STATE_DISPENSING,
  STATE_POST_DISPENSE,
  STATE_MISSED,
  STATE_LOADING
};

static State  state          = STATE_IDLE;
static String currentEventId = "";
static time_t dispenseTime   = 0;
static bool   doseDoneToday  = false;
static int    lastCheckedDay = -1;

static void statusLedUpdate() {
  static unsigned long lastBlink = 0;
  static bool blinkOn = false;
  if (millis() - lastBlink >= 600) {
    lastBlink = millis();
    blinkOn   = !blinkOn;
  }

  bool txOn = false, rxOn = false;

  switch (state) {
    case STATE_IDLE:                                    break;
    case STATE_DOSE_DUE:    txOn = blinkOn;             break;
    case STATE_DISPENSING:  txOn = true;                break;
    case STATE_POST_DISPENSE: txOn = blinkOn;           break;
    case STATE_MISSED:      rxOn = true;                break;
    case STATE_LOADING:     txOn = true; rxOn = blinkOn; break;
  }

  digitalWrite(PIN_LED_TX, txOn ? HIGH : LOW);
  digitalWrite(PIN_LED_RX, rxOn ? HIGH : LOW);
}

static void doDispense() {
  int daySlot = schedulerCurrentDaySlot();

  alertsDispensing();

  displayMessage("Dispensing...", DAY_LABELS[daySlot]);
  stepperMoveTo(daySlot);

  delay(500);  // let pills settle before capture

  bool pillsPresent = cameraCaptureAndDetect();
  time_t now = schedulerNow();

  char dayName[16];
  strncpy(dayName, DAY_LABELS[daySlot], sizeof(dayName));

  if (currentEventId.length() == 0) {
    currentEventId = supabaseInsertDoseEvent(dayName, now, "dispensed");
  }
  if (currentEventId.length() > 0) {
    supabaseUpdateDoseEvent(currentEventId, "dispensed", "", pillsPresent ? 1 : 0);
  }

  dispenseTime  = now;
  doseDoneToday = true;
  state         = STATE_POST_DISPENSE;

  Serial.printf("[main] dispensed — on-device CV: %s\n",
                pillsPresent ? "pills detected" : "cup empty");
}

static void doMissed() {
  alertsMissed();
  if (currentEventId.length() > 0) {
    supabaseUpdateDoseEvent(currentEventId, "missed", "", -1);
  }
  state         = STATE_MISSED;
  doseDoneToday = true;
  Serial.println("[main] dose marked MISSED");
}

// ═════════════════════════════════════════════════════════════════════════════
// SETUP & LOOP
// ═════════════════════════════════════════════════════════════════════════════

static unsigned long lastDisplayUpdate = 0;
static unsigned long lastCmdPoll       = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== MedSync booting ===");

  alertsInit(); // inits NeoPixel + buzzer, turns both off

  stepperInit();
  displayInit();
  displaySplash();

  // Startup 360° carousel sweep — runs while the splash is on screen
  for (int i = 0; i < TOTAL_SLOTS * STEPS_PER_SLOT; i++) doHalfStep(1);
  deenergize();  // currentSlot unchanged — full circle returns to start

  schedulerInit();
  bleInit();

  if (!cameraInit()) {
    displayMessage("CAM ERROR", "Check wiring");
    while (true) delay(1000);
  }

  // Re-init NeoPixel after camera — esp_camera_init can disturb RMT state
  pixel.begin();
  pixel.setBrightness(80);
  pixelSet(255, 235, 200);

  displayMessage("MedSync", "Ready");
  delay(1000);

  Serial.println("[main] setup complete — status LEDs taking over GPIO43/44");
  statusLedInit();  // GPIO43/44 → LED outputs; Serial goes silent after this
}

void loop() {
  unsigned long now_ms = millis();

  if (now_ms - lastDisplayUpdate > DISPLAY_REFRESH_MS) {
    lastDisplayUpdate = now_ms;
    char tStr[10];
    schedulerGetTimeStr(tStr, sizeof(tStr));
    int daySlot = schedulerCurrentDaySlot();

    DoseStatus dispStatus = DOSE_PENDING;
    if      (state == STATE_POST_DISPENSE || doseDoneToday) dispStatus = DOSE_DISPENSED;
    else if (state == STATE_MISSED)                         dispStatus = DOSE_MISSED;

    if (state == STATE_LOADING) {
      displayLoadingMode();
    } else {
      displayHome(daySlot, tStr, dispStatus);
    }
  }

  int todaySlot = schedulerCurrentDaySlot();
  if (todaySlot != lastCheckedDay) {
    lastCheckedDay  = todaySlot;
    doseDoneToday   = false;
    currentEventId  = "";
    dispenseTime    = 0;
    if (state == STATE_MISSED || state == STATE_POST_DISPENSE) {
      state = STATE_IDLE;
    }
    Serial.printf("[main] new day — slot %d\n", todaySlot);
  }

  if (now_ms - lastCmdPoll > CMD_POLL_MS) {
    lastCmdPoll = now_ms;
    String cmd = supabaseCheckCommand();

    if (cmd == "dispense" && state == STATE_DOSE_DUE && !doseDoneToday) {
      supabaseMarkCommandDone(pendingCmdId);
      state = STATE_DISPENSING;
    } else if (cmd == "load" && state != STATE_LOADING) {
      supabaseMarkCommandDone(pendingCmdId);
      stepperMoveToLoad();
      state = STATE_LOADING;
    } else if (cmd == "unload" && state == STATE_LOADING) {
      supabaseMarkCommandDone(pendingCmdId);
      stepperReturnFromLoad(schedulerCurrentDaySlot());
      state = STATE_IDLE;
    }
  }

  // BLE command handling
  portENTER_CRITICAL(&bleMux);
  String bleSnapshot = bleCmd;
  bleCmd = "";
  portEXIT_CRITICAL(&bleMux);
  if (bleSnapshot.length() > 0) {
    String cmd = bleSnapshot;
    if (cmd == "DISPENSE" && state != STATE_LOADING) {
      // Advance one slot CW, skipping SLOT_LOAD (7).
      int nextSlot = (currentSlot + 1) % TOTAL_SLOTS;
      if (nextSlot == SLOT_LOAD) nextSlot = (nextSlot + 1) % TOTAL_SLOTS;
      alertsDispensing();
      stepperMoveTo(nextSlot);
    } else if (cmd == "LOAD" && state != STATE_LOADING) {
      stepperMoveToLoad();
      state = STATE_LOADING;
    } else if (cmd == "UNLOAD" && state == STATE_LOADING) {
      stepperReturnFromLoad(schedulerCurrentDaySlot());
      state = STATE_IDLE;
    } else if (cmd == "CALIBRATE") {
      // Physically align the carousel so Monday's compartment is over the
      // dispense hole, then send CALIBRATE. Writes slot 0 to NVS without
      // moving the motor — all subsequent GOTO/DISPENSE moves are relative
      // to this home position.
      currentSlot = SLOT_MONDAY;
      prefs.begin("stepper", false);
      prefs.putInt("slot", currentSlot);
      prefs.end();
      Serial.println("[stepper] calibrated — current position set to MONDAY (slot 0)");
    } else if (cmd.startsWith("GOTO:")) {
      int target = cmd.substring(5).toInt();
      if (target >= 0 && target < TOTAL_SLOTS) {
        alertsDispensing();
        stepperMoveTo(target);
        Serial.printf("[stepper] GOTO → slot %d (%s)\n", target, DAY_LABELS[target]);
      }
    }
  }


  // BLE status notification
  if (bleClientConnected && pStatusChar && (now_ms - lastBleNotify > 3000)) {
    lastBleNotify = now_ms;
    char tStr[10];
    schedulerGetTimeStr(tStr, sizeof(tStr));
    int slot = schedulerCurrentDaySlot();
    const char* statusStr =
      (state == STATE_POST_DISPENSE) ? "dispensed" :
      (state == STATE_MISSED)        ? "missed"    :
      (state == STATE_LOADING)       ? "loading"   : "pending";
    char json[128];
    snprintf(json, sizeof(json),
      "{\"slot\":%d,\"status\":\"%s\",\"time\":\"%s\",\"day\":\"%s\"}",
      slot, statusStr, tStr, DAY_LABELS[slot < 8 ? slot : 0]);
    pStatusChar->setValue(json);
    pStatusChar->notify();
  }

  switch (state) {

    case STATE_IDLE:
      if (schedulerIsDoseWindow() && !doseDoneToday) {
        Serial.println("[main] dose window open");
        char dayName[16];
        strncpy(dayName, DAY_LABELS[schedulerCurrentDaySlot()], sizeof(dayName));
        currentEventId = supabaseInsertDoseEvent(dayName, schedulerNow(), "pending");
        state = STATE_DOSE_DUE;
      }
      break;

    case STATE_DOSE_DUE:
      alertsDoseDue();
      if (schedulerIsDoseWindowExpired()) {
        doMissed();
      }
      break;

    case STATE_DISPENSING:
      doDispense();
      break;

    case STATE_POST_DISPENSE:
      if (dispenseTime > 0 &&
          schedulerSecondsSince(dispenseTime) > (VERIFY_WINDOW_MIN * 60)) {
        alertsMissed();
        Serial.println("[main] 30 min post-dispense — pill may not be taken");
        state = STATE_IDLE;
      }
      break;

    case STATE_MISSED:
      break;

    case STATE_LOADING:
      break;
  }

  statusLedUpdate();
  delay(10);
}
