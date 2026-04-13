#include "firebase_client.h"
#include "secrets.h"
#include "wifi_manager.h"
#include "time_util.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

static WiFiClientSecure firebaseClient;

// Reliability tuning -------------------------------------------------
static const uint16_t FB_HTTP_TIMEOUT_MS = 5000;   // per-call TLS+HTTP budget
static const uint8_t  FB_MAX_RETRIES     = 1;     // total tries = 1 + retries
static const uint16_t FB_RETRY_DELAY_MS  = 250;   // brief pause before retry

static uint16_t fbConsecutiveFailures = 0;
// --------------------------------------------------------------------

// Returns HTTP code, or negative HTTPClient error.
// Retries once on transient TLS / connection errors.
static int firebaseSend(const char *method, const String &url, const String &body) {
  for (uint8_t attempt = 0; attempt <= FB_MAX_RETRIES; ++attempt) {
    HTTPClient http;
    http.setTimeout(FB_HTTP_TIMEOUT_MS);
    http.setConnectTimeout(FB_HTTP_TIMEOUT_MS);
    http.setReuse(true);

    if (!http.begin(firebaseClient, url)) {
      Serial.println("[Firebase] http.begin() failed.");
      delay(FB_RETRY_DELAY_MS);
      continue;
    }
    http.addHeader("Content-Type", "application/json");

    int code;
    if (strcmp(method, "PUT") == 0) {
      code = http.PUT(body);
    } else {
      code = http.POST(body);
    }
    http.end();

    // Success or non-transient HTTP error → return immediately.
    const bool transient =
      (code == HTTPC_ERROR_CONNECTION_REFUSED) ||  // -1
      (code == HTTPC_ERROR_CONNECTION_LOST)    ||  // -5
      (code == HTTPC_ERROR_READ_TIMEOUT)       ||  // -11
      (code == HTTPC_ERROR_NOT_CONNECTED)      ||  // -4
      (code >= 500 && code < 600);                 // 5xx server errors

    if (!transient) return code;

    if (attempt < FB_MAX_RETRIES) {
      Serial.printf("[Firebase] %s transient err %d — retrying.\n", method, code);
      delay(FB_RETRY_DELAY_MS);
    } else {
      return code;
    }
  }
  return HTTPC_ERROR_CONNECTION_REFUSED;
}

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

  // Firebase server-side timestamp (ms since epoch, set by Firebase)
  JsonObject ts = doc["timestamp"].to<JsonObject>();
  ts[".sv"] = "timestamp";

  // Human-readable ESP32-local time (from NTP, falls back to uptime)
  doc["isoTime"]     = currentIsoString();
  doc["uptime_s"]    = uptimeSeconds();

  String out;
  serializeJson(doc, out);
  return out;
}

bool logToFirebase(const DeskState &state) {
  if (!wifiIsConnected()) return false;

  const String json = buildSensorJson(state);
  const String authParam = "?auth=" + String(FIREBASE_AUTH);
  bool ok = true;

  // --- 1. PUT /latest.json (overwrite with current reading) ---
  {
    const String url = "https://" + String(FIREBASE_HOST) + "/latest.json" + authParam;
    const int code = firebaseSend("PUT", url, json);
    if (code == 200) {
      Serial.println("[Firebase] /latest updated.");
    } else {
      Serial.printf("[Firebase] /latest error: %d\n", code);
      ok = false;
    }
  }

  // --- 2. POST /readings.json (timestamped history) ---
  {
    const String url = "https://" + String(FIREBASE_HOST) + "/readings.json" + authParam;
    const int code = firebaseSend("POST", url, json);
    if (code == 200) {
      Serial.println("[Firebase] /readings appended.");
    } else {
      Serial.printf("[Firebase] /readings error: %d\n", code);
      ok = false;
    }
  }

  // Track failure streaks so we can escalate the warning.
  if (ok) {
    if (fbConsecutiveFailures > 0) {
      Serial.printf("[Firebase] Recovered after %u consecutive failures.\n",
                    fbConsecutiveFailures);
    }
    fbConsecutiveFailures = 0;
  } else {
    fbConsecutiveFailures++;
    if (fbConsecutiveFailures == 5 || fbConsecutiveFailures % 20 == 0) {
      Serial.printf("[Firebase] WARNING — %u consecutive write failures. "
                    "Check Wi-Fi / DNS / Firebase host.\n",
                    fbConsecutiveFailures);
    }
  }

  return ok;
}
