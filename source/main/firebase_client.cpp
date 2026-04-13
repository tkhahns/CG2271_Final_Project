#include "firebase_client.h"
#include "secrets.h"
#include "wifi_manager.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

static WiFiClientSecure firebaseClient;

static const char *warningLabel(uint8_t state) {
  switch (state) {
    case WARNING_STATE_IDLE:         return "Normal";
    case WARNING_STATE_GREEN:        return "Green";
    case WARNING_STATE_YELLOW:       return "Yellow";
    case WARNING_STATE_RED:          return "Critical";
    case WARNING_STATE_RED_BUZZER:   return "Critical+Buzzer";
    default:                         return "Unknown";
  }
}

void initFirebase() {
  firebaseClient.setInsecure();  // Skip cert pinning (acceptable for school project)
  Serial.println("[Firebase] Client ready.");
}

static String buildSensorJson(const DeskState &state) {
  JsonDocument doc;

  // Numeric fields — use null for invalid readings so JSON stays well-formed.
  if (isnan(state.temp))           doc["temperature"] = nullptr;
  else                             doc["temperature"] = serialized(String(state.temp, 1));

  if (isnan(state.humidity))       doc["humidity"] = nullptr;
  else                             doc["humidity"] = serialized(String(state.humidity, 1));

  if (state.distance < 0.0f)       doc["distance"] = nullptr;
  else                             doc["distance"] = serialized(String(state.distance, 1));

  doc["light"]             = state.light;
  doc["soundP2P"]          = state.soundP2P;
  doc["systemActive"]      = state.systemActive;
  doc["activeCount"]       = state.activeCount;
  doc["warningState"]      = state.warningState;
  doc["warningLabel"]      = warningLabel(state.warningState);
  doc["warningSuppressed"] = state.warningSuppressed;

  // Firebase server-side timestamp
  JsonObject ts = doc["timestamp"].to<JsonObject>();
  ts[".sv"] = "timestamp";

  String out;
  serializeJson(doc, out);
  return out;
}

bool logToFirebase(const DeskState &state) {
  if (!wifiIsConnected()) return false;

  const String json = buildSensorJson(state);
  bool ok = true;

  // --- 1. PUT /latest.json (overwrite with current reading) ---
  {
    HTTPClient http;
    const String url = "https://" + String(FIREBASE_HOST) +
                       "/latest.json?auth=" + String(FIREBASE_AUTH);
    http.begin(firebaseClient, url);
    http.addHeader("Content-Type", "application/json");
    const int code = http.PUT(json);

    if (code == 200) {
      Serial.println("[Firebase] /latest updated.");
    } else {
      Serial.printf("[Firebase] /latest error: %d\n", code);
      ok = false;
    }
    http.end();
  }

  // --- 2. POST /readings.json (timestamped history) ---
  {
    HTTPClient http;
    const String url = "https://" + String(FIREBASE_HOST) +
                       "/readings.json?auth=" + String(FIREBASE_AUTH);
    http.begin(firebaseClient, url);
    http.addHeader("Content-Type", "application/json");
    const int code = http.POST(json);

    if (code == 200) {
      Serial.println("[Firebase] /readings appended.");
    } else {
      Serial.printf("[Firebase] /readings error: %d\n", code);
      ok = false;
    }
    http.end();
  }

  return ok;
}
