# MedSync Dev Session Notes — May 10-11 2026

This file summarizes all changes made during this session for reference in future conversations.

---

## What Was in the Repo at the Start

A multi-agent framework had already made changes before this session:
- Consolidated all firmware from separate `.cpp` module files into a single `src/main.cpp`
- Created the iOS SwiftUI app scaffolding (`ios/MedSyncApp/`)
- `SupabaseService.swift` used hardcoded mock data (intentional — demo only, no real Supabase calls from iOS)
- KiCad schematic and PCB layout files existed in the repo

---

## Changes Made This Session

### 1. BLE Wireless Dispensing Fixed

**Problem:** The BLE `DISPENSE` command was gated behind `state == STATE_DOSE_DUE && !doseDoneToday`, meaning it only worked during the 9 AM dose window and only once per day. Tapping Dispense in the app did nothing outside that window.

**Fix (`src/main.cpp`):** Rewrote the BLE DISPENSE handler to bypass `doDispense()` entirely. It now:
- Works from any state except `STATE_LOADING`
- Advances `currentSlot` by exactly one slot clockwise, skipping `SLOT_LOAD` (slot 7)
- Calls `alertsDispensing()` then `stepperMoveTo(nextSlot)` directly — same pattern as LOAD/UNLOAD
- Does not touch `doseDoneToday`, does not block, does not call Supabase

### 2. BLE Slot Navigation and Calibration

**Added to firmware (`src/main.cpp`):**
- `GOTO:N` — moves motor directly to slot N (0=Mon through 7=Load)
- `CALIBRATE` — physically position Monday over dispense hole, then send this. Writes `currentSlot = 0` to NVS without moving motor. All subsequent moves are relative to this home.

**Physical assembly workflow:**
1. Power on ESP32, connect iOS app over BLE
2. Hand-rotate carousel until Monday is over dispense hole
3. Tap Calibrate Home in app
4. Use day buttons (Mon-Sun) to verify each slot

### 3. iOS App — BLE Status Display Fix

**Problem:** The BLE status JSON (`{"slot":3,"status":"pending","time":"09:15:30","day":"THURSDAY"}`) was being rendered as raw text in the Controls tab.

**Fix (`ios/MedSyncApp/CommandPanelView.swift`):** Added `formattedDeviceStatus()` helper that parses the JSON and displays:
```
THURSDAY  •  09:15:30
Status: Pending
```
Falls back to raw string if parsing fails. Key fix: cast to `[String: Any]` not `[String: String]` because `slot` is an integer in the JSON.

### 4. iOS App — Slot Navigation UI

**Added to `CommandPanelView.swift`:**
- 8-button grid (Mon, Tue, Wed, Thu, Fri, Sat, Sun, Load) — each sends `GOTO:N` directly over BLE
- Calibrate Home button — sends `CALIBRATE`
- All buttons disabled when BLE is disconnected

### 5. OLED Next Dose Time

**Changed (`src/main.cpp` — `displayHome()`):** Replaced the `"[ MEDSYNC ]"` header at the top of the OLED with `"NEXT: 09:00"` formatted from `DOSE_HOUR` and `DOSE_MINUTE` constants. Same font and position, no layout shift.

### 6. Push Notifications

**Created `ios/MedSyncApp/NotificationManager.swift`** — singleton using `UNUserNotificationCenter`:
- `requestPermission()` — called on app launch
- `scheduleDoseReminder()` — daily repeating notification at 9:00 AM, scheduled on first launch
- `sendMissedDoseNotification()` — fires immediately when BLE status transitions to "missed"
- `sendRefillNotification()` — fires when all 7 day names appear in dispensed/missed events
- `checkAndSendRefillIfNeeded(events:)` — checks against the 7 day name strings ("Monday" etc.)

**Wired into:**
- `MedSyncApp.swift` — permission request + dose reminder scheduled during splash screen
- `BluetoothManager.swift` — missed dose notification triggers on BLE status change from non-missed → missed

### 7. Caregiver Dashboard

**Created `ios/MedSyncApp/CaregiverDashboardView.swift`:**
- Weekly Summary: 4-number row (Taken / Unconfirmed / Not taken / Missed) from mock dose events
- 7-Day Overview: Mon-Sun coloured circles (green=taken, red=missed/not-taken, orange=pending, grey=no data)
- Missed Doses: list of missed events with day and time
- Refill Status: red warning if all 7 days covered, otherwise "X of 7 days dispensed"
- Photo Feed: placeholder until Supabase is wired up
- Uses `.navigationBarTitleDisplayMode(.inline)` to prevent title clipping at large text sizes

