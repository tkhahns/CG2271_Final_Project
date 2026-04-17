#include "wifi_manager.h"
#include "secrets.h"
#include <WiFi.h>

static uint32_t lastReconnectAttempt = 0;
static const uint32_t RECONNECT_COOLDOWN_MS = 10000;
static wl_status_t lastPrintedStatus = WL_IDLE_STATUS;

static const char *wifiStatusName(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:      return "IDLE";
    case WL_NO_SSID_AVAIL:   return "NO_SSID";
    case WL_SCAN_COMPLETED:   return "SCAN_DONE";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:   return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:  return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

void wifiInit() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);

  WiFi.disconnect(false, false);
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
    lastPrintedStatus = WL_CONNECTED;
  } else {
    lastPrintedStatus = WiFi.status();
    Serial.print("[WiFi] Connection FAILED. Status: ");
    Serial.println(wifiStatusName(lastPrintedStatus));
  }
}

bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void wifiEnsureUp() {
  const wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    if (lastPrintedStatus != WL_CONNECTED) {
      Serial.print("[WiFi] Reconnected. IP: ");
      Serial.println(WiFi.localIP());
      lastPrintedStatus = WL_CONNECTED;
    }
    return;
  }

  if (status != lastPrintedStatus) {
    Serial.print("[WiFi] Status: ");
    Serial.println(wifiStatusName(status));
    lastPrintedStatus = status;
  }

  // WL_IDLE_STATUS usually means the ESP32 is already trying to connect.
  // Calling WiFi.begin() again during that window causes:
  // "wifi:sta is connecting, cannot set config".
  if (status == WL_IDLE_STATUS) return;

  const uint32_t now = millis();
  if (now - lastReconnectAttempt < RECONNECT_COOLDOWN_MS) return;
  lastReconnectAttempt = now;

  Serial.println("[WiFi] Reconnecting...");
  WiFi.reconnect();
}
