#include "uart_handler.h"
#include "config.h"

// Use Serial2 for MCXC444 communication
#define MCXC_SERIAL Serial2

void initUART() {
    MCXC_SERIAL.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    Serial.println("UART initialized.");
}

// Expected format from MCXC444:
// "TEMP:28.5,HUM:65.0,LIGHT:512,NOISE:1,DIST:45.2,WARN:1\n"
SensorData_t readUART() {
    SensorData_t data = {0, 0, 0, 0, 0, 0, false};

    if (!MCXC_SERIAL.available()) {
        return data;
    }

    String line = MCXC_SERIAL.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) {
        return data;
    }

    Serial.print("UART received: ");
    Serial.println(line);

    // Parse comma-separated key:value pairs
    int start = 0;
    while (start < (int)line.length()) {
        int commaIdx = line.indexOf(',', start);
        if (commaIdx == -1) commaIdx = line.length();

        String pair = line.substring(start, commaIdx);
        int colonIdx = pair.indexOf(':');

        if (colonIdx > 0) {
            String key = pair.substring(0, colonIdx);
            String val = pair.substring(colonIdx + 1);

            if (key == "TEMP")       data.temperature  = val.toFloat();
            else if (key == "HUM")   data.humidity     = val.toFloat();
            else if (key == "LIGHT") data.light        = val.toInt();
            else if (key == "NOISE") data.noise        = val.toInt();
            else if (key == "DIST")  data.distance     = val.toFloat();
            else if (key == "WARN")  data.warningLevel = val.toInt();
        }

        start = commaIdx + 1;
    }

    data.valid = true;
    return data;
}

void sendUART(const char* message) {
    MCXC_SERIAL.println(message);
}
