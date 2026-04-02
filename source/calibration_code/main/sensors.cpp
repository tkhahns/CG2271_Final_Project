#include <Arduino.h>
#include "sensors.h"
#include <DHT.h>

static DHT dht(DHTPIN, DHTTYPE);

// ── Init ──────────────────────────────────────────────────────────
void sensorsInit() {
  dht.begin();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
}

// ── Private: Ultrasonic ───────────────────────────────────────────
static float readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  float duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1.0f;
  return duration * 0.0343f / 2.0f;
}

// ── Public: Read all local sensors ────────────────────────────────
void sensorsRead(DeskState &state) {
  state.temp     = dht.readTemperature();
  state.humidity = dht.readHumidity();
  state.distance = readUltrasonic();
}

// ── Public: Evaluate desk state ───────────────────────────────────
uint8_t evaluateState(const DeskState &state) {
  if (state.temp     > TEMP_HI
      || state.distance < DIST_LOW
      || state.humidity > HUMID_HI
      || state.light    < LIGHT_LOW
      || state.noise) {
    return CRITICAL;
  }
  if (state.temp     < TEMP_LOW
      || state.distance > DIST_HI
      || state.humidity < HUMID_LOW
      || state.light    > LIGHT_HI) {
    return NORMAL;
  }
  return WARNING;
}

// ── Debug Helpers ─────────────────────────────────────────────────
void printDistance(float distance) {
  Serial.print("DIST_CM=");
  if (distance < 0) Serial.print("ERR");
  else Serial.print(distance, 2);
}

void printHumidity(float humidity) {
  Serial.print(" | HUMIDITY=");
  if (isnan(humidity)) Serial.print("ERR");
  else { Serial.print(humidity, 1); Serial.print("%"); }
}

void printTemperature(float temperature) {
  Serial.print(" | TEMP_C=");
  if (isnan(temperature)) Serial.print("ERR");
  else { Serial.print(temperature, 1); Serial.print("C"); }
}