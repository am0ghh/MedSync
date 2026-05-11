#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// MedSync — Hardware Configuration
// Verify every GPIO against the Freenove ESP32-S3-WROOM pinout before flashing.
// ─────────────────────────────────────────────────────────────────────────────

// ── Stepper Motor (ULN2003 + 28BYJ-48) ───────────────────────────────────────
#define PIN_IN1          1
#define PIN_IN2          2
#define PIN_IN3         42
#define PIN_IN4         41
#define STEPS_PER_SLOT 512   // half-steps for 45° rotation
#define TOTAL_SLOTS      8   // 7 days + 1 load slot

// ── OLED Display (SSD1306 0.96" I2C) ─────────────────────────────────────────
#define PIN_OLED_SDA    39
#define PIN_OLED_SCL    40

// ── Camera (OV2640 built into Freenove ESP32-S3-WROOM) ───────────────────────
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD     4
#define CAM_PIN_SIOC     5
#define CAM_PIN_Y9      16
#define CAM_PIN_Y8      17
#define CAM_PIN_Y7      18
#define CAM_PIN_Y6      12
#define CAM_PIN_Y5      10
#define CAM_PIN_Y4       8
#define CAM_PIN_Y3       9
#define CAM_PIN_Y2      11
#define CAM_PIN_VSYNC    6
#define CAM_PIN_HREF     7
#define CAM_PIN_PCLK    13

// ── Alerts ────────────────────────────────────────────────────────────────────
#define PIN_BUZZER      38
#define PIN_NEOPIXEL    48   // onboard WS2812B RGB LED
#define NEOPIXEL_COUNT   1

// ── Status LEDs (onboard TX / RX indicator LEDs) ──────────────────────────────
#define PIN_LED_TX      43   // TX indicator LED — GPIO43 / UART0 TX
#define PIN_LED_RX      44   // RX indicator LED — GPIO44 / UART0 RX
// statusLedInit() reconfigures these pins as GPIO outputs, which disconnects
// UART0 and silences Serial.print(). Boot messages still appear before setup()
// calls statusLedInit(). Comment out that call to restore serial debugging.

// ── Carousel Slot Layout ──────────────────────────────────────────────────────
// Physical order matches day-of-week (Monday first):
//   0=Monday  1=Tuesday  2=Wednesday  3=Thursday
//   4=Friday  5=Saturday 6=Sunday     7=LOAD (blocked slot)
#define SLOT_MONDAY      0
#define SLOT_TUESDAY     1
#define SLOT_WEDNESDAY   2
#define SLOT_THURSDAY    3
#define SLOT_FRIDAY      4
#define SLOT_SATURDAY    5
#define SLOT_SUNDAY      6
#define SLOT_LOAD        7

// ── Dose Schedule ─────────────────────────────────────────────────────────────
#define DOSE_HOUR        9   // 9:00 AM — change to match patient's schedule
#define DOSE_MINUTE      0
#define DOSE_WINDOW_MIN 60   // minutes until dose is marked missed if not taken
#define VERIFY_WINDOW_MIN 30 // minutes after dispense to wait for CV confirmation

// ── NTP / Time ────────────────────────────────────────────────────────────────
#define NTP_SERVER      "pool.ntp.org"
// POSIX timezone string — change to match your location:
// Eastern:  "EST5EDT,M3.2.0,M11.1.0"
// Central:  "CST6CDT,M3.2.0,M11.1.0"
// Mountain: "MST7MDT,M3.2.0,M11.1.0"
// Pacific:  "PST8PDT,M3.2.0,M11.1.0"
#define TIMEZONE_POSIX  "EST5EDT,M3.2.0,M11.1.0"

// ── Polling Intervals ─────────────────────────────────────────────────────────
#define CMD_POLL_MS     5000   // how often to check Supabase for remote commands
#define DISPLAY_REFRESH_MS 1000 // how often to redraw the OLED clock
