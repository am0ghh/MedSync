#pragma once
#include <Arduino.h>

// Connects to WiFi and synchronises time via NTP.
// Blocks until both succeed. Shows progress on Serial.
void schedulerInit();

// Returns the current time as a formatted string "HH:MM:SS".
// Buffer must be at least 9 chars.
void schedulerGetTimeStr(char* buf, size_t len);

// Returns the current day-of-week as a carousel slot index:
//   Monday=0 … Sunday=6  (matches SLOT_* defines in config.h)
int schedulerCurrentDaySlot();

// Returns true if the current time is within the dose window
// (between DOSE_HOUR:DOSE_MINUTE and DOSE_HOUR:DOSE_MINUTE + DOSE_WINDOW_MIN).
bool schedulerIsDoseWindow();

// Returns true if the dose window has fully expired without dispensing.
bool schedulerIsDoseWindowExpired();

// Returns elapsed seconds since the given Unix timestamp.
long schedulerSecondsSince(time_t t);

// Returns the current Unix timestamp.
time_t schedulerNow();
