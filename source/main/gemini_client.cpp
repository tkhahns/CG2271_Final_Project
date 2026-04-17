#include "gemini_client.h"
#include "secrets.h"
#include "wifi_manager.h"
#include "time_util.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

static WiFiClientSecure aiClient;
static uint32_t lastCallMs    = 0;
static uint32_t backoffUntilMs = 0;  // set after a 429 response
static String lastGeminiError;

static const uint16_t GEMINI_HTTP_TIMEOUT_MS = 8000;
static const uint8_t  GEMINI_MAX_RETRIES     = 1;
static const uint16_t GEMINI_RETRY_DELAY_MS  = 300;

static void setGeminiError(const String &message) {
  lastGeminiError = message;
}

String getLastGeminiError() {
  return lastGeminiError;
}

static const char *httpErrorName(int code) {
  switch (code) {
    case HTTPC_ERROR_CONNECTION_REFUSED: return "connection refused";
    case HTTPC_ERROR_SEND_HEADER_FAILED: return "send header failed";
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED:return "send payload failed";
    case HTTPC_ERROR_NOT_CONNECTED:      return "not connected";
    case HTTPC_ERROR_CONNECTION_LOST:    return "connection lost";
    case HTTPC_ERROR_NO_STREAM:          return "no response stream";
    case HTTPC_ERROR_NO_HTTP_SERVER:     return "no HTTP server";
    case HTTPC_ERROR_TOO_LESS_RAM:       return "not enough RAM";
    case HTTPC_ERROR_ENCODING:           return "transfer encoding error";
    case HTTPC_ERROR_STREAM_WRITE:       return "stream write error";
    case HTTPC_ERROR_READ_TIMEOUT:       return "read timeout";
    default:                            return "";
  }
}

static bool isTransientHttpError(int code) {
  return (code == HTTPC_ERROR_CONNECTION_REFUSED) ||
         (code == HTTPC_ERROR_CONNECTION_LOST) ||
         (code == HTTPC_ERROR_NOT_CONNECTED) ||
         (code == HTTPC_ERROR_READ_TIMEOUT) ||
         (code >= 500 && code < 600);
}

static const char *warningDesc(uint8_t s) {
  switch (s) {
    case WARNING_STATE_IDLE:         return "normal";
    case WARNING_STATE_GREEN:        return "green";
    case WARNING_STATE_YELLOW:       return "yellow";
    case WARNING_STATE_RED:          return "critical";
    case WARNING_STATE_RED_BUZZER:   return "critical with buzzer";
    default:                         return "unknown";
  }
}

void initGemini() {
  aiClient.setInsecure();
  lastCallMs = 0;
  lastGeminiError = "";
  Serial.println("[Gemini] Client ready.");
}

static String buildContext(const DeskState &s) {
  String ctx = "Time: ";
  ctx += currentIsoString();
  ctx += ". Current desk sensor readings: ";
  ctx += "temp=";
  ctx += isnan(s.temp) ? String("N/A") : String(s.temp, 1) + "C";
  ctx += ", humidity=";
  ctx += isnan(s.humidity) ? String("N/A") : String(s.humidity, 1) + "%";
  ctx += ", distance=";
  ctx += (s.distance < 0) ? String("N/A") : String(s.distance, 1) + "cm";
  ctx += ", light=" + String(s.light);
  ctx += ", sound=" + String(s.soundP2P);
  ctx += ", state=" + String(warningDesc(s.warningState));
  return ctx;
}

