#pragma once
#include <Arduino.h>

// Day labels indexed by carousel slot (0=Monday … 6=Sunday, 7=LOAD DAY).
// Defined in display.cpp; extern here so main.cpp can also use them.
extern const char* DAY_LABELS[8];

// Dose status values used by the display.
enum DoseStatus {
  DOSE_PENDING,
  DOSE_DISPENSED,
  DOSE_MISSED
};

// Initialises the U8g2 OLED driver.
void displayInit();

// Renders the standard home screen:
//   - Current day name (large font)
//   - Current time HH:MM
//   - Dose status badge (PENDING / DISPENSED / MISSED)
//   - Progress dots for the week
void displayHome(int daySlot, const char* timeStr, DoseStatus status);

// Renders the full-screen LOADING MODE screen.
void displayLoadingMode();

// Renders a transient message centred on screen (e.g. "Dispensing...").
void displayMessage(const char* line1, const char* line2 = nullptr);