**Created `ios/MedSyncApp/SettingsView.swift`:**
- "Caregiver Mode" toggle stored with `@AppStorage("caregiverModeEnabled")` — persists across launches
- Dose schedule info (read-only: 9:00 AM, 60 min window)

**Updated `ios/MedSyncApp/ContentView.swift`:**
- Always shows: History, Controls, Settings tabs
- When `caregiverModeEnabled == true`: adds Caregiver tab between Controls and Settings

**Note:** These three new Swift files must be manually added to the Xcode project target (right-click MedSyncApp group → Add Files) or Xcode will throw "cannot find type in scope" build errors.

### 8. On-Device Computer Vision (No Python Required)

**Changed (`src/main.cpp`):**
- Camera pixel format changed from `PIXFORMAT_JPEG` to `PIXFORMAT_RGB565`
- Frame buffer location changed from `CAMERA_FB_IN_DRAM` to `CAMERA_FB_IN_PSRAM` (320x240 RGB565 = 150KB, needs PSRAM)
- Removed `jpeg_quality` setting (irrelevant for RGB565)

**Added `detectPillsOnDevice(camera_fb_t* fb)`:**
- Iterates all 76,800 pixels of the 320x240 frame
- Per pixel: extract R, G, B from RGB565; skip if too dark (<40 brightness) or near-white (>180 min channel); compute saturation
- If saturation > 50 and pixel passes brightness checks → coloured pixel (pill candidate)
- If >1.5% of frame is coloured → pills present
- Result written directly to Supabase `dose_events.pills_detected` — no Python script needed

**Added `cameraCaptureAndDetect()`:** replaces `cameraCaptureAndUpload()` in `doDispense()`

**Limitation:** Works well for coloured pills in a white/light-grey cup. All-white pills may not be detected reliably.

**Python CV processor (`scripts/cv_processor.py`)** kept in repo as optional higher-accuracy alternative. Deployment options:
- Local: `python cv_processor.py` (laptop must stay on)
- Railway or Render: free tier, deploy `scripts/` as a background worker

---

## File Change Summary

| File | Change |
|---|---|
| `src/main.cpp` | BLE dispense fix, GOTO/CALIBRATE commands, OLED next dose time, on-device CV |
| `include/config.h` | Status LED pin definitions added |
| `ios/MedSyncApp/ContentView.swift` | Settings + conditional Caregiver tab |
| `ios/MedSyncApp/MedSyncApp.swift` | Notification permission + reminder on launch |
| `ios/MedSyncApp/BluetoothManager.swift` | Missed dose notification on BLE status change |
| `ios/MedSyncApp/CommandPanelView.swift` | JSON status display, slot nav UI, calibrate button |
| `ios/MedSyncApp/NotificationManager.swift` | Created — all notification logic |
| `ios/MedSyncApp/CaregiverDashboardView.swift` | Created — caregiver tab |
| `ios/MedSyncApp/SettingsView.swift` | Created — settings tab with caregiver toggle |
| `README.md` | Full rewrite to reflect current architecture |

---

## Commits

| Hash | Description |
|---|---|
| `6508bda` | Add BLE dispense/navigation, iOS controls UI, firmware consolidation |
| `a129a2f` | Add notifications, caregiver dashboard, OLED next dose time |
| `980729c` | Update README to reflect current architecture and features |
| `d7bbd17` | Add on-device pill detection via RGB565 colour thresholding on ESP32 |

---

## Known Gaps (from idea_doc.pdf)

- Physical dispense button — dropped, replaced by app-triggered dispense
- Battery (LiPo + TP4056) — dropped from BOM, device runs USB-C
- Push notification to patient phone when dose window opens — iOS local notification handles this
- Caregiver notifications via Pushover — replaced by iOS local notifications
- React caregiver dashboard — replaced by native iOS SwiftUI caregiver tab
- `SupabaseService.swift` still uses mock data — real REST calls not wired up (intentional for demo)
- Photo upload removed when switching to on-device CV — photo feed placeholder in caregiver tab

---

## BLE Quick Reference

Device advertises as: `MedSync`
Service UUID: `4FAFC201-1FB5-459E-8FCC-C5C9C331914B`

| Command | Effect |
|---|---|
| `DISPENSE` | Advance one slot CW (skips load slot) |
| `LOAD` | Rotate to slot 7 (loading mode) |
| `UNLOAD` | Return to current day slot |
| `GOTO:N` | Go directly to slot N (0-7) |
| `CALIBRATE` | Set current position as Monday (slot 0), save to NVS |

Status JSON (notified every 3s):
```json
{"slot": 3, "status": "pending", "time": "09:15:30", "day": "THURSDAY"}
```
