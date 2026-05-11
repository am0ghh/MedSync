# MedSync — Smart Medication Dispenser

MedSync is a smart pill dispenser built around the Freenove ESP32-S3-WROOM. It automates daily medication scheduling, dispenses pills on a timed schedule, captures a photo of the collection cup after each dispense, and pushes adherence data to a Supabase backend. A caregiver can monitor and control the device remotely through an iOS app over Bluetooth.

---

## Features

- **Automated daily dispensing** — rotates a 3D-printed 8-slot carousel to the correct day's compartment at a scheduled time (default 9:00 AM)
- **BLE wireless control** — iOS app connects over Bluetooth to dispense, navigate slots, enter/exit loading mode, and calibrate the carousel home position
- **On-device computer vision** — immediately after dispensing, the camera captures an RGB565 frame and runs a colour saturation algorithm directly on the ESP32 to detect whether pills are present in the cup. No external server or internet connection required for pill detection.
- **Optional Python CV backend** — a more accurate OpenCV-based detector (`scripts/cv_processor.py`) can run on any internet-connected machine as an alternative or supplement to on-device detection
- **Push notifications** — daily dose reminder at the scheduled time; missed dose alert fires when BLE status transitions to missed; refill alert fires when all 7 day slots have been used
- **Caregiver dashboard** — optional tab in the iOS app showing weekly adherence summary, 7-day status grid, missed doses, and refill status
- **Loading mode** — rotates the blocked slot over the dispensing hole so the caregiver can safely refill compartments
- **Slot navigation and calibration** — go directly to any named day slot or physically calibrate the Monday home position without moving the motor
- **OLED display** — shows current day, live clock, next scheduled dose time, dose status, and a 7-day progress indicator
- **Local alerts** — onboard buzzer and NeoPixel RGB LED notify the patient when a dose is due or missed
- **Persistent state** — carousel position survives reboots via NVS flash memory
- **Automatic cleanup** — Supabase records and photos older than 14 days are deleted automatically by the CV backend

---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | Freenove ESP32-S3-WROOM |
| Stepper motor | 28BYJ-48 |
| Motor driver | ULN2003 |
| Display | SSD1306 0.96" OLED (I2C) |
| Camera | OV2640 (built into Freenove board) |
| Buzzer | Active buzzer |
| LED | Onboard WS2812B NeoPixel (GPIO 48) |
| Power | USB-C |

### Pin Assignments

| Signal | GPIO |
|---|---|
| Stepper IN1 | 1 |
| Stepper IN2 | 2 |
| Stepper IN3 | 42 |
| Stepper IN4 | 41 |
| OLED SDA | 39 |
| OLED SCL | 40 |
| Buzzer | 38 |
| NeoPixel | 48 (onboard) |

> Camera pins (4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18) are reserved by the OV2640 and must not be used for anything else.

### Carousel Layout

The carousel has 8 slots. Slots 0-6 hold one day's medication each. Slot 7 is a solid blocked slot used during loading mode to seal the dispense hole.

```
Slot 0 -> Monday
Slot 1 -> Tuesday
Slot 2 -> Wednesday
Slot 3 -> Thursday
Slot 4 -> Friday
Slot 5 -> Saturday
Slot 6 -> Sunday
Slot 7 -> LOAD (blocked)
```

The motor uses half-step drive (512 half-steps = 45 degrees per slot).

---

## System Architecture

```
+----------------------------------+
|       ESP32-S3 Firmware          |
|                                  |
|  State machine (dose workflow)   |
|  Scheduler (NTP time sync)       |
|  Stepper motor controller        |
|  OLED display renderer           |
|  Buzzer + NeoPixel alerts        |
|  Camera capture                  |
|  BLE GATT server                 |
|  Supabase REST client            |
+----------+----------+------------+
           |          |
        BLE (local) HTTPS (WiFi)
           |          |
+----------+   +------+------------------+
| iOS App  |   |       Supabase          |
|          |   |                         |
| History  |   | dose_events (Postgres)  |
| Controls |   | commands    (Postgres)  |
| Caregiver|   | pill-photos (Storage)   |
| Settings |   +------------+------------+
+----------+                |
                            |
               +------------+-----------+
               |      CV Processor      |
               |      (Python)          |
               |                        |
               | OpenCV pill detection  |
               | Auto-cleanup (14 days) |
               +------------------------+
```

