#pragma once

// Initialises buzzer GPIO and NeoPixel. Turns everything off.
void alertsInit();

// Dose-due alert: pulses buzzer + flashes NeoPixel amber.
// Non-blocking — call repeatedly in the main loop while dose is pending.
void alertsDoseDue();

// Missed-dose alert: single long buzzer tone + solid red NeoPixel.
void alertsMissed();

// Dispensing indicator: brief double-beep + green flash.
void alertsDispensing();

// Turns off buzzer and NeoPixel completely.
void alertsOff();
