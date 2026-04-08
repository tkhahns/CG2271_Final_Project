#include <Arduino.h>
#include "uart_receive.h"
#include "sensors.h"

DeskState espDesk = {NAN, NAN, -1.0f, 0, 0, false, 0, WARNING_STATE_IDLE, false};
uint32_t lastEspPrintMs = 0;

static void applyBuzzer(bool enabled) {
  digitalWrite(BUZZER_PIN, enabled ? HIGH : LOW);
}

static void printEspSensors() {
  Serial.print("TEMP=");
  if (isnan(espDesk.temp)) {
    Serial.print("ERR");
  } else {
    Serial.print(espDesk.temp, 1);
  }

  Serial.print(" | DIST=");
  if (espDesk.distance < 0) {
    Serial.print("ERR");
  } else {
    Serial.print(espDesk.distance, 2);
  }

  Serial.print(" | LIGHT=");
  Serial.print(espDesk.light);
  Serial.print(" | SOUND=");
  Serial.print(espDesk.soundP2P);
  Serial.print(" | STARTED=");
  Serial.print(espDesk.systemActive ? 1 : 0);
  Serial.print(" | CNT=");
  Serial.print(espDesk.activeCount);
  Serial.print(" | SUPP=");
  Serial.println(espDesk.warningSuppressed ? 1 : 0);
}

void setup() {
  Serial.begin(9600);
  delay(300);

  uartReceiveInit();
  sensorsInit();
  applyBuzzer(false);

  Serial.println("ESP32 warning bridge ready");
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
    if (espDesk.systemActive) {
      printEspSensors();
    }
  }

  delay(LOOP_DELAY_MS);
}