---

## BLE Protocol

The device advertises as **MedSync** with service UUID `4FAFC201-1FB5-459E-8FCC-C5C9C331914B`.

### Characteristics

| UUID | Direction | Purpose |
|---|---|---|
| `BEB5483E-...-26A8` | Write | Send commands to device |
| `BEB5483E-...-26A9` | Notify | Receive status JSON every 3 seconds |

### Commands (write to command characteristic)

| Command | Effect |
|---|---|
| `DISPENSE` | Advances carousel one slot clockwise (skips load slot). Works from any state except LOADING. |
| `LOAD` | Rotates to slot 7 (loading mode — dispense hole sealed) |
| `UNLOAD` | Returns from loading mode to current day slot |
| `GOTO:N` | Moves directly to slot N (0-7) |
| `CALIBRATE` | Sets current physical position as slot 0 (Monday) without moving motor. Save to NVS. |

### Status JSON (notified every 3 seconds)

```json
{"slot": 3, "status": "pending", "time": "09:15:30", "day": "THURSDAY"}
```

---

## Dose Workflow

1. At the scheduled dose time the buzzer and NeoPixel activate; a push notification fires on the iOS app
2. The caregiver or patient triggers dispensing via the iOS app
3. The stepper motor rotates one slot clockwise, dropping pills into the collection cup
4. The onboard camera captures a JPEG of the cup
5. The image is uploaded to Supabase Storage; a dose event record is created
6. The Python CV processor detects whether pills are present in the cup
7. The result (`pills_detected: true/false`) is written back to the database
8. If no dispense occurs within 60 minutes, the dose is marked missed and a push notification fires
9. If pills are still detected 30 minutes after dispensing, a secondary alert fires

---

## Project Structure

```
MedSync/
+-- src/
|   +-- main.cpp              # All firmware — state machine, stepper, display,
|                             #   scheduler, BLE, alerts, camera, Supabase client
+-- include/
|   +-- config.h              # Pin definitions and tunable constants
|   +-- secrets.h             # WiFi + Supabase credentials (gitignored)
+-- ios/MedSyncApp/
|   +-- MedSyncApp.swift      # App entry point, notification setup
|   +-- ContentView.swift     # Tab layout (History / Controls / Caregiver / Settings)
|   +-- BluetoothManager.swift# CoreBluetooth — scan, connect, send commands, receive status
|   +-- NotificationManager.swift # Local notifications — dose reminder, missed, refill
|   +-- DoseHistoryView.swift # Weekly dose history list
|   +-- DoseDetailView.swift  # Per-dose detail screen
|   +-- CommandPanelView.swift# BLE controls — dispense, load/unload, slot nav, calibrate
|   +-- CaregiverDashboardView.swift # Caregiver tab — summary, grid, missed doses, refill
|   +-- SettingsView.swift    # Settings tab — caregiver mode toggle, dose schedule info
|   +-- SupabaseService.swift # Data layer (mock data for demo)
|   +-- Models.swift          # DoseEvent model
|   +-- Config.swift          # App-level constants
+-- scripts/
|   +-- cv_processor.py       # OpenCV pill detection backend
|   +-- requirements.txt
|   +-- .env.example
+-- supabase_setup.sql        # Run once in Supabase SQL Editor to create tables
+-- platformio.ini
+-- medsync.kicad_sch         # KiCad schematic
```

---

## Computer Vision

### On-Device Detection (default)

Pill detection runs directly on the ESP32-S3 with no external dependency. After each dispense the camera captures a 320x240 RGB565 frame into PSRAM and the firmware iterates every pixel:

1. Converts RGB565 to 8-bit R, G, B channels
2. Skips pixels that are too dark (shadows) or near-white (the cup interior)
3. Computes a saturation value for the remaining pixels
4. If more than 1.5% of the frame contains saturated, coloured pixels — pills are present

