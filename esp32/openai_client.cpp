#include "openai_client.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static WiFiClientSecure openaiClient;
static unsigned long lastCallTime = 0;

static const char* warningLevelStr(int level) {
    switch (level) {
        case 0:  return "Normal";
        case 1:  return "Warning";
        case 2:  return "Critical";
        default: return "Unknown";
    }
}

void initOpenAI() {
    openaiClient.setInsecure();
    Serial.println("[OpenAI] Client ready.");
}

// Build a prompt from sensor data and call the Chat Completions API.
// Returns a one-sentence study advisory string.
// Rate-limited to avoid excessive API calls.
String getAdvisory(const SensorData_t& data) {
    if (WiFi.status() != WL_CONNECTED) {
        return "Wi-Fi not connected.";
    }

    // Rate limiting: minimum interval between calls
    unsigned long now = millis();
    if (lastCallTime > 0 && (now - lastCallTime) < OPENAI_MIN_INTERVAL_MS) {
        Serial.println("[OpenAI] Rate limited, skipping call.");
        return "Advisory unavailable (rate limited).";
    }

    Serial.println("[OpenAI] Requesting advisory...");

    HTTPClient http;
    http.begin(openaiClient, "https://api.openai.com/v1/chat/completions");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));
    http.setTimeout(20000);

    // Build the user message with current sensor readings
    String userMsg = "Current study desk sensor readings: ";
    userMsg += "Temperature = " + String(data.temperature, 1) + " C, ";
    userMsg += "Humidity = " + String(data.humidity, 1) + " %, ";
    userMsg += "Ambient Light = " + String(data.light) + " (higher = brighter), ";
    userMsg += "Noise = " + String(data.noise ? "noisy environment detected" : "quiet") + ", ";
    userMsg += "Sitting Distance = " + String(data.distance, 1) + " cm from screen. ";
    userMsg += "Overall status: " + String(warningLevelStr(data.warningLevel)) + ".";

    // Build the JSON request body using ArduinoJson
    JsonDocument reqDoc;
    reqDoc["model"] = OPENAI_MODEL;
    reqDoc["max_tokens"] = OPENAI_MAX_TOKENS;
    reqDoc["temperature"] = 0.7;

    JsonArray messages = reqDoc["messages"].to<JsonArray>();

    JsonObject sysMsg = messages.add<JsonObject>();
    sysMsg["role"] = "system";
    sysMsg["content"] = "You are a study environment advisor for a university student "
                        "in Singapore. Given the sensor readings from their study desk, "
                        "respond with exactly ONE short sentence of actionable advice to "
                        "improve their study conditions. Be specific and practical. "
                        "Do not repeat the sensor values.";

    JsonObject usrMsg = messages.add<JsonObject>();
    usrMsg["role"] = "user";
    usrMsg["content"] = userMsg;

    String requestBody;
    serializeJson(reqDoc, requestBody);

    int code = http.POST(requestBody);
    lastCallTime = millis();

    String advisory = "Unable to get advisory.";

    if (code == 200) {
        String response = http.getString();

        JsonDocument resDoc;
        DeserializationError err = deserializeJson(resDoc, response);

        if (!err) {
            const char* content = resDoc["choices"][0]["message"]["content"];
            if (content) {
                advisory = String(content);
                advisory.trim();
                Serial.print("[OpenAI] Advisory: ");
                Serial.println(advisory);
            }
        } else {
            Serial.print("[OpenAI] JSON parse error: ");
            Serial.println(err.c_str());
        }
    } else {
        Serial.print("[OpenAI] HTTP error: ");
        Serial.println(code);
        if (code > 0) {
            String errBody = http.getString();
            Serial.println(errBody.substring(0, 200));
        }
    }

    http.end();
    return advisory;
}
