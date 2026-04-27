"""
MedSync CV Processor
--------------------
Polls Supabase for dose events that have been dispensed but not yet analysed,
downloads the pill photo, runs OpenCV colour segmentation to detect whether
pills are present in the cup, then writes the result back to Supabase.

Also handles the 14-day cleanup of old records and storage objects.

Run this script continuously on any machine that has internet access:
    python cv_processor.py

Dependencies:  pip install -r requirements.txt
"""

import os
import time
import logging
import requests
import numpy as np
import cv2
from datetime import datetime, timezone, timedelta
from dotenv import load_dotenv

load_dotenv()

# ── Config ────────────────────────────────────────────────────────────────────
SUPABASE_URL    = os.getenv("SUPABASE_URL")
SUPABASE_KEY    = os.getenv("SUPABASE_KEY")
SUPABASE_BUCKET = os.getenv("SUPABASE_BUCKET", "pill-photos")
POLL_INTERVAL   = int(os.getenv("POLL_INTERVAL_SEC", "15"))
CLEANUP_DAYS    = 14

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s"
)
log = logging.getLogger("medsync-cv")

HEADERS = {
    "apikey":        SUPABASE_KEY,
    "Authorization": f"Bearer {SUPABASE_KEY}",
    "Content-Type":  "application/json",
}

# ── Supabase helpers ──────────────────────────────────────────────────────────

def fetch_unanalysed_events():
    """Returns dose events with status='dispensed' and pills_detected IS NULL."""
    url = (
        f"{SUPABASE_URL}/rest/v1/dose_events"
        "?status=eq.dispensed"
        "&pills_detected=is.null"
        "&order=created_at.asc"
    )
    r = requests.get(url, headers=HEADERS, timeout=10)
    r.raise_for_status()
    return r.json()


def update_event(event_id: str, pills_detected: bool):
    url = f"{SUPABASE_URL}/rest/v1/dose_events?id=eq.{event_id}"
    payload = {"pills_detected": pills_detected}
    r = requests.patch(url, json=payload, headers=HEADERS, timeout=10)
    r.raise_for_status()
    log.info(f"Updated event {event_id}: pills_detected={pills_detected}")


def download_image(photo_url: str) -> np.ndarray | None:
    """Downloads a JPEG from Supabase Storage and returns an OpenCV image."""
    try:
        r = requests.get(photo_url, headers=HEADERS, timeout=15)
        r.raise_for_status()
        arr = np.frombuffer(r.content, dtype=np.uint8)
        img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
        return img
    except Exception as e:
        log.error(f"Image download failed: {e}")
        return None


def delete_storage_object(filename: str):
    """Deletes a single object from Supabase Storage."""
    url = f"{SUPABASE_URL}/storage/v1/object/{SUPABASE_BUCKET}/{filename}"
    requests.delete(url, headers={
        "apikey":        SUPABASE_KEY,
        "Authorization": f"Bearer {SUPABASE_KEY}",
    }, timeout=10)


def cleanup_old_records():
    """
    Deletes dose_events older than CLEANUP_DAYS days,
    and removes their associated images from Storage.
    """
    cutoff = (datetime.now(timezone.utc) - timedelta(days=CLEANUP_DAYS)).isoformat()
    url = f"{SUPABASE_URL}/rest/v1/dose_events?created_at=lt.{cutoff}"

    # Fetch records to get photo URLs before deleting.
    r = requests.get(url, headers=HEADERS, timeout=10)
    old_records = r.json() if r.ok else []

    for record in old_records:
        photo_url = record.get("photo_url", "")
        if photo_url:
            # Extract filename from the public URL.
            filename = photo_url.split(f"/{SUPABASE_BUCKET}/")[-1]
            delete_storage_object(filename)
            log.info(f"Deleted storage object: {filename}")

    if old_records:
        r = requests.delete(url, headers=HEADERS, timeout=10)
        log.info(f"Cleaned up {len(old_records)} record(s) older than {CLEANUP_DAYS} days")


# ── Computer Vision ───────────────────────────────────────────────────────────

def detect_pills(img: np.ndarray) -> bool:
    """
    Returns True if pills are detected in the collection cup.

    Strategy:
      1. Convert to HSV colour space.
      2. Exclude background colours (white cup, grey shadows, black background).
      3. Count the remaining 'foreground' coloured pixels.
      4. If they exceed a threshold → pills present.

    This works well for coloured pills (orange, yellow, pink, blue, red) in a
    white or clear cup. For all-white pills, the shape-based fallback (circles)
    provides a secondary check.
    """
    if img is None:
        return False

    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)

    # ── Colour exclusion masks ────────────────────────────────────────────────
    # White / near-white (the cup itself)
    white_mask = cv2.inRange(hsv,
        np.array([0,  0, 180]),
        np.array([180, 40, 255])
    )
    # Black / dark background
    black_mask = cv2.inRange(hsv,
        np.array([0,  0,  0]),
        np.array([180, 255, 60])
    )
    # Grey (shadows, cup rim)
    grey_mask = cv2.inRange(hsv,
        np.array([0,  0, 60]),
        np.array([180, 30, 180])
    )

    exclude = cv2.bitwise_or(white_mask, cv2.bitwise_or(black_mask, grey_mask))
    colour_mask = cv2.bitwise_not(exclude)

    # Morphological opening: remove tiny noise specks.
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    colour_mask = cv2.morphologyEx(colour_mask, cv2.MORPH_OPEN, kernel)

    coloured_pixels = cv2.countNonZero(colour_mask)
    total_pixels    = img.shape[0] * img.shape[1]
    ratio           = coloured_pixels / total_pixels

    log.debug(f"Colour ratio: {ratio:.4f} ({coloured_pixels}/{total_pixels})")

    # Primary decision: >1.5% coloured pixels → pills detected.
    if ratio > 0.015:
        return True

    # ── Fallback: circle detection for white pills ────────────────────────────
    grey_img = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    blurred  = cv2.GaussianBlur(grey_img, (9, 9), 2)
    circles  = cv2.HoughCircles(
        blurred,
        cv2.HOUGH_GRADIENT,
        dp=1.2,
        minDist=20,
        param1=50,
        param2=30,
        minRadius=5,
        maxRadius=40
    )

    if circles is not None:
        log.debug(f"HoughCircles found {len(circles[0])} circle(s)")
        return True

    return False


# ── Main loop ─────────────────────────────────────────────────────────────────

def main():
    log.info("MedSync CV Processor started")
    log.info(f"Polling every {POLL_INTERVAL}s — Supabase: {SUPABASE_URL}")

    cleanup_counter = 0

    while True:
        try:
            events = fetch_unanalysed_events()

            for event in events:
                event_id  = event["id"]
                photo_url = event.get("photo_url", "")
                log.info(f"Analysing event {event_id} — {photo_url}")

                if not photo_url:
                    log.warning("No photo URL — skipping")
                    update_event(event_id, False)
                    continue

                img           = download_image(photo_url)
                pills_present = detect_pills(img)

                log.info(f"Result: {'PILLS DETECTED' if pills_present else 'CUP EMPTY'}")
                update_event(event_id, pills_present)

            # Run cleanup once per hour (every 240 × 15s polls).
            cleanup_counter += 1
            if cleanup_counter >= 240:
                cleanup_counter = 0
                cleanup_old_records()

        except requests.exceptions.RequestException as e:
            log.error(f"Network error: {e}")
        except Exception as e:
            log.exception(f"Unexpected error: {e}")

        time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    main()
