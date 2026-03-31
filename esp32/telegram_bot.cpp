#include "telegram_bot.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static WiFiClientSecure telegramClient;
static long lastUpdateId = 0;

// Constructed in initTelegram() to avoid global String construction issues
static String botUrl;

static const char* warningLevelStr(int level) {
    switch (level) {
        case 0:  return "Normal";
        case 1:  return "Warning";
        case 2:  return "Critical";
        default: return "Unknown";
    }
}

void initTelegram() {
    telegramClient.setInsecure();
    botUrl = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN);
    Serial.println("[Telegram] Bot ready.");
}

// Send a message to the configured chat using ArduinoJson for safe escaping
bool sendTelegramMessage(const String& text) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.begin(telegramClient, botUrl + "/sendMessage");
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["chat_id"] = TELEGRAM_CHAT_ID;
    doc["text"] = text;
    doc["parse_mode"] = "Markdown";

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);
    http.end();

    if (code == 200) {
        Serial.println("[Telegram] Message sent.");
        return true;
    } else {
        Serial.print("[Telegram] Send error: ");
        Serial.println(code);
        return false;
    }
}

// Format sensor readings as a readable Telegram message
static String formatStatus(const SensorData_t& data) {
    String s = "*Study Desk Status*\n";
    s += "Temp: "       + String(data.temperature, 1) + " C\n";
    s += "Humidity: "   + String(data.humidity, 1) + " %\n";
    s += "Light: "      + String(data.light) + "\n";
    s += "Noise: "      + String(data.noise ? "Noisy" : "Quiet") + "\n";
    s += "Distance: "   + String(data.distance, 1) + " cm\n";
    s += "Status: *"    + String(warningLevelStr(data.warningLevel)) + "*";
    return s;
}

// Send an alert message with warning emoji and advisory
bool sendTelegramAlert(const SensorData_t& data, const String& advisory) {
    String msg;

    if (data.warningLevel == 2) {
        msg = "CRITICAL ALERT\n\n";
    } else if (data.warningLevel == 1) {
        msg = "Warning\n\n";
    }

    msg += formatStatus(data);
    msg += "\n\n*Advisory:* " + advisory;

    return sendTelegramMessage(msg);
}

// Poll Telegram for incoming commands and handle them
TelegramResult_t pollTelegram(const SensorData_t& data, float& tempThreshold, float& distThreshold) {
    TelegramResult_t result = {CMD_NONE, 0, false};

    if (WiFi.status() != WL_CONNECTED) return result;

    HTTPClient http;
    String url = botUrl + "/getUpdates?offset=" + String(lastUpdateId + 1) + "&timeout=1&limit=5";
    http.begin(telegramClient, url);
    http.setTimeout(5000);
    int code = http.GET();

    if (code != 200) {
        http.end();
        return result;
    }

    String response = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, response)) return result;

    JsonArray results = doc["result"].as<JsonArray>();

    for (JsonObject update : results) {
        long updateId = update["update_id"];
        if (updateId > lastUpdateId) lastUpdateId = updateId;

        const char* textRaw = update["message"]["text"];
        if (!textRaw) continue;

        String text = String(textRaw);
        text.trim();
        if (text.length() == 0) continue;

        Serial.print("[Telegram] Command: ");
        Serial.println(text);

        // --- /start ---
        if (text == "/start") {
            result.command = CMD_START;
            result.received = true;

            sendTelegramMessage(
                "*Smart Study Monitor*\n\n"
                "Commands:\n"
                "/status - View current readings\n"
                "/settemp <C> - Set temperature threshold\n"
                "/setdist <cm> - Set distance threshold\n"
                "/help - Show this message"
            );
        }
        // --- /status ---
        else if (text == "/status") {
            result.command = CMD_STATUS;
            result.received = true;

            if (data.valid) {
                sendTelegramMessage(formatStatus(data));
            } else {
                sendTelegramMessage("No sensor data available yet.");
            }
        }
        // --- /settemp <value> ---
        else if (text.startsWith("/settemp")) {
            result.command = CMD_SETTEMP;
            result.received = true;

            String valStr = text.substring(8);
            valStr.trim();
            float val = valStr.toFloat();

            if (val > 0 && val < 60) {
                tempThreshold = val;
                result.value = val;
                sendTelegramMessage("Temperature threshold set to " + String(val, 1) + " C");
            } else {
                sendTelegramMessage("Usage: /settemp <value>\nExample: /settemp 30");
            }
        }
        // --- /setdist <value> ---
        else if (text.startsWith("/setdist")) {
            result.command = CMD_SETDIST;
            result.received = true;

            String valStr = text.substring(8);
            valStr.trim();
            float val = valStr.toFloat();

            if (val > 0 && val < 200) {
                distThreshold = val;
                result.value = val;
                sendTelegramMessage("Distance threshold set to " + String(val, 1) + " cm");
            } else {
                sendTelegramMessage("Usage: /setdist <value>\nExample: /setdist 40");
            }
        }
        // --- /help ---
        else if (text == "/help") {
            result.command = CMD_HELP;
            result.received = true;

            sendTelegramMessage(
                "*Commands:*\n"
                "/status - Current sensor readings\n"
                "/settemp <C> - Temperature threshold\n"
                "/setdist <cm> - Distance threshold\n"
                "/help - Show this message"
            );
        }
    }

    return result;
}
