#include <Arduino.h>
#include "comms.h"

// ── Init ──────────────────────────────────────────────────────────
void commsInit() {
  Serial1.begin(9600, SERIAL_8N1, RX1_PIN, TX1_PIN);
}

// ── Private: Process a validated packet ───────────────────────────
static void processMCXCData(uint8_t id, uint8_t h, uint8_t l, DeskState &state) {
  switch (id) {
    case ID_LIGHT: {
      uint16_t value = (h << 8) | l;
      Serial.print("Light Level from MCXC: ");
      Serial.println(value);
      state.light = value;
      break;
    }
    case ID_NOISE:
      Serial.println("Noise Event Detected by MCXC!");
      state.noise = (l > 0);
      break;

    case ID_SYSTEM_CTRL:
      if (l == 1) {
        Serial.println("SYSTEM STARTED");
        state.systemActive = true;
      } else {
        Serial.println("SYSTEM STOPPED");
        state.systemActive = false;
      }
      break;

    case ID_ALERT_ACK:
      Serial.println("User acknowledged alert. Silencing for 10 seconds...");
      state.noise       = false;
      state.lastAckTime = millis();
      digitalWrite(BUZZER_PIN, LOW);
      break;

    default:
      Serial.print("Unknown Command ID received: 0x");
      Serial.println(id, HEX);
      break;
  }
}

// ── Public: Drain the UART1 receive buffer ────────────────────────
void receiveFromMCXC(DeskState &state) {
  while (Serial1.available() >= 5) {
    if (Serial1.read() != SYNC_BYTE) continue;

    uint8_t id   = Serial1.read();
    uint8_t high = Serial1.read();
    uint8_t low  = Serial1.read();
    uint8_t cs   = Serial1.read();

    if (cs == (id ^ high ^ low)) {
      processMCXCData(id, high, low, state);
    }
  }
}

// ── Public: Send RGB command ──────────────────────────────────────
void sendRGBCommand(uint8_t state) {
  uint8_t checksum = ID_RGB ^ 0x00 ^ state;
  Serial1.write(SYNC_BYTE);
  Serial1.write(ID_RGB);
  Serial1.write((uint8_t)0x00);  // High byte
  Serial1.write(state);           // Low byte
  Serial1.write(checksum);
}