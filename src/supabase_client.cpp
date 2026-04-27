#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "supabase_client.h"
#include "secrets.h"

// All Supabase REST calls go over HTTPS. setInsecure() skips cert verification
// which is acceptable for a prototype — swap in the Supabase root CA for production.

static void addSupabaseHeaders(HTTPClient& http) {
  http.addHeader("apikey",        SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Content-Type",  "application/json");
}

// ── Insert dose event ─────────────────────────────────────────────────────────
String supabaseInsertDoseEvent(const char* day, time_t scheduledTime, const char* status) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(SUPABASE_URL) + "/rest/v1/dose_events";
  http.begin(client, url);
  addSupabaseHeaders(http);
  http.addHeader("Prefer", "return=representation"); // return the created row

  // Format scheduledTime as ISO 8601
  struct tm ti;
  gmtime_r(&scheduledTime, &ti);
  char iso[30];
  strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &ti);

  JsonDocument doc;
  doc["day"]            = day;
  doc["scheduled_time"] = iso;
  doc["status"]         = status;
  String body;
  serializeJson(doc, body);

  int code = http.POST(body);
  String result = "";

  if (code == 201) {
    String resp = http.getString();
    JsonDocument respDoc;
    deserializeJson(respDoc, resp);
    // Response is an array; grab the first element's id
    if (respDoc.is<JsonArray>() && respDoc.as<JsonArray>().size() > 0) {
      result = respDoc[0]["id"].as<String>();
    }
    Serial.printf("[supabase] dose event inserted, id=%s\n", result.c_str());
  } else {
    Serial.printf("[supabase] insert failed, HTTP %d: %s\n", code, http.getString().c_str());
  }

  http.end();
  return result;
}

// ── Update dose event ─────────────────────────────────────────────────────────
bool supabaseUpdateDoseEvent(const String& id, const char* status,
                             const char* photoUrl, int pillsDetected) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(SUPABASE_URL) + "/rest/v1/dose_events?id=eq." + id;
  http.begin(client, url);
  addSupabaseHeaders(http);

  JsonDocument doc;
  doc["status"] = status;
  if (photoUrl && strlen(photoUrl) > 0) doc["photo_url"] = photoUrl;
  if (pillsDetected == 0) doc["pills_detected"] = false;
  if (pillsDetected == 1) doc["pills_detected"] = true;
  // dispensed_at: set server-side using now()
  if (strcmp(status, "dispensed") == 0) {
    time_t now;
    time(&now);
    struct tm ti;
    gmtime_r(&now, &ti);
    char iso[30];
    strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &ti);
    doc["dispensed_at"] = iso;
  }

  String body;
  serializeJson(doc, body);

  int code = http.PATCH(body);
  bool ok = (code == 200 || code == 204);
  if (!ok) Serial.printf("[supabase] update failed, HTTP %d\n", code);
  http.end();
  return ok;
}

// ── Upload image to Supabase Storage ─────────────────────────────────────────
String supabaseUploadImage(const uint8_t* data, size_t len, const String& filename) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(SUPABASE_URL) + "/storage/v1/object/" +
               SUPABASE_BUCKET + "/" + filename;
  http.begin(client, url);
  http.addHeader("apikey",        SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Content-Type",  "image/jpeg");

  int code = http.POST((uint8_t*)data, len);

  String publicUrl = "";
  if (code == 200 || code == 201) {
    publicUrl = String(SUPABASE_URL) + "/storage/v1/object/public/" +
                SUPABASE_BUCKET + "/" + filename;
    Serial.printf("[supabase] image uploaded: %s\n", publicUrl.c_str());
  } else {
    Serial.printf("[supabase] image upload failed, HTTP %d: %s\n",
                  code, http.getString().c_str());
  }

  http.end();
  return publicUrl;
}

// ── Check for pending remote command ─────────────────────────────────────────
// Returns the oldest unexecuted command string, or "" if none.
// Also returns the command's UUID via the passed-by-reference cmdId for acking.
static String pendingCmdId = "";

String supabaseCheckCommand() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(SUPABASE_URL) +
               "/rest/v1/commands?executed=eq.false&order=created_at.asc&limit=1";
  http.begin(client, url);
  http.addHeader("apikey",        SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);

  int code = http.GET();
  String cmd = "";

  if (code == 200) {
    String resp = http.getString();
    JsonDocument doc;
    deserializeJson(doc, resp);
    if (doc.is<JsonArray>() && doc.as<JsonArray>().size() > 0) {
      cmd          = doc[0]["command"].as<String>();
      pendingCmdId = doc[0]["id"].as<String>();
      Serial.printf("[supabase] command received: %s\n", cmd.c_str());
    }
  }

  http.end();
  return cmd;
}

// ── Acknowledge (mark done) the last fetched command ─────────────────────────
bool supabaseMarkCommandDone(const String& id) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(SUPABASE_URL) + "/rest/v1/commands?id=eq." + id;
  http.begin(client, url);
  addSupabaseHeaders(http);

  int code = http.PATCH("{\"executed\":true}");
  http.end();
  return (code == 200 || code == 204);
}

// ── Delete records older than 14 days ────────────────────────────────────────
bool supabaseCleanupOldRecords() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  // Supabase REST supports filtering with lt. (less-than) on timestamps.
  String url = String(SUPABASE_URL) +
               "/rest/v1/dose_events?created_at=lt." +
               "now()-interval+14+days"; // PostgREST interprets this
  // Note: for robust cleanup, the Python processor handles this instead.
  // This call is a best-effort sweep when the ESP32 is connected.
  http.begin(client, url);
  addSupabaseHeaders(http);

  int code = http.sendRequest("DELETE");
  http.end();
  Serial.printf("[supabase] cleanup HTTP %d\n", code);
  return (code == 200 || code == 204);
}
