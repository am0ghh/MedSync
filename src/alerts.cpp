#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "alerts.h"
#include "config.h"

static Adafruit_NeoPixel pixel(NEOPIXEL_COUNT, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Helper: set the NeoPixel to an RGB colour.
static void setPixel(uint8_t r, uint8_t g, uint8_t b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

void alertsInit() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pixel.begin();
  pixel.setBrightness(80); // 0–255; 80 is visible but not blinding
  setPixel(0, 0, 0);       // off
}

// Dose-due: non-blocking amber pulse + short beep every ~2 seconds.
// Call this in a loop — it uses millis() so it won't block the main loop.
void alertsDoseDue() {
  static unsigned long lastToggle = 0;
  static bool state = false;

  if (millis() - lastToggle > 1000) {
    lastToggle = millis();
    state = !state;
    if (state) {
      setPixel(255, 120, 0); // amber
      digitalWrite(PIN_BUZZER, HIGH);
      delay(80);
      digitalWrite(PIN_BUZZER, LOW);
    } else {
      setPixel(0, 0, 0);
    }
  }
}

// Missed dose: long tone + solid red.
void alertsMissed() {
  setPixel(255, 0, 0);
  digitalWrite(PIN_BUZZER, HIGH);
  delay(1000);
  digitalWrite(PIN_BUZZER, LOW);
}

// Dispensing: double-beep + green flash.
void alertsDispensing() {
  setPixel(0, 255, 0);
  for (int i = 0; i < 2; i++) {
    digitalWrite(PIN_BUZZER, HIGH); delay(80);
    digitalWrite(PIN_BUZZER, LOW);  delay(80);
  }
  delay(300);
  setPixel(0, 0, 0);
}

// Off: silence + pixel off.
void alertsOff() {
  digitalWrite(PIN_BUZZER, LOW);
  setPixel(0, 0, 0);
}
