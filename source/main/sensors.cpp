#include <Arduino.h>
#include "sensors.h"
#include <DHT.h>

static DHT dht(DHTPIN, DHTTYPE);
static float cachedTemp = NAN;
static float cachedHumidity = NAN;
static uint32_t lastDhtReadMs = 0;
static bool lightIsDark = false;

void sensorsInit() {
  dht.begin();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
}

static float readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  const float duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) {
    return -1.0f;
  }
  return duration * 0.0343f / 2.0f;
}

void sensorsRead(DeskState &state) {
  const uint32_t now = millis();
  if ((lastDhtReadMs == 0U) || (now - lastDhtReadMs >= 2000U)) {
    const float newTemp = dht.readTemperature();
    const float newHumidity = dht.readHumidity();

    if (!isnan(newTemp)) {
      cachedTemp = newTemp;
    }
    if (!isnan(newHumidity)) {
      cachedHumidity = newHumidity;
    }

    lastDhtReadMs = now;
  }

  state.temp = cachedTemp;
  state.humidity = cachedHumidity;
  state.distance = readUltrasonic();
}

uint8_t breachedCount(const DeskState &state) {
  uint8_t count = 0;

  if (!isnan(state.temp) && state.temp >= TEMP_HIGH_THRESHOLD_C) {
    count++;
  }
  if (state.distance >= 0.0f && state.distance <= DIST_CLOSE_THRESHOLD_CM) {
    count++;
  }

  if (state.light <= LIGHT_DARK_THRESHOLD) {
    lightIsDark = true;
  } else if (state.light >= LIGHT_BRIGHT_THRESHOLD) {
    lightIsDark = false;
  }
  if (lightIsDark) {
    count++;
  }

  if (state.soundP2P >= SOUND_THRESHOLD) {
    count++;
  }

  return count;
}

uint8_t evaluateState(const DeskState &state) {
  const uint8_t activeCount = breachedCount(state);

  if (activeCount == 0U) {
    return WARNING_STATE_IDLE;
  }
  if (activeCount == 1U) {
    return WARNING_STATE_YELLOW;
  }
  if (activeCount == 2U) {
    return WARNING_STATE_RED;
  }
  return WARNING_STATE_RED_BUZZER;
}

void printDistance(float distance) {
  Serial.print("DIST_CM=");
  if (distance < 0) {
    Serial.print("ERR");
  } else {
    Serial.print(distance, 2);
  }
}

void printHumidity(float humidity) {
  Serial.print(" | HUMIDITY=");
  if (isnan(humidity)) {
    Serial.print("ERR");
  } else {
    Serial.print(humidity, 1);
    Serial.print("%");
  }
}

void printTemperature(float temperature) {
  Serial.print(" | TEMP_C=");
  if (isnan(temperature)) {
    Serial.print("ERR");
  } else {
    Serial.print(temperature, 1);
    Serial.print("C");
  }
}
