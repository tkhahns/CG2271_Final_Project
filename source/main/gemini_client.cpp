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
  if (!wifiIsConnected()) return "";

  const uint32_t now = millis();
  if (backoffUntilMs != 0 && now < backoffUntilMs) {
    Serial.printf("[Gemini] In 429 backoff for %lu more ms — skipping.\n",
                  (unsigned long)(backoffUntilMs - now));
    return "";
  }
  if (lastCallMs != 0 && (now - lastCallMs) < GEMINI_MIN_INTERVAL_MS) {
    Serial.println("[Gemini] Rate-limited — skipping.");
    return "";
  }
  lastCallMs = now;

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
  HTTPClient http;
  String url = "https://generativelanguage.googleapis.com/v1beta/models/";
  url += GEMINI_MODEL;
  url += ":generateContent?key=";
  url += GEMINI_API_KEY;

  http.begin(aiClient, url);
  http.addHeader("Content-Type", "application/json");

  const int code = http.POST(body);
  if (code != 200) {
    Serial.printf("[Gemini] HTTP error: %d\n", code);
    if (code == 429) {
      backoffUntilMs = millis() + GEMINI_BACKOFF_ON_429_MS;
      Serial.printf("[Gemini] Backing off for %u ms due to 429.\n",
                    GEMINI_BACKOFF_ON_429_MS);
    }
    http.end();
    return "";
  }

  String resp = http.getString();
  http.end();

  // ---- Parse response ----
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, resp);
  if (err) {
    Serial.printf("[Gemini] JSON parse error: %s\n", err.c_str());
    return "";
  }

  const char *reply =
    doc["candidates"][0]["content"]["parts"][0]["text"] | "";
  String out = String(reply);
  out.trim();
  return out;
}

String askGeminiForAdvice(const DeskState &state) {
  return askGemini(state,
    "Based on these readings, what should I adjust right now to stay "
    "comfortable and focused? Keep it short.");
}
