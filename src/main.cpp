#include <Arduino.h>
#include "config.h"
#include "stepper.h"
#include "display.h"
#include "scheduler.h"
#include "alerts.h"
#include "supabase_client.h"
#include "camera_handler.h"
#include <Adafruit_NeoPixel.h>

// ── State Machine ─────────────────────────────────────────────────────────────
enum State {
  STATE_IDLE,           // normal — waiting for next dose window
  STATE_DOSE_DUE,       // dose window is open — alert active
  STATE_DISPENSING,     // motor moving, camera firing
  STATE_POST_DISPENSE,  // waiting for CV result (30 min window)
  STATE_MISSED,         // dose window expired without dispensing
  STATE_LOADING         // carousel locked in load slot
};

static State       state           = STATE_IDLE;
static String      currentEventId  = "";   // Supabase row UUID for today's dose
static time_t      dispenseTime    = 0;    // Unix timestamp of last dispense
static bool        doseDoneToday   = false;
static int         lastCheckedDay  = -1;   // prevents re-triggering on same day

// NeoPixel for camera fill-light and alerts (shared, managed here).
static Adafruit_NeoPixel pixel(NEOPIXEL_COUNT, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

static void pixelSet(uint8_t r, uint8_t g, uint8_t b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

// ── Timers ────────────────────────────────────────────────────────────────────
static unsigned long lastDisplayUpdate = 0;
static unsigned long lastCmdPoll       = 0;

// ── Dispense sequence ─────────────────────────────────────────────────────────
static void doDispense() {
  int daySlot = schedulerCurrentDaySlot();

  // Brief green flash + double-beep to signal start.
  alertsDispensing();

  // Move carousel to today's slot so pills drop into the cup.
  displayMessage("Dispensing...", DAY_LABELS[daySlot]);
  stepperMoveTo(daySlot);

  // Flash white fill light and capture photo.
  pixelSet(255, 255, 255);
  delay(200); // let lighting stabilise

  // Build a unique filename: medsync_YYYYMMDD_HHMMSS.jpg
  char filename[40];
  time_t now = schedulerNow();
  struct tm ti;
  localtime_r(&now, &ti);
  snprintf(filename, sizeof(filename), "medsync_%04d%02d%02d_%02d%02d%02d.jpg",
           ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
           ti.tm_hour, ti.tm_min, ti.tm_sec);

  String photoUrl = cameraCaptureAndUpload(String(filename));
  pixelSet(0, 0, 0); // fill light off

  // Insert or update dose event in Supabase.
  if (currentEventId.length() == 0) {
    // Create the record if it wasn't created when the alarm fired.
    char dayName[16];
    strncpy(dayName, DAY_LABELS[daySlot], sizeof(dayName));
    currentEventId = supabaseInsertDoseEvent(dayName, now, "dispensed");
  }
  if (currentEventId.length() > 0) {
    supabaseUpdateDoseEvent(currentEventId, "dispensed", photoUrl.c_str(), -1);
  }

  dispenseTime  = now;
  doseDoneToday = true;
  state         = STATE_POST_DISPENSE;

  Serial.printf("[main] dispensed — photo: %s\n", photoUrl.c_str());
}

// ── Missed dose ───────────────────────────────────────────────────────────────
static void doMissed() {
  alertsMissed();
  if (currentEventId.length() > 0) {
    supabaseUpdateDoseEvent(currentEventId, "missed", "", -1);
  }
  state         = STATE_MISSED;
  doseDoneToday = true; // lock out further attempts today
  Serial.println("[main] dose marked MISSED");
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== MedSync booting ===");

  // NeoPixel: initialise first so it doesn't float and glow randomly.
  pixel.begin();
  pixel.setBrightness(80);
  pixelSet(0, 0, 0);

  alertsInit();
  stepperInit();
  displayInit();

  displayMessage("MedSync", "Starting...");

  schedulerInit(); // connects WiFi + syncs NTP

  if (!cameraInit()) {
    displayMessage("CAM ERROR", "Check wiring");
    while (true) delay(1000);
  }

  displayMessage("MedSync", "Ready");
  delay(1000);

  Serial.println("[main] setup complete");
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  unsigned long now_ms = millis();

  // ── Refresh OLED clock every second ────────────────────────────────────────
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

  // ── Reset daily flags at midnight ──────────────────────────────────────────
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

  // ── Poll Supabase for remote commands ──────────────────────────────────────
  if (now_ms - lastCmdPoll > CMD_POLL_MS) {
    lastCmdPoll = now_ms;
    String cmd = supabaseCheckCommand();

    if (cmd == "dispense" && state == STATE_DOSE_DUE && !doseDoneToday) {
      supabaseMarkCommandDone(cmd);
      state = STATE_DISPENSING;
    } else if (cmd == "load" && state != STATE_LOADING) {
      supabaseMarkCommandDone(cmd);
      stepperMoveToLoad();
      state = STATE_LOADING;
    } else if (cmd == "unload" && state == STATE_LOADING) {
      supabaseMarkCommandDone(cmd);
      stepperReturnFromLoad(schedulerCurrentDaySlot());
      state = STATE_IDLE;
    }
  }

  // ── State machine ───────────────────────────────────────────────────────────
  switch (state) {

    case STATE_IDLE:
      // Enter dose-due state when the window opens (and dose not yet done today).
      if (schedulerIsDoseWindow() && !doseDoneToday) {
        Serial.println("[main] dose window open");
        // Create a pending record in Supabase when the alarm fires.
        char dayName[16];
        strncpy(dayName, DAY_LABELS[schedulerCurrentDaySlot()], sizeof(dayName));
        currentEventId = supabaseInsertDoseEvent(dayName, schedulerNow(), "pending");
        state = STATE_DOSE_DUE;
      }
      break;

    case STATE_DOSE_DUE:
      alertsDoseDue(); // non-blocking pulse

      // Auto-dispense after window expires (no button in current build).
      // Remove this block once a physical button is added.
      if (schedulerIsDoseWindowExpired()) {
        doMissed();
      }
      break;

    case STATE_DISPENSING:
      doDispense();
      break;

    case STATE_POST_DISPENSE:
      // If 30 minutes have passed since dispensing and no CV result yet,
      // fire a "still present" alert. The Python processor writes pills_detected;
      // we just flag it here — the dashboard will show the unresolved state.
      if (dispenseTime > 0 &&
          schedulerSecondsSince(dispenseTime) > (VERIFY_WINDOW_MIN * 60)) {
        alertsMissed(); // re-use missed alert tone for "pill not taken"
        Serial.println("[main] 30 min post-dispense — pill may not be taken");
        state = STATE_IDLE;
      }
      break;

    case STATE_MISSED:
      // Nothing to do — dashboard shows missed status. Reset at midnight.
      break;

    case STATE_LOADING:
      // Carousel is locked in load slot — ignore all dose logic.
      break;
  }

  delay(10); // yield to background tasks (WiFi, httpd)
}

