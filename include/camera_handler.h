#pragma once
#include <Arduino.h>

// Initialises the OV2640 camera hardware.
// Returns true on success. Halts with OLED error screen on failure.
bool cameraInit();

// Captures a single JPEG frame, uploads it to Supabase Storage,
// and returns the public URL. Returns empty string on failure.
// filename should be unique (e.g. based on timestamp).
String cameraCaptureAndUpload(const String& filename);
