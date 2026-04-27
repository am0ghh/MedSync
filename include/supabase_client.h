#pragma once
#include <Arduino.h>

// Inserts a new dose event record. Returns the UUID of the created row,
// or an empty string on failure.
String supabaseInsertDoseEvent(const char* day,
                               time_t scheduledTime,
                               const char* status);

// Updates an existing dose event record by UUID.
bool supabaseUpdateDoseEvent(const String& id,
                             const char* status,
                             const char* photoUrl,
                             int pillsDetected); // -1 = unknown, 0 = none, 1 = present

// Uploads a JPEG image to Supabase Storage.
// Returns the public URL, or empty string on failure.
String supabaseUploadImage(const uint8_t* data, size_t len, const String& filename);

// Checks the commands table for an unexecuted command.
// Returns the command string ("dispense", "load", "unload") or "" if none pending.
String supabaseCheckCommand();

// Marks a command as executed.
bool supabaseMarkCommandDone(const String& id);

// Deletes dose_event records (and their images) older than 14 days.
// Call this periodically from the Python processor or directly here.
bool supabaseCleanupOldRecords();
