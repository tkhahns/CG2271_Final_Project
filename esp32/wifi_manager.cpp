#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>

static unsigned long lastReconnectAttempt = 0;
static const unsigned long RECONNECT_COOLDOWN_MS = 5000;

void connectWiFi() {
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
        Serial.println("[WiFi] Connection FAILED.");
    }
}

bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void ensureWiFiConnected() {
    if (isWiFiConnected()) return;

    unsigned long now = millis();
    if (now - lastReconnectAttempt < RECONNECT_COOLDOWN_MS) return;
    lastReconnectAttempt = now;

    Serial.println("[WiFi] Lost connection. Reconnecting...");
    connectWiFi();
}
