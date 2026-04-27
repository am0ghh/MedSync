#include <Arduino.h>
#include <Preferences.h>
#include "stepper.h"
#include "config.h"

static Preferences prefs;
static int currentSlot = 0;
static int stepIdx     = 0;

// Half-step sequence: [IN1, IN2, IN3, IN4]
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

// Releases all coils to prevent motor overheating when idle.
static void deenergize() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
}

// Advances one half-step in the given direction (+1 CW, -1 CCW).
static void doHalfStep(int dir) {
  stepIdx = (stepIdx + dir + 8) % 8;
  setCoils(HALF_STEP[stepIdx]);
  delay(1); // 1 ms per half-step — near max speed for 28BYJ-48
}

void stepperInit() {
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  deenergize();

  // Restore last known carousel position from NVS.
  // On first boot this returns 0 (SLOT_MONDAY), which assumes the user
  // has manually positioned the carousel so Monday's slot is over the hole.
  prefs.begin("stepper", true); // read-only
  currentSlot = prefs.getInt("slot", 0);
  prefs.end();

  Serial.printf("[stepper] restored slot %d from NVS\n", currentSlot);
}

void stepperMoveTo(int targetSlot) {
  if (targetSlot == currentSlot) return;

  // Always rotate clockwise (forward). Calculate how many slots to advance.
  int slotsToMove = ((targetSlot - currentSlot) + TOTAL_SLOTS) % TOTAL_SLOTS;
  int steps       = slotsToMove * STEPS_PER_SLOT;

  Serial.printf("[stepper] moving %d slot(s) CW → slot %d\n", slotsToMove, targetSlot);

  for (int i = 0; i < steps; i++) doHalfStep(1);
  deenergize();

  currentSlot = targetSlot;

  // Persist position so it survives a reboot.
  prefs.begin("stepper", false);
  prefs.putInt("slot", currentSlot);
  prefs.end();
}

int stepperCurrentSlot() {
  return currentSlot;
}

void stepperMoveToLoad() {
  stepperMoveTo(SLOT_LOAD);
}

void stepperReturnFromLoad(int daySlot) {
  stepperMoveTo(daySlot);
}