String askGemini(const DeskState &state, const String &question) {
  if (!wifiIsConnected()) {
    setGeminiError("Wi-Fi is disconnected.");
    return "";
  }

  const uint32_t now = millis();
  if (backoffUntilMs != 0 && now < backoffUntilMs) {
    const unsigned long remainingMs = (unsigned long)(backoffUntilMs - now);
    Serial.printf("[Gemini] In 429 backoff for %lu more ms — skipping.\n",
                  remainingMs);
    setGeminiError(String("Gemini is cooling down after a 429 response. Try again in ") +
                   String((remainingMs + 999UL) / 1000UL) + " s.");
    return "";
  }
  if (lastCallMs != 0 && (now - lastCallMs) < GEMINI_MIN_INTERVAL_MS) {
    Serial.println("[Gemini] Rate-limited — skipping.");
    const unsigned long remainingMs =
      (unsigned long)(GEMINI_MIN_INTERVAL_MS - (now - lastCallMs));
    setGeminiError(String("Local cooldown active. Try again in ") +
                   String((remainingMs + 999UL) / 1000UL) + " s.");
    return "";
  }
  lastCallMs = now;
  setGeminiError("");

  // ---- Build Gemini request body ----
  JsonDocument payload;

  JsonObject sys = payload["systemInstruction"].to<JsonObject>();
  JsonArray sysParts = sys["parts"].to<JsonArray>();
  JsonObject sysPart = sysParts.add<JsonObject>();
  sysPart["text"] =
    "You are a concise study-environment assistant for a smart desk. "
    "Give actionable, friendly advice in 2-3 sentences. "
    "Focus on temperature, posture (distance to screen), light, and noise.";

  JsonArray contents = payload["contents"].to<JsonArray>();
  JsonObject turn = contents.add<JsonObject>();
  turn["role"] = "user";
  JsonArray parts = turn["parts"].to<JsonArray>();
  JsonObject part = parts.add<JsonObject>();
  String userText = buildContext(state);
  userText += ". Question: ";
  userText += question;
  part["text"] = userText;

  JsonObject gen = payload["generationConfig"].to<JsonObject>();
  gen["maxOutputTokens"] = GEMINI_MAX_TOKENS;
  gen["temperature"]     = 0.5;

  String body;
  serializeJson(payload, body);

  // ---- Send ----
  String url = "https://generativelanguage.googleapis.com/v1beta/models/";
  url += GEMINI_MODEL;
  url += ":generateContent?key=";
  url += GEMINI_API_KEY;

  int code = 0;
  String resp = "";
  for (uint8_t attempt = 0; attempt <= GEMINI_MAX_RETRIES; ++attempt) {
    HTTPClient http;
    http.setTimeout(GEMINI_HTTP_TIMEOUT_MS);
    http.setConnectTimeout(GEMINI_HTTP_TIMEOUT_MS);

    if (!http.begin(aiClient, url)) {
      setGeminiError("Could not start HTTPS request to Gemini.");
      return "";
    }
    http.addHeader("Content-Type", "application/json");

    code = http.POST(body);
    resp = http.getString();
    http.end();

    if (!isTransientHttpError(code) || attempt == GEMINI_MAX_RETRIES) {
      break;
    }

    Serial.printf("[Gemini] Transient HTTP error %d (%s), retrying.\n",
                  code, httpErrorName(code));
    delay(GEMINI_RETRY_DELAY_MS);
  }

  if (code != 200) {
    Serial.printf("[Gemini] HTTP error: %d\n", code);
    String reason = "";
    JsonDocument errDoc;
    const DeserializationError errParse = deserializeJson(errDoc, resp);
    if (!errParse) {
      String status = errDoc["error"]["status"] | "";
      String message = errDoc["error"]["message"] | "";
      if (status.length() > 0) {
        reason += status;
      }
      if (message.length() > 0) {
        if (reason.length() > 0) reason += ": ";
        reason += message;
      }
    }
    if (reason.length() == 0) {
      const char *errorName = httpErrorName(code);
      if (strlen(errorName) > 0) {
        reason = errorName;
      } else {
        reason = "HTTP " + String(code);
      }
    }
    String failure = "Gemini returned ";
    failure += String(code);
    failure += ". ";
    failure += reason;
    setGeminiError(failure);
    if (resp.length() > 0) {
      if (resp.length() > 500) {
        resp = resp.substring(0, 500);
      }
      Serial.printf("[Gemini] Error body: %s\n", resp.c_str());
    }
    if (code == 429) {
      backoffUntilMs = millis() + GEMINI_BACKOFF_ON_429_MS;
      Serial.printf("[Gemini] Backing off for %u ms due to 429.\n",
                    GEMINI_BACKOFF_ON_429_MS);
    }
    return "";
  }

  // ---- Parse response ----
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, resp);
  if (err) {
    Serial.printf("[Gemini] JSON parse error: %s\n", err.c_str());
    setGeminiError("Gemini replied, but the response JSON could not be parsed.");
    return "";
  }

  const char *reply =
    doc["candidates"][0]["content"]["parts"][0]["text"] | "";
  String out = String(reply);
  out.trim();
  if (out.length() == 0) {
    setGeminiError("Gemini returned an empty answer.");
  }
  return out;
}

String askGeminiForAdvice(const DeskState &state) {
  return askGemini(state,
    "Based on these readings, what should I adjust right now to stay "
    "comfortable and focused? Keep it short.");
}
