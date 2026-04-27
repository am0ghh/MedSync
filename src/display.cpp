#include <Arduino.h>
#include <U8g2lib.h>
#include "display.h"
#include "config.h"

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA
);

// Day labels matching carousel slot indices (0=Monday … 6=Sunday, 7=LOAD)
// Non-static so main.cpp can access via the extern in display.h
const char* DAY_LABELS[8] = {
  "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY",
  "FRIDAY", "SATURDAY", "SUNDAY", "LOAD DAY"
};

void displayInit() {
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

// ── Home screen ───────────────────────────────────────────────────────────────
// Layout:
//   [ PETERCORP ]       ← tiny header
//   ─────────────────
//   MONDAY              ← large day name
//   09:00               ← current time
//   [ PENDING ]         ← dose status badge
//   • • • ○ ○ ○ ○      ← weekly progress dots
void displayHome(int daySlot, const char* timeStr, DoseStatus status) {
  u8g2.clearBuffer();

  // Header
  u8g2.setFont(u8g2_font_4x6_tr);
  const char* hdr = "[ MEDSYNC ]";
  u8g2.drawStr((128 - u8g2.getStrWidth(hdr)) / 2, 7, hdr);
  u8g2.drawHLine(0, 9, 128);

  // Day name — smaller font for long names
  const char* label = DAY_LABELS[daySlot < 8 ? daySlot : 0];
  if (strlen(label) <= 6) u8g2.setFont(u8g2_font_helvB18_tr);
  else                    u8g2.setFont(u8g2_font_helvB12_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(label)) / 2, 30, label);

  // Time
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(timeStr)) / 2, 42, timeStr);

  // Status badge
  const char* badge;
  if      (status == DOSE_PENDING)   badge = "[ PENDING ]";
  else if (status == DOSE_DISPENSED) badge = "[ DISPENSED ]";
  else                               badge = "[ MISSED ]";

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(badge)) / 2, 52, badge);

  // Weekly progress dots (7 day slots, skip slot 7 = LOAD)
  u8g2.drawHLine(0, 54, 128);
  int startX = (128 - 6 * 16) / 2;
  for (int i = 0; i < 7; i++) {
    int dx = startX + i * 16;
    if      (i < daySlot)  u8g2.drawDisc(dx, 61, 3);         // past: filled
    else if (i == daySlot) { u8g2.drawCircle(dx, 61, 3); u8g2.drawCircle(dx, 61, 1); } // current: double ring
    else                   u8g2.drawCircle(dx, 61, 1);        // future: tiny ring
  }

  u8g2.sendBuffer();
}

// ── Loading Mode screen ───────────────────────────────────────────────────────
void displayLoadingMode() {
  u8g2.clearBuffer();

  // Bold top/bottom bars
  u8g2.drawBox(0, 0, 128, 16);
  u8g2.drawBox(0, 48, 128, 16);

  // Inverted text in bars
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(10, 11, "> > > > > > > > > > >");
  u8g2.drawStr(10, 61, "< < < < < < < < < < <");
  u8g2.setDrawColor(1);

  // Main label
  u8g2.setFont(u8g2_font_helvB14_tr);
  const char* msg = "LOADING";
  u8g2.drawStr((128 - u8g2.getStrWidth(msg)) / 2, 33, msg);
  u8g2.setFont(u8g2_font_6x10_tr);
  const char* sub = "MODE";
  u8g2.drawStr((128 - u8g2.getStrWidth(sub)) / 2, 45, sub);

  u8g2.sendBuffer();
}

// ── Transient message screen ──────────────────────────────────────────────────
void displayMessage(const char* line1, const char* line2) {
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
