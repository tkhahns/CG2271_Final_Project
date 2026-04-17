#include <Arduino.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "uart_receive.h"
#include "sensors.h"

namespace {

constexpr size_t kFrameBufferSize = 96;
constexpr size_t kSuggestionPayloadMax = 72;
char g_frameBuffer[kFrameBufferSize];
size_t g_frameIndex = 0;
bool g_haveMcxcFrame = false;

static uint8_t frameChecksum(const char *frameBody) {
  uint8_t checksum = 0U;
  const char *cursor = (frameBody[0] == '$') ? (frameBody + 1) : frameBody;
  while (*cursor != '\0') {
    checksum ^= static_cast<uint8_t>(*cursor);
    ++cursor;
  }
  return checksum;
}

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  return -1;
}

static bool stripAndValidateChecksum(const char *frame,
                                     char *payload,
                                     size_t payloadSize,
                                     bool *hadChecksum) {
  strncpy(payload, frame, payloadSize - 1U);
  payload[payloadSize - 1U] = '\0';

  char *separator = strrchr(payload, '*');
  if (separator == nullptr) {
    *hadChecksum = false;
    return true;
  }

  if ((separator[1] == '\0') || (separator[2] == '\0') || (separator[3] != '\0')) {
    *hadChecksum = true;
    return false;
  }

  const int high = hexNibble(separator[1]);
  const int low = hexNibble(separator[2]);
  *hadChecksum = true;
  if (high < 0 || low < 0) {
    return false;
  }

  *separator = '\0';
  const uint8_t expected = static_cast<uint8_t>((high << 4) | low);
  return frameChecksum(payload) == expected;
}

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
  char payload[kFrameBufferSize];
  bool hadChecksum = false;
  if (!stripAndValidateChecksum(frame, payload, sizeof(payload), &hadChecksum)) {
    if (hadChecksum) {
      Serial.println("CHECKSUM ERROR");
    }
    return;
  }

  unsigned int light = 0;
  unsigned int sound = 0;
  unsigned int started = 0;
  unsigned int suppressed = 0;

  const int parsed = sscanf(payload, "$MCXC,%u,%u,%u,%u",
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

  char body[48];
  snprintf(body,
           sizeof(body),
           "$ESP,%u,%d,%d",
           static_cast<unsigned int>(activeCount),
           tempDeci,
           distDeci);

  char frame[56];
  snprintf(frame,
           sizeof(frame),
           "%s*%02X\r\n",
           body,
           static_cast<unsigned int>(frameChecksum(body)));
  Serial1.print(frame);
}

void uartSendSuggestion(const String &suggestion) {

  char payload[kSuggestionPayloadMax + 1U];
  size_t out = 0U;
  for (size_t i = 0; i < static_cast<size_t>(suggestion.length()) && out < kSuggestionPayloadMax; ++i) {
    const char c = suggestion[i];
    if (c == '\r' || c == '\n' || c == '*') {
      payload[out++] = ' ';
    } else {
      payload[out++] = c;
    }
  }
  payload[out] = '\0';

  char body[kSuggestionPayloadMax + 8U];
  snprintf(body, sizeof(body), "$SUG,%s", payload);

  char frame[kSuggestionPayloadMax + 16U];
  snprintf(frame,
           sizeof(frame),
           "%s*%02X\r\n",
           body,
           static_cast<unsigned int>(frameChecksum(body)));
  Serial1.print(frame);
}
