/*
 * Smart Study Environment Monitor — ESP32 Main
 * CG2271 AY2025/26 Semester 2 — B01-08
 *
 * Member 4: Firebase RTDB, Telegram Bot API, OpenAI API, Wi-Fi / HTTP
 * Member 3: DHT11, HC-SR04, LCD Display, Buzzer, ESP32 UART Parsing
 *
 * Flow (per interim report Section 3.4):
 *   1. Receive sensor bundle from MCXC444 via UART
 *   2. Log readings + warning state to Firebase RTDB
 *   3. Poll Telegram Bot API for user commands (/status, /settemp, /setdist)
 *   4. ONLY when a Telegram command is sent OR a condition requires attention:
 *      call OpenAI API -> get one-sentence advisory -> display on LCD
 */

#include "config.h"
#include "wifi_manager.h"
#include "uart_handler.h"
#include "firebase_client.h"
#include "telegram_bot.h"
#include "openai_client.h"
#include "lcd_display.h"

// ==================== State ====================

// Thresholds (updated via Telegram /settemp, /setdist)
float tempThreshold = DEFAULT_TEMP_THRESHOLD;
float distThreshold = DEFAULT_DIST_THRESHOLD;

// Track warning level to detect changes
int lastWarningLevel = 0;

// Latest valid sensor data (kept for Telegram /status queries)
SensorData_t latestData = {0, 0, 0, 0, 0, 0, false};

// Timing
unsigned long lastSensorRead   = 0;
unsigned long lastTelegramPoll = 0;
unsigned long lastFirebaseLog  = 0;

// ==================== Setup ====================

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("========================================");
    Serial.println("  Smart Study Environment Monitor");
    Serial.println("  ESP32 — Starting up...");
    Serial.println("========================================");

    // 1. LCD first so we can show boot status
    initLCD();
    displayText("Study Monitor", "Booting...");

    // 2. Wi-Fi
    displayText("Study Monitor", "WiFi...");
    connectWiFi();

    if (isWiFiConnected()) {
        displayText("Study Monitor", "WiFi OK");
    } else {
        displayText("Study Monitor", "WiFi FAIL");
    }
    delay(500);

    // 3. UART to MCXC444
    initUART();

    // 4. Cloud services
    initFirebase();
    initTelegram();
    initOpenAI();

    displayText("Monitor Ready", "Waiting data...");
    Serial.println("[Main] Setup complete. Entering main loop.");
}

// ==================== Helpers ====================

// Request OpenAI advisory -> display on LCD -> send back to MCXC444 via UART
static String requestAndDisplayAdvisory(const SensorData_t& data) {
    String advisory = getAdvisory(data);

    // Display advisory on LCD (Member 3's scrollText)
    scrollText(advisory);

    // Send advisory back to MCXC444 so vCommsTask can process it
    sendUART(advisory.c_str());

    return advisory;
}

// Send updated threshold back to MCXC444 via UART
static void relayThresholdToMCXC(const char* key, float value) {
    String msg = String(key) + ":" + String(value, 1);
    sendUART(msg.c_str());
    Serial.print("[Main] Threshold relayed to MCXC444: ");
    Serial.println(msg);
}

// ==================== Main Loop ====================

void loop() {
    // Keep Wi-Fi alive
    ensureWiFiConnected();

    unsigned long now = millis();

    // ========================================================
    // STEP 1: Read sensor data from MCXC444 via UART (every 2s)
    // ========================================================
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL_MS) {
        lastSensorRead = now;

        SensorData_t data = readUART();

        if (data.valid) {
            latestData = data;

            // Show live readings on LCD
            String line1 = "T:" + String(data.temperature, 0) + "C H:" + String(data.humidity, 0) + "%";
            String line2 = "D:" + String(data.distance, 0) + "cm " + String(data.noise ? "LOUD" : "OK");
            displayText(line1, line2);

            // ========================================================
            // STEP 2: Log to Firebase RTDB
            // ========================================================
            if (now - lastFirebaseLog >= FIREBASE_LOG_INTERVAL_MS) {
                lastFirebaseLog = now;
                logToFirebase(data);
            }

            // ========================================================
            // STEP 3: Check if condition requires attention
            //         -> Call OpenAI + alert via Telegram
            //         (Per report: "only when ... a condition requires attention")
            // ========================================================
            bool warningChanged = (data.warningLevel != lastWarningLevel);
            bool requiresAttention = warningChanged && (data.warningLevel >= 1);
            lastWarningLevel = data.warningLevel;

            if (requiresAttention) {
                Serial.print("[Main] Warning level changed to ");
                Serial.print(data.warningLevel);
                Serial.println(" — requesting advisory.");

                String advisory = requestAndDisplayAdvisory(data);

                // Send Telegram alert
                sendTelegramAlert(data, advisory);
            }
        }
    }

    // ========================================================
    // STEP 4: Poll Telegram for user commands (every 3s)
    //         -> On command, call OpenAI for advisory
    //         (Per report: "only when a telegram command is sent ...")
    // ========================================================
    if (now - lastTelegramPoll >= TELEGRAM_POLL_INTERVAL_MS) {
        lastTelegramPoll = now;

        TelegramResult_t tgResult = pollTelegram(latestData, tempThreshold, distThreshold);

        if (tgResult.received) {
            // Relay threshold changes back to MCXC444 via UART
            if (tgResult.command == CMD_SETTEMP) {
                relayThresholdToMCXC("SETTEMP", tgResult.value);
            }
            else if (tgResult.command == CMD_SETDIST) {
                relayThresholdToMCXC("SETDIST", tgResult.value);
            }

            // Per report: call OpenAI when a Telegram command is sent
            // Only for commands where advisory is relevant and data exists
            if (latestData.valid &&
                (tgResult.command == CMD_STATUS ||
                 tgResult.command == CMD_SETTEMP ||
                 tgResult.command == CMD_SETDIST)) {

                String advisory = requestAndDisplayAdvisory(latestData);

                // Also send the advisory via Telegram for /status
                if (tgResult.command == CMD_STATUS) {
                    sendTelegramMessage("*Advisory:* " + advisory);
                }
            }
        }
    }
}
