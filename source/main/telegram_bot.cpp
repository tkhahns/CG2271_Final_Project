#include "telegram_bot.h"
#include "secrets.h"
#include "wifi_manager.h"
#include "time_util.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

static WiFiClientSecure tgClient;
static long lastUpdateId = 0;

static const char *warningText(uint8_t s) {
  switch (s) {
    case WARNING_STATE_IDLE:         return "Normal";
    case WARNING_STATE_GREEN:        return "Green";
    case WARNING_STATE_YELLOW:       return "Yellow";
    case WARNING_STATE_RED:          return "Critical";
    case WARNING_STATE_RED_BUZZER:   return "Critical + Buzzer";
    default:                         return "Unknown";
  }
}

void initTelegram() {
  tgClient.setInsecure();  // Telegram uses valid certs but we skip pinning for simplicity
  lastUpdateId = 0;
  Serial.println("[Telegram] Client ready.");
}

String formatStatus(const DeskState &s) {
  String msg = "📊 Desk Status\n";
  msg += "🕒 " + currentIsoString() + "\n";

  msg += "🌡 Temp: ";
  msg += isnan(s.temp) ? String("ERR") : String(s.temp, 1) + " °C";
  msg += "\n💧 Humidity: ";
  msg += isnan(s.humidity) ? String("ERR") : String(s.humidity, 1) + " %";
  msg += "\n📏 Distance: ";
  msg += (s.distance < 0) ? String("ERR") : String(s.distance, 1) + " cm";
  msg += "\n💡 Light: " + String(s.light);
  msg += "\n🔊 SoundP2P: " + String(s.soundP2P);
  msg += "\n▶ Active: " + String(s.systemActive ? "YES" : "NO");
  msg += "\n#: " + String(s.activeCount);
  msg += "\n⚠ State: " + String(warningText(s.warningState));
  if (s.warningSuppressed) msg += " (suppressed)";
  return msg;
}

bool sendTelegramMessage(const String &text) {
  if (!wifiIsConnected()) return false;

  JsonDocument payload;
  payload["chat_id"] = TELEGRAM_CHAT_ID;
  payload["text"]    = text;
  String body;
  serializeJson(payload, body);

  HTTPClient http;
  const String url = "https://api.telegram.org/bot" +
                     String(TELEGRAM_BOT_TOKEN) + "/sendMessage";
  http.begin(tgClient, url);
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(body);
  http.end();

  if (code != 200) {
    Serial.printf("[Telegram] sendMessage error: %d\n", code);
    return false;
  }
  return true;
}

bool sendTelegramAlert(const DeskState &state) {
  String msg = "🚨 ALERT — ";
  msg += warningText(state.warningState);
  msg += "\n🕒 " + currentIsoString();
  msg += "\n\n";
  msg += formatStatus(state);
  return sendTelegramMessage(msg);
}

static TgCmd parseCommand(const String &text, float &value, String &asked) {
  value = NAN;
  asked = "";

  String t = text;
  t.trim();
  if (t.length() == 0) return CMD_NONE;

  // Lowercase first token
  int sp = t.indexOf(' ');
  String head = (sp < 0) ? t : t.substring(0, sp);
  String tail = (sp < 0) ? ""   : t.substring(sp + 1);
  head.toLowerCase();
  tail.trim();

  if (head == "/start")           return CMD_START;
  if (head == "/status")          return CMD_STATUS;
  if (head == "/help")            return CMD_HELP;
  if (head == "/clear")           return CMD_CLEAR;

  if (head == "/settemp" && tail.length() > 0) {
    value = tail.toFloat();
    return CMD_SETTEMP;
  }
  if (head == "/setdist" && tail.length() > 0) {
    value = tail.toFloat();
    return CMD_SETDIST;
  }
  if (head == "/ask" && tail.length() > 0) {
    asked = tail;
    return CMD_ASK;
  }

  return CMD_NONE;
}

TgResult pollTelegram() {
  TgResult r;
  r.received = false;
  r.command  = CMD_NONE;
  r.value    = NAN;
  r.text     = "";

  if (!wifiIsConnected()) return r;

  HTTPClient http;
  // limit=10 drains short backlogs in one poll cycle so /ask doesn't get
  // stuck behind older /start messages.
  String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) +
               "/getUpdates?timeout=1&limit=10";
  if (lastUpdateId > 0) {
    url += "&offset=" + String(lastUpdateId + 1);
  }

  http.begin(tgClient, url);
  const int code = http.GET();
  if (code != 200) {
    Serial.printf("[Telegram] getUpdates HTTP error: %d\n", code);
    http.end();
    return r;
  }

  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[Telegram] JSON parse error: %s\n", err.c_str());
    return r;
  }

  JsonArray results = doc["result"].as<JsonArray>();
  if (results.size() == 0) return r;

  // Walk the batch: always advance offset past every update, but only
  // act on the most recent message from the authorized chat.
  for (JsonObject update : results) {
    const long uid = update["update_id"].as<long>();
    if (uid > lastUpdateId) lastUpdateId = uid;

    // chat.id is a JSON number, not a string — read as int64 and stringify.
    const int64_t chatId = update["message"]["chat"]["id"].as<int64_t>();
    const String  chatStr = String((long long)chatId);
    const char   *text    = update["message"]["text"] | "";

    Serial.printf("[Telegram] update_id=%ld chat=%s text=\"%s\"\n",
                  uid, chatStr.c_str(), text);

    if (chatStr != String(TELEGRAM_CHAT_ID)) {
      Serial.printf("[Telegram] Ignored — chat %s != expected %s\n",
                    chatStr.c_str(), TELEGRAM_CHAT_ID);
      continue;
    }

    // Parse + remember; later iterations overwrite, so we end up with
    // the latest valid command in the batch.
    String tmpText;
    float  tmpValue;
    TgCmd  cmd = parseCommand(String(text), tmpValue, tmpText);
    if (cmd != CMD_NONE || strlen(text) > 0) {
      r.received = true;
      r.command  = cmd;
      r.value    = tmpValue;
      r.text     = tmpText;
    }
  }

  return r;
}
