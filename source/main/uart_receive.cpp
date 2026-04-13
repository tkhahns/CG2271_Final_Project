#include <Arduino.h>
#include <stdio.h>
#include <math.h>

#include "uart_receive.h"
#include "sensors.h"

namespace {

constexpr size_t kFrameBufferSize = 80;
constexpr size_t kSuggestionPayloadMax = 72;
char g_frameBuffer[kFrameBufferSize];
size_t g_frameIndex = 0;
bool g_haveMcxcFrame = false;

static uint8_t warningStateFromCount(uint8_t activeCount) {
  if (activeCount == 0U) {
    return WARNING_STATE_IDLE;
  }
  if (activeCount == 1U) {
    return WARNING_STATE_GREEN;
  }
  if (activeCount == 2U) {
    return WARNING_STATE_YELLOW;
  }
  if (activeCount == 3U) {
    return WARNING_STATE_RED;
  }
  return WARNING_STATE_RED_BUZZER;
}

static void handleFrame(const char *frame, DeskState &state) {
  unsigned int light = 0;
  unsigned int sound = 0;
  unsigned int started = 0;
  unsigned int suppressed = 0;

  const int parsed = sscanf(frame, "$MCXC,%u,%u,%u,%u",
                            &light,
                            &sound,
                            &started,
                            &suppressed);
  if (parsed == 4) {
    state.light = static_cast<uint16_t>(light);
    state.soundP2P = static_cast<uint16_t>(sound);
    state.systemActive = (started != 0U);
    state.warningSuppressed = (suppressed != 0U);
    g_haveMcxcFrame = true;
  }
}

static int encodeDeciOrInvalid(float value, bool isValid) {
  if (!isValid) {
    return -1;
  }
  return static_cast<int>(lroundf(value * 10.0f));
}

}  // namespace

void uartReceiveInit() {
  Serial1.begin(UART_LINK_BAUD_RATE, SERIAL_8N1, RX1_PIN, TX1_PIN);
  Serial.println("UART link to MCXC ready");
}

void uartReceiveLoop(DeskState &state) {
  while (Serial1.available() > 0) {
    const char incoming = static_cast<char>(Serial1.read());

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      if (g_frameIndex > 0U) {
        g_frameBuffer[g_frameIndex] = '\0';
        handleFrame(g_frameBuffer, state);
        g_frameIndex = 0U;
      }
      continue;
    }

    if (g_frameIndex < (kFrameBufferSize - 1U)) {
      g_frameBuffer[g_frameIndex++] = incoming;
    } else {
      g_frameIndex = 0U;
    }
  }
}

void uartSendEspSensors(const DeskState &state) {
  DeskState &mutableState = const_cast<DeskState &>(state);
  uint8_t activeCount = 0U;

  if (g_haveMcxcFrame && state.systemActive) {
    activeCount = breachedCount(state);
  }

  mutableState.activeCount = activeCount;
  mutableState.warningState = warningStateFromCount(activeCount);

  const int tempDeci = encodeDeciOrInvalid(state.temp, !isnan(state.temp));
  const int distDeci = encodeDeciOrInvalid(state.distance, state.distance >= 0.0f);

  char frame[48];
  snprintf(frame,
           sizeof(frame),
           "$ESP,%u,%d,%d\r\n",
           static_cast<unsigned int>(activeCount),
           tempDeci,
           distDeci);
  Serial1.print(frame);
}

void uartSendSuggestion(const String &suggestion) {
  if (suggestion.length() == 0) {
    return;
  }

  char payload[kSuggestionPayloadMax + 1U];
  size_t out = 0U;
  for (size_t i = 0; i < static_cast<size_t>(suggestion.length()) && out < kSuggestionPayloadMax; ++i) {
    const char c = suggestion[i];
    if (c == '\r' || c == '\n') {
      payload[out++] = ' ';
    } else {
      payload[out++] = c;
    }
  }
  payload[out] = '\0';

  Serial1.print("$SUG,");
  Serial1.print(payload);
  Serial1.print("\r\n");
}
