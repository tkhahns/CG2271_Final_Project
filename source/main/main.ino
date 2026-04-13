#include <Arduino.h>
#include "config.h"
#include "uart_receive.h"
#include "sensors.h"
#include "wifi_manager.h"
#include "firebase_client.h"
#include "telegram_bot.h"
#include "openai_client.h"
#include "secrets.h"

DeskState espDesk = {NAN, NAN, -1.0f, 0, 0, false, 0, WARNING_STATE_IDLE, false};

static uint32_t lastEspPrintMs   = 0;
static uint32_t lastFirebaseMs   = 0;
static uint32_t lastTelegramMs   = 0;
static uint8_t  lastWarningState = WARNING_STATE_IDLE;

static void applyBuzzer(bool enabled) {
  digitalWrite(BUZZER_PIN, enabled ? HIGH : LOW);
}

static void printEspSensors() {
  Serial.print("TEMP=");
  if (isnan(espDesk.temp)) Serial.print("ERR");
  else                     Serial.print(espDesk.temp, 1);

  Serial.print(" | DIST=");
  if (espDesk.distance < 0) Serial.print("ERR");
  else                      Serial.print(espDesk.distance, 2);

  Serial.print(" | LIGHT=");    Serial.print(espDesk.light);
  Serial.print(" | SOUND=");    Serial.print(espDesk.soundP2P);
  Serial.print(" | STARTED=");  Serial.print(espDesk.systemActive ? 1 : 0);
  Serial.print(" | CNT=");      Serial.print(espDesk.activeCount);
  Serial.print(" | SUPP=");     Serial.println(espDesk.warningSuppressed ? 1 : 0);
}

// ---- Cloud handlers ----

static void handleTelegramCommand(const TgResult &r) {
  switch (r.command) {
    case CMD_START:
      sendTelegramMessage(
        "👋 Smart Study Monitor online.\n"
        "Commands:\n/status — current readings\n"
        "/settemp <C> — temp threshold\n/setdist <cm> — distance threshold\n"
        "/ask <question> — ask AI about your desk\n/help");
      break;

    case CMD_STATUS:
      sendTelegramMessage(formatStatus(espDesk));
      break;

    case CMD_SETTEMP:
      sendTelegramMessage("✅ Temp threshold received: " + String(r.value, 1) + " °C");
      // Threshold changes flow back to MCXC via UART (future extension)
      break;

    case CMD_SETDIST:
      sendTelegramMessage("✅ Distance threshold received: " + String(r.value, 1) + " cm");
      break;

    case CMD_HELP:
      sendTelegramMessage(
        "Commands:\n/status\n/settemp <C>\n/setdist <cm>\n/ask <question>");
      break;

    case CMD_ASK: {
      sendTelegramMessage("🤔 Thinking...");
      String answer = askOpenAI(espDesk, r.text);
      if (answer.length() == 0) answer = "(AI unavailable or rate-limited — try again shortly)";
      sendTelegramMessage("🤖 " + answer);
      break;
    }

    default:
      sendTelegramMessage("Unknown command. Send /help for options.");
      break;
  }
}

static void runCloudLoop() {
  wifiEnsureUp();
  if (!wifiIsConnected()) return;

  const uint32_t now = millis();

  // 1. Periodic Firebase log
  if (now - lastFirebaseMs >= FIREBASE_LOG_INTERVAL_MS) {
    lastFirebaseMs = now;
    logToFirebase(espDesk);
  }

  // 2. Poll Telegram for incoming commands
  if (now - lastTelegramMs >= TELEGRAM_POLL_INTERVAL_MS) {
    lastTelegramMs = now;
    TgResult r = pollTelegram();
    if (r.received) handleTelegramCommand(r);
  }

  // 3. Rising-edge alert on warning state transitions into RED / RED_BUZZER
  if (espDesk.warningState != lastWarningState) {
    const bool escalated =
      (espDesk.warningState >= WARNING_STATE_RED) &&
      (lastWarningState      <  WARNING_STATE_RED);

    if (escalated && !espDesk.warningSuppressed) {
      sendTelegramAlert(espDesk);
      // AI advice on critical events (rate-limited inside askOpenAI)
      String tip = askOpenAIForAdvice(espDesk);
      if (tip.length() > 0) sendTelegramMessage("🤖 " + tip);
    }
    lastWarningState = espDesk.warningState;
  }
}

void setup() {
  Serial.begin(9600);
  delay(300);

  uartReceiveInit();
  sensorsInit();
  applyBuzzer(false);

  wifiInit();
  initFirebase();
  initTelegram();
  initOpenAI();

  Serial.println("ESP32 warning bridge + cloud ready");
}

void loop() {
  uartReceiveLoop(espDesk);
  sensorsRead(espDesk);
  uartSendEspSensors(espDesk);

  const bool buzzerOn = espDesk.systemActive &&
                        !espDesk.warningSuppressed &&
                        (espDesk.warningState >= WARNING_STATE_RED_BUZZER);
  applyBuzzer(buzzerOn);

  if (millis() - lastEspPrintMs >= 1000UL) {
    lastEspPrintMs = millis();
    if (espDesk.systemActive) printEspSensors();
  }

  runCloudLoop();

  delay(LOOP_DELAY_MS);
}
