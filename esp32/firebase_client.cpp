#include "firebase_client.h"
#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static WiFiClientSecure firebaseClient;

static const char* warningLevelStr(int level) {
    switch (level) {
        case 0:  return "Normal";
        case 1:  return "Warning";
        case 2:  return "Critical";
        default: return "Unknown";
    }
}

void initFirebase() {
    firebaseClient.setInsecure();  // Skip cert verification (acceptable for school project)
    Serial.println("[Firebase] Client ready.");
}

static String buildSensorJson(const SensorData_t& data) {
    JsonDocument doc;
    doc["temperature"]  = serialized(String(data.temperature, 1));
    doc["humidity"]     = serialized(String(data.humidity, 1));
    doc["light"]        = data.light;
    doc["noise"]        = data.noise;
    doc["distance"]     = serialized(String(data.distance, 1));
    doc["warningLevel"] = data.warningLevel;
    doc["warningText"]  = warningLevelStr(data.warningLevel);

    // Firebase server timestamp
    JsonObject ts = doc["timestamp"].to<JsonObject>();
    ts[".sv"] = "timestamp";

    String json;
    serializeJson(doc, json);
    return json;
}

bool logToFirebase(const SensorData_t& data) {
    if (!isWiFiConnected()) return false;

    String json = buildSensorJson(data);
    bool success = true;

    // --- 1. PUT to /latest.json (overwrite with current reading) ---
    {
        HTTPClient http;
        String url = "https://" + String(FIREBASE_HOST) + "/latest.json?auth=" + String(FIREBASE_AUTH);
        http.begin(firebaseClient, url);
        http.addHeader("Content-Type", "application/json");
        int code = http.PUT(json);

        if (code == 200) {
            Serial.println("[Firebase] /latest updated.");
        } else {
            Serial.print("[Firebase] /latest error: ");
            Serial.println(code);
            success = false;
        }
        http.end();
    }

    // --- 2. POST to /readings.json (append timestamped history) ---
    {
        HTTPClient http;
        String url = "https://" + String(FIREBASE_HOST) + "/readings.json?auth=" + String(FIREBASE_AUTH);
        http.begin(firebaseClient, url);
        http.addHeader("Content-Type", "application/json");
        int code = http.POST(json);

        if (code == 200) {
            Serial.println("[Firebase] /readings appended.");
        } else {
            Serial.print("[Firebase] /readings error: ");
            Serial.println(code);
            success = false;
        }
        http.end();
    }

    return success;
}

// Read threshold overrides stored in Firebase (e.g. set via a web dashboard)
bool readThresholdsFromFirebase(float& tempThreshold, float& distThreshold) {
    if (!isWiFiConnected()) return false;

    HTTPClient http;
    String url = "https://" + String(FIREBASE_HOST) + "/thresholds.json?auth=" + String(FIREBASE_AUTH);
    http.begin(firebaseClient, url);
    int code = http.GET();

    if (code != 200) {
        http.end();
        return false;
    }

    String response = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, response)) return false;

    if (doc.containsKey("temperature")) tempThreshold = doc["temperature"].as<float>();
    if (doc.containsKey("distance"))    distThreshold = doc["distance"].as<float>();

    return true;
}
