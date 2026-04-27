#include <Arduino.h>
#include <WiFi.h>
#include "scheduler.h"
#include "config.h"
#include "secrets.h"

// Converts tm_wday (0=Sun…6=Sat) to carousel slot (0=Mon…6=Sun).
static int wdayToSlot(int wday) {
  // tm_wday: 0=Sun, 1=Mon, … 6=Sat
  // Slot:    6=Sun, 0=Mon, … 5=Sat
  return (wday == 0) ? SLOT_SUNDAY : (wday - 1);
}

void schedulerInit() {
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

  // Sync time via NTP. configTime sets the system clock; tzset applies DST rules.
  configTime(0, 0, NTP_SERVER);
  setenv("TZ", TIMEZONE_POSIX, 1);
  tzset();

  // Wait for a valid time (year > 2020 confirms NTP succeeded).
  Serial.print("[ntp] syncing");
  time_t now = 0;
  struct tm ti;
  for (int i = 0; i < 20; i++) {
    time(&now);
    localtime_r(&now, &ti);
    if (ti.tm_year > 120) break; // year 2021+
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[ntp] time: %04d-%02d-%02d %02d:%02d:%02d\n",
    ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
    ti.tm_hour, ti.tm_min, ti.tm_sec);
}

void schedulerGetTimeStr(char* buf, size_t len) {
  time_t now;
  struct tm ti;
  time(&now);
  localtime_r(&now, &ti);
  snprintf(buf, len, "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
}

int schedulerCurrentDaySlot() {
  time_t now;
  struct tm ti;
  time(&now);
  localtime_r(&now, &ti);
  return wdayToSlot(ti.tm_wday);
}

bool schedulerIsDoseWindow() {
  time_t now;
  struct tm ti;
  time(&now);
  localtime_r(&now, &ti);

  int currentMinutes  = ti.tm_hour * 60 + ti.tm_min;
  int doseStart       = DOSE_HOUR  * 60 + DOSE_MINUTE;
  int doseEnd         = doseStart + DOSE_WINDOW_MIN;

  return (currentMinutes >= doseStart && currentMinutes < doseEnd);
}

bool schedulerIsDoseWindowExpired() {
  time_t now;
  struct tm ti;
  time(&now);
  localtime_r(&now, &ti);

  int currentMinutes = ti.tm_hour * 60 + ti.tm_min;
  int doseEnd        = DOSE_HOUR  * 60 + DOSE_MINUTE + DOSE_WINDOW_MIN;

  return (currentMinutes >= doseEnd);
}

long schedulerSecondsSince(time_t t) {
  return (long)(schedulerNow() - t);
}

time_t schedulerNow() {
  time_t now;
  time(&now);
  return now;
}
