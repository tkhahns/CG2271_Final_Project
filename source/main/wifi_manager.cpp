#include "wifi_manager.h"
#include "secrets.h"
#include <WiFi.h>

static uint32_t lastReconnectAttempt = 0;
static const uint32_t RECONNECT_COOLDOWN_MS = 5000;

void wifiInit() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[WiFi] Connecting to ");
  Serial.print(WIFI_SSID);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_MAX_RETRIES) {
    delay(WIFI_RETRY_MS);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] Connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] Connection FAILED — cloud features disabled.");
  }
}

bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void wifiEnsureUp() {
  if (wifiIsConnected()) return;

  const uint32_t now = millis();
  if (now - lastReconnectAttempt < RECONNECT_COOLDOWN_MS) return;
  lastReconnectAttempt = now;

  Serial.println("[WiFi] Lost connection. Reconnecting...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}
