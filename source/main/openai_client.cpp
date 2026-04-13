#include "openai_client.h"
#include "secrets.h"
#include "wifi_manager.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

static WiFiClientSecure aiClient;
static uint32_t lastCallMs = 0;

static const char *warningDesc(uint8_t s) {
  switch (s) {
    case WARNING_STATE_IDLE:         return "normal";
    case WARNING_STATE_ACKNOWLEDGED: return "acknowledged";
    case WARNING_STATE_YELLOW:       return "warning";
    case WARNING_STATE_RED:          return "critical";
    case WARNING_STATE_RED_BUZZER:   return "critical with buzzer";
    default:                         return "unknown";
  }
}

void initOpenAI() {
  aiClient.setInsecure();
  lastCallMs = 0;
  Serial.println("[OpenAI] Client ready.");
}

static String buildContext(const DeskState &s) {
  String ctx = "Current desk sensor readings: ";
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

String askOpenAI(const DeskState &state, const String &question) {
  if (!wifiIsConnected()) return "";

  const uint32_t now = millis();
  if (lastCallMs != 0 && (now - lastCallMs) < OPENAI_MIN_INTERVAL_MS) {
    Serial.println("[OpenAI] Rate-limited — skipping.");
    return "";
  }
  lastCallMs = now;

  JsonDocument payload;
  payload["model"] = OPENAI_MODEL;
  payload["max_tokens"] = OPENAI_MAX_TOKENS;
  payload["temperature"] = 0.5;

  JsonArray msgs = payload["messages"].to<JsonArray>();
  JsonObject sys = msgs.add<JsonObject>();
  sys["role"] = "system";
  sys["content"] =
    "You are a concise study-environment assistant for a smart desk. "
    "Give actionable, friendly advice in 2-3 sentences. "
    "Focus on temperature, posture (distance to screen), light, and noise.";

  JsonObject user = msgs.add<JsonObject>();
  user["role"] = "user";
  String content = buildContext(state);
  content += ". Question: ";
  content += question;
  user["content"] = content;

  String body;
  serializeJson(payload, body);

  HTTPClient http;
  http.begin(aiClient, "https://api.openai.com/v1/chat/completions");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));

  const int code = http.POST(body);
  if (code != 200) {
    Serial.printf("[OpenAI] HTTP error: %d\n", code);
    http.end();
    return "";
  }

  String resp = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, resp);
  if (err) {
    Serial.printf("[OpenAI] JSON parse error: %s\n", err.c_str());
    return "";
  }

  const char *reply = doc["choices"][0]["message"]["content"] | "";
  String out = String(reply);
  out.trim();
  return out;
}

String askOpenAIForAdvice(const DeskState &state) {
  return askOpenAI(state,
    "Based on these readings, what should I adjust right now to stay "
    "comfortable and focused? Keep it short.");
}
