#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "config.h"
#include "uart_receive.h"
#include "sensors.h"
#include "wifi_manager.h"
#include "firebase_client.h"
#include "telegram_bot.h"
#include "gemini_client.h"
#include "time_util.h"
#include "secrets.h"

DeskState espDesk = {NAN, NAN, -1.0f, 0, 0, false, 0, WARNING_STATE_IDLE, false};

static SemaphoreHandle_t g_espDeskMutex = nullptr;
static uint32_t lastEspPrintMs = 0;

static void applyBuzzer(bool enabled) {
  digitalWrite(BUZZER_PIN, enabled ? HIGH : LOW);
}

static DeskState readDeskStateSnapshot() {
  DeskState snapshot;

  if ((g_espDeskMutex != nullptr) &&
      (xSemaphoreTake(g_espDeskMutex, pdMS_TO_TICKS(20)) == pdTRUE)) {
    snapshot = espDesk;
    xSemaphoreGive(g_espDeskMutex);
  } else {
    snapshot = espDesk;
  }

  return snapshot;
}

static void printEspSensors(const DeskState &state) {
  Serial.print("TEMP=");
  if (isnan(state.temp)) Serial.print("ERR");
  else                   Serial.print(state.temp, 1);

  Serial.print(" | DIST=");
  if (state.distance < 0) Serial.print("ERR");
  else                    Serial.print(state.distance, 2);

  Serial.print(" | LIGHT=");    Serial.print(state.light);
  Serial.print(" | SOUND=");    Serial.print(state.soundP2P);
  Serial.print(" | STARTED=");  Serial.print(state.systemActive ? 1 : 0);
  Serial.print(" | CNT=");      Serial.print(state.activeCount);
  Serial.print(" | SUPP=");     Serial.println(state.warningSuppressed ? 1 : 0);
}

static void handleTelegramCommand(const TgResult &r, const DeskState &snapshot) {
  switch (r.command) {
    case CMD_START:
      sendTelegramMessage(
        "👋 Smart Study Monitor online.\n"
        "Commands:\n/status — current readings\n"
        "/settemp <C> — temp threshold\n/setdist <cm> — distance threshold\n"
        "/ask <question> — ask AI about your desk\n/help");
      break;

    case CMD_STATUS:
      sendTelegramMessage(formatStatus(snapshot));
      break;

    case CMD_SETTEMP:
      sendTelegramMessage("✅ Temp threshold received: " + String(r.value, 1) + " °C");
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
      String answer = askGemini(snapshot, r.text);
      if (answer.length() == 0) answer = "(AI unavailable or rate-limited — try again shortly)";
      sendTelegramMessage("🤖 " + answer);
      
      // ✅ AI text routed to display
      uartSendSuggestion(answer); 
      break;
    }

    default:
      sendTelegramMessage("Unknown command. Send /help for options.");
      break;
  }
}

static void cloudTask(void *param) {
  (void)param;

  uint32_t lastFirebaseMs   = 0;
  uint32_t lastTelegramMs   = 0;
  uint32_t lastRedAlertMs   = 0;   // for periodic re-alerts while RED
  uint32_t lastRedGeminiMs  = 0;   // for periodic re-advice while RED
  uint8_t  lastWarningState = WARNING_STATE_IDLE;

  for (;;) {
    wifiEnsureUp();
    timeMaintain();

    if (wifiIsConnected()) {
      const uint32_t now = millis();
      const DeskState snapshot = readDeskStateSnapshot();
      const bool inRed =
        (snapshot.warningState >= WARNING_STATE_RED) &&
        !snapshot.warningSuppressed;

      // ---- 1. Periodic Firebase stream ----
      if (now - lastFirebaseMs >= FIREBASE_LOG_INTERVAL_MS) {
        lastFirebaseMs = now;
        logToFirebase(snapshot);
      }

      // ---- 2. Telegram command poll ----
      if (now - lastTelegramMs >= TELEGRAM_POLL_INTERVAL_MS) {
        lastTelegramMs = now;
        TgResult r = pollTelegram();
        if (r.received) {
          // ✅ Test string removed
          handleTelegramCommand(r, snapshot);
        }
      }
[4/15/2026 5:13 PM] Ethan Lam: // ---- 3. RED edge detection (transition into RED) ----
      if (snapshot.warningState != lastWarningState) {
        const bool escalated =
          (snapshot.warningState >= WARNING_STATE_RED) &&
          (lastWarningState < WARNING_STATE_RED);


        if (escalated) {
          // Force-write the moment of escalation, never wait for the cadence.
          if (logToFirebase(snapshot)) lastFirebaseMs = now;

          if (!snapshot.warningSuppressed) {
            sendTelegramAlert(snapshot);
            lastRedAlertMs = now;

            String tip = askGeminiForAdvice(snapshot);
            if (tip.length() > 0) {
              sendTelegramMessage("🤖 " + tip);
              // ✅ Alert tip routed to display
              uartSendSuggestion(tip); 
            }
            lastRedGeminiMs = now;
          }
        }
        lastWarningState = snapshot.warningState;
      }

      // ---- 4. While stuck in RED — periodic re-alert + re-advice ----
      if (inRed) {
        if (now - lastRedAlertMs >= RED_PERSIST_ALERT_INTERVAL_MS) {
          lastRedAlertMs = now;
          sendTelegramAlert(snapshot);
        }
        if (now - lastRedGeminiMs >= RED_PERSIST_GEMINI_INTERVAL_MS) {
          lastRedGeminiMs = now;
          String tip = askGeminiForAdvice(snapshot);
          if (tip.length() > 0) {
            sendTelegramMessage("🤖 " + tip);
            // ✅ Repeated alert tip routed to display
            uartSendSuggestion(tip);
          }
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(CLOUD_TASK_TICK_MS));
  }
}

void setup() {
  Serial.begin(9600);
  delay(300);

  g_espDeskMutex = xSemaphoreCreateMutex();

  uartReceiveInit();
  sensorsInit();
  applyBuzzer(false);

  wifiInit();
  timeInit();
  initFirebase();
  initTelegram();
  initGemini();

  xTaskCreatePinnedToCore(cloudTask,
                          "cloud",
                          8192,
                          nullptr,
                          1,
                          nullptr,
                          0);

  Serial.println("ESP32 warning bridge + cloud ready");
}

void loop() {
  if ((g_espDeskMutex != nullptr) &&
      (xSemaphoreTake(g_espDeskMutex, pdMS_TO_TICKS(20)) == pdTRUE)) {
    uartReceiveLoop(espDesk);
    sensorsRead(espDesk);
    uartSendEspSensors(espDesk);
    xSemaphoreGive(g_espDeskMutex);
  } else {
    uartReceiveLoop(espDesk);
    sensorsRead(espDesk);
    uartSendEspSensors(espDesk);
  }

  const DeskState snapshot = readDeskStateSnapshot();
  const bool buzzerOn = snapshot.systemActive &&
                        !snapshot.warningSuppressed &&
                        (snapshot.warningState >= WARNING_STATE_RED_BUZZER);
  applyBuzzer(buzzerOn);

  if ((millis() - lastEspPrintMs >= 1000UL) && snapshot.systemActive) {
    lastEspPrintMs = millis();
    printEspSensors(snapshot);
  }

  delay(50);
}