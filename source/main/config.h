#pragma once
#include <Arduino.h>
#include <stdint.h>

#define BUZZER_PIN  9
#define DHTPIN      4
#define DHTTYPE     DHT11
#define TRIG_PIN    5
#define ECHO_PIN    18
#define RX1_PIN     2
#define TX1_PIN     1
#define UART_LINK_BAUD_RATE 9600UL

#define TEMP_HIGH_THRESHOLD_C   25.0f
#define DIST_CLOSE_THRESHOLD_CM 15.0f
#define LIGHT_DARK_THRESHOLD    100U
#define LIGHT_BRIGHT_THRESHOLD  210U
#define SOUND_THRESHOLD         9U

#define WARNING_STATE_IDLE          0U
#define WARNING_STATE_ACKNOWLEDGED  1U
#define WARNING_STATE_YELLOW        2U
#define WARNING_STATE_RED           3U
#define WARNING_STATE_RED_BUZZER    4U

#define LOOP_DELAY_MS 500U

struct DeskState {
  float    temp;
  float    humidity;
  float    distance;
  uint16_t light;
  uint16_t soundP2P;
  bool     systemActive;
  uint8_t  activeCount;
  uint8_t  warningState;
  bool     warningSuppressed;
};