The result (`pills_detected: true/false`) is written directly to Supabase without any intermediate server. The device works fully standalone: no laptop, no cloud function, no Python runtime needed.

Limitations: works well for coloured pills against a white or light-grey cup. All-white pills may not be detected reliably by the on-device path.

### Optional Python Backend (higher accuracy)

`scripts/cv_processor.py` uses OpenCV with colour segmentation, morphological filtering, and a HoughCircles fallback for white pills. It is more accurate than the on-device path but requires a host.

**Hosting options:**

| Option | Setup | Cost |
|---|---|---|
| Local laptop | `python cv_processor.py` in a terminal | Free, laptop must stay on |
| Railway | Push `scripts/` folder, set env vars, deploy as background worker | Free tier available |
| Render | Same as Railway — connect repo, set build command to `pip install -r requirements.txt`, start command to `python cv_processor.py` | Free tier available |

To use the Python backend, set `SUPABASE_URL` and `SUPABASE_KEY` in `scripts/.env` and run the script. It polls Supabase every 15 seconds for new unanalysed events and overwrites the on-device result if needed.

---

## Setup

### 1. Supabase

1. Create a free project at supabase.com
2. Go to Storage, create a bucket named `pill-photos`, set it to public
3. Run `supabase_setup.sql` in the SQL Editor to create the required tables
4. From Settings > API, copy the Project URL and service role key

### 2. Firmware

1. Create `include/secrets.h` (gitignored — never commit this):

```cpp
#pragma once
#define WIFI_SSID       "your_network"
#define WIFI_PASS       "your_password"
#define SUPABASE_URL    "https://your-project.supabase.co"
#define SUPABASE_KEY    "your_service_role_key"
#define SUPABASE_BUCKET "pill-photos"
```

2. Edit `include/config.h` to set your timezone, dose time, and verify all pin assignments match your wiring.

3. Flash with PlatformIO:

```bash
pio run --target upload
pio device monitor
```

### 3. iOS App

Open `ios/MedSyncApp.xcodeproj` in Xcode. Select your iPhone as the run target, set a development team under Signing & Capabilities, then hit Run. On first launch the app requests notification permission and schedules the daily dose reminder.

To enable the caregiver dashboard, go to the Settings tab and toggle Caregiver Mode.

### 4. Python CV Processor

```bash
cd scripts
cp .env.example .env
# Fill in SUPABASE_URL and SUPABASE_KEY in .env
pip install -r requirements.txt
python cv_processor.py
```

The processor runs continuously, polling Supabase every 15 seconds for new unanalysed images.

### 5. Physical Calibration

After assembling the carousel:

1. Power on the ESP32 and open the iOS app
2. Connect over BLE from the Controls tab
3. Hand-rotate the carousel until Monday's compartment is directly over the dispense hole
4. Tap **Calibrate Home** in the Slot Navigation section
5. Use the day buttons (Mon-Sun) to verify each slot rotates to the correct position

---

## Configuration Reference (`config.h`)

| Constant | Default | Description |
|---|---|---|
| `DOSE_HOUR` | `9` | Hour of scheduled dose (24h) |
| `DOSE_MINUTE` | `0` | Minute of scheduled dose |
| `DOSE_WINDOW_MIN` | `60` | Minutes before dose is marked missed |
| `VERIFY_WINDOW_MIN` | `30` | Minutes after dispense before secondary alert |
| `TIMEZONE_POSIX` | `"EST5EDT,..."` | POSIX timezone string |
| `CMD_POLL_MS` | `5000` | Supabase command poll interval (ms) |
| `STEPS_PER_SLOT` | `512` | Half-steps per 45-degree slot rotation |
| `TOTAL_SLOTS` | `8` | Total carousel slots (7 days + 1 load slot) |

---

## Dependencies

### Firmware (PlatformIO)
- U8g2 — OLED display
- esp32-camera — OV2640 driver
- ArduinoJson — JSON serialisation for Supabase REST
- Adafruit NeoPixel — WS2812B LED
- ESP32 BLE Arduino — BLE GATT server

### Python CV Processor
- opencv-python
- numpy
- requests
- python-dotenv
