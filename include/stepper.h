#pragma once
#include <Arduino.h>

// Initialises GPIO pins and restores last known slot from NVS.
void stepperInit();

// Moves the carousel to the given slot (0–7). Always moves forward (CW).
// Persists the new position to NVS so it survives reboots.
void stepperMoveTo(int targetSlot);

// Returns the slot index currently over the dispensing hole.
int  stepperCurrentSlot();

// Convenience: rotate to the LOAD (blocked) slot.
void stepperMoveToLoad();

// Convenience: rotate back to a specific day slot after leaving load mode.
void stepperReturnFromLoad(int daySlot);
