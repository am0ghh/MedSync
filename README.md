# MedSync — Smart Medication Dispenser

MedSync is an embedded smart pill dispenser system built around the Freenove ESP32-S3-WROOM. It automates medication scheduling, dispenses pills on a daily schedule, captures a photo of the collection cup after each dispense, and pushes adherence data to a live caregiver dashboard via Supabase.

---

## Features

- **Automated daily dispensing** — rotates a 3D-printed 8-slot carousel to the correct day's compartment at a scheduled time
- **Camera verification** — captures a JPEG photo of the collection cup immediately after dispensing and uploads it to cloud storage
- **Computer vision pill detection** — a Python backend analyses each photo using OpenCV to determine whether pills were actually taken
- **Live caregiver dashboard** — real-time adherence log, dose status, and photo feed via Supabase
- **Remote control** — caregivers can trigger dispensing or loading mode remotely through the dashboard
- **Loading mode** — physically seals the dispenser during refill by rotating a blocked slot over the dispensing hole
- **Local alerts** — onboard buzzer and RGB LED notify the patient when a dose is due
- **Persistent state** — carousel position and dose history survive reboots via NVS flash memory
- **Automatic cleanup** — photos and records older than 14 days are deleted automatically

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
| Power | USB-C wall power |

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

> **Note:** Camera pins (4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18) are reserved by the OV2640 peripheral and must not be used for anything else.

### Carousel Layout

The 3D-printed carousel has 8 slots. Slots 0–6 hold one day's medication each. Slot 7 is a solid blocked slot used during loading mode to physically seal the dispense hole.

```
Slot 0 → Monday
Slot 1 → Tuesday
Slot 2 → Wednesday
Slot 3 → Thursday
Slot 4 → Friday
Slot 5 → Saturday
Slot 6 → Sunday
Slot 7 → LOAD (blocked)
```

The motor uses half-step drive (512 half-steps = 45°) for smooth, quiet rotation.

---

## System Architecture

```
┌─────────────────────────────────┐
│       ESP32-S3 Firmware         │
│                                 │
│  Scheduler (NTP time sync)      │
│  State machine (dose workflow)  │
│  Stepper motor controller       │
│  OLED display renderer          │
│  Buzzer + NeoPixel alerts       │
│  Camera capture                 │
│  Supabase REST client           │
└────────────┬────────────────────┘
             │ HTTPS (WiFi)
             ▼
┌─────────────────────────────────┐
│           Supabase              │
│                                 │
│  dose_events  (PostgreSQL)      │
│  commands     (PostgreSQL)      │
│  pill-photos  (Storage)         │
└────────┬───────────┬────────────┘
         │           │
         ▼           ▼
┌──────────────┐  ┌──────────────────────┐
│  CV Processor│  │  Caregiver Dashboard │
│  (Python)    │  │  (React — coming)    │
│              │  │                      │
│  OpenCV pill │  │  Adherence log       │
│  detection   │  │  Photo feed          │
│  Auto-cleanup│  │  Remote dispense     │
└──────────────┘  └──────────────────────┘
```

---

## Dose Workflow

1. At the scheduled dose time, the buzzer and NeoPixel activate
2. The caregiver app (or auto-timeout) triggers dispensing
3. The stepper motor rotates to today's compartment, dropping pills into the cup below
4. The onboard camera flashes white and captures a JPEG of the cup
5. The image is uploaded to Supabase Storage; a dose event record is created
6. The Python CV processor detects whether pills are present in the cup
7. The result (`pills_detected: true/false`) is written back to the database
8. The caregiver dashboard updates in real time
9. If no dispense occurs within 60 minutes, the dose is marked **missed**
10. If pills are still detected 30 minutes after dispensing, a secondary alert fires

---

## Project Structure

```
PeterCorp/
├── src/
│   ├── main.cpp              # State machine entry point
│   ├── stepper.cpp           # Motor control + NVS position tracking
│   ├── display.cpp           # OLED rendering
│   ├── scheduler.cpp         # WiFi, NTP, dose window logic
│   ├── alerts.cpp            # Buzzer + NeoPixel patterns
│   ├── supabase_client.cpp   # Supabase REST API client
│   └── camera_handler.cpp    # OV2640 capture + upload
├── include/
│   ├── config.h              # Pin definitions and constants
│   ├── secrets.h             # WiFi + Supabase credentials (gitignored)
│   ├── stepper.h
│   ├── display.h
│   ├── scheduler.h
│   ├── alerts.h
│   ├── supabase_client.h
│   └── camera_handler.h
├── scripts/
│   ├── cv_processor.py       # OpenCV pill detection backend
│   ├── requirements.txt
│   └── .env.example
├── supabase_setup.sql        # Run once in Supabase SQL Editor
└── platformio.ini
```

---

## Setup

### 1. Supabase

1. Create a free project at [supabase.com](https://supabase.com)
2. Go to **Storage → New bucket**, name it `pill-photos`, enable **Public bucket**
3. Run `supabase_setup.sql` in **SQL Editor** to create the required tables
4. From **Settings → API**, copy your **Project URL** and **service role key**

### 2. Firmware

1. Create `include/secrets.h` (it is gitignored — never commit it):

```cpp
#pragma once
#define WIFI_SSID       "your_network"
#define WIFI_PASS       "your_password"
#define SUPABASE_URL    "https://your-project.supabase.co"
#define SUPABASE_KEY    "your_service_role_key"
#define SUPABASE_BUCKET "pill-photos"
```

2. Edit `include/config.h`:
   - Set `TIMEZONE_POSIX` to your local timezone
   - Set `DOSE_HOUR` and `DOSE_MINUTE` to the patient's dose time

3. Flash with PlatformIO:

```bash
pio run --target upload
pio device monitor
```

4. Confirm the Serial Monitor shows:
```
[wifi] connected
[ntp] time: 2026-04-26 09:00:00
[camera] ready
```

### 3. Python CV Processor

```bash
cd scripts
cp .env.example .env
# Fill in SUPABASE_URL and SUPABASE_KEY in .env

pip install -r requirements.txt
python cv_processor.py
```

The processor runs continuously, polling Supabase every 15 seconds for new unanalysed images.

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

---

## Remote Commands

The caregiver dashboard can insert rows into the `commands` table to control the device remotely:

| Command | Effect |
|---|---|
| `dispense` | Triggers motor + camera (only valid during dose window) |
| `load` | Rotates to slot 7 (loading mode) |
| `unload` | Returns to current day's slot, resumes normal operation |

---

## Dependencies

### Firmware (PlatformIO)
- [U8g2](https://github.com/olikraus/u8g2) — OLED display
- [esp32-camera](https://github.com/espressif/esp32-camera) — OV2640 driver
- [ArduinoJson](https://arduinojson.org/) — JSON serialisation for Supabase REST
- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) — WS2812B LED

### Python CV Processor
- opencv-python
- numpy
- requests
- python-dotenv
