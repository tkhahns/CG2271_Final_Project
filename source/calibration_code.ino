#include <DHT.h>

#define TRIG_PIN 5
#define ECHO_PIN 18

#define DHTPIN 4
#define DHTTYPE DHT11

#define MIC_PIN 13   // microphone A0 -> GPIO34

DHT dht(DHTPIN, DHTTYPE);

float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return -1.0;
  }

  return duration * 0.0343 / 2.0;
}

void readMicWindow(int &micRaw, int &micMin, int &micMax, int &micP2P) {
  micMin = 4095;
  micMax = 0;

  for (int i = 0; i < 200; i++) {
    int v = analogRead(MIC_PIN);

    if (v < micMin) micMin = v;
    if (v > micMax) micMax = v;

    delayMicroseconds(500);
  }

  micRaw = analogRead(MIC_PIN);
  micP2P = micMax - micMin;
}

void setup() {
  Serial.begin(9600);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  analogReadResolution(12);   // ESP32 ADC: 0 to 4095

  dht.begin();

  Serial.println("ESP32 Ultrasonic + DHT + Microphone Test Start");
}

void loop() {
  float distance = readDistanceCm();
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  int micRaw, micMin, micMax, micP2P;
  readMicWindow(micRaw, micMin, micMax, micP2P);

  Serial.print("DIST_CM=");
  if (distance < 0) {
    Serial.print("ERR");
  } else {
    Serial.print(distance, 2);
  }

  Serial.print(" | HUMIDITY=");
  if (isnan(humidity)) {
    Serial.print("ERR");
  } else {
    Serial.print(humidity, 1);
    Serial.print("%");
  }

  Serial.print(" | TEMP_C=");
  if (isnan(temperature)) {
    Serial.print("ERR");
  } else {
    Serial.print(temperature, 1);
    Serial.print("C");
  }

  Serial.print(" | MIC_RAW=");
  Serial.print(micRaw);

  Serial.print(" | MIC_MIN=");
  Serial.print(micMin);

  Serial.print(" | MIC_MAX=");
  Serial.print(micMax);

  Serial.print(" | MIC_P2P=");
  Serial.println(micP2P);

  delay(1000);
}