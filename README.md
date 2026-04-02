# Smart Study Environment Monitor

CG2271 Real-Time Operating Systems — AY2025/26 Semester 2 — B01-08

A dual-board IoT system that monitors study desk conditions (temperature, humidity, light, noise, sitting distance) and provides real-time feedback via RGB LED, LCD, buzzer, and Telegram notifications with AI-generated study advisories.

## Architecture

```
                         UART                 HTTPS
MCXC444 (FreeRTOS)    ◀──────▶    ESP32    ◀───────▶    Cloud
──────────────────                ─────                  ─────
Photoresistor (ADC)               DHT11                  Firebase RTDB
Big Sound (IRQ)                   HC-SR04                Telegram Bot
RGB LED                           LCD 16x2               OpenAI API
SW2/SW3 (IRQ)                     Buzzer
                                  Wi-Fi
```

## Hardware

| Component | Board | Interface |
|---|---|---|
| DHT11 (temp/humidity) | ESP32 | GPIO |
| Photoresistor (light) | MCXC444 | ADC |
| Big Sound (noise) | MCXC444 | GPIO IRQ |
| HC-SR04 (distance) | ESP32 | GPIO |
| RGB LED | MCXC444 | GPIO |
| Active Buzzer | ESP32 | GPIO |
| LCD QC1602A + PCF8574 | ESP32 | I2C |
| SW2 (start/stop) | MCXC444 | IRQ |
| SW3 (alert ack) | MCXC444 | IRQ |

## MCXC444 — FreeRTOS Tasks

| Task | Priority | Role |
|---|---|---|
| vAlertTask | High | Reads sensor queue, sets warning level, drives RGB LED |
| vButtonTask | High | Handles SW2/SW3 interrupts |
| vCommsTask | Medium | UART TX/RX with ESP32 |
| vEnvTask | Low | Polls photoresistor and sound sensor |
| vDistanceTask | Low | Polls HC-SR04 ultrasonic sensor |

RTOS primitives: `xSensorQueue` (queue), `xSoundSemaphore` (binary semaphore), `xSystemStatusMutex` (mutex).

## ESP32 — Cloud Integration

| Module | File | Function |
|---|---|---|
| Wi-Fi | `wifi_manager.cpp` | Connect/reconnect to Wi-Fi |
| Firebase RTDB | `firebase_client.cpp` | PUT `/latest.json`, POST `/readings.json` |
| Telegram Bot | `telegram_bot.cpp` | `/status`, `/settemp`, `/setdist`, push alerts |
| OpenAI API | `openai_client.cpp` | One-sentence study advisory from sensor data |
| UART | `uart_handler.cpp` | Parse sensor bundle from MCXC444 |
| LCD | `lcd_display.cpp` | Display readings and advisory text |

OpenAI is called **only** when a Telegram command is received or a warning condition triggers.

## UART Protocol

MCXC444 to ESP32:
```
TEMP:28.5,HUM:65.0,LIGHT:512,NOISE:1,DIST:45.2,WARN:1\n
```

ESP32 to MCXC444:
```
SETTEMP:30.0\n
SETDIST:40.0\n
Advisory text here\n
```

## Warning States

| Level | Condition | Response |
|---|---|---|
| 0 — Normal | All within thresholds | RGB: Green |
| 1 — Warning | Any one threshold exceeded | RGB: Yellow |
| 2 — Critical | Multiple thresholds exceeded | RGB: Red, Buzzer, Telegram alert, OpenAI advisory |

## ESP32 Setup

1. Install [Arduino IDE](https://www.arduino.cc/en/software) and add ESP32 board support
2. Install libraries: `ArduinoJson` (v7+), `LiquidCrystal I2C`
3. Copy `esp32/config.h.example` to `esp32/config.h` and fill in:
   - Wi-Fi SSID and password
   - Firebase RTDB URL and database secret
   - Telegram bot token and chat ID (create via [@BotFather](https://t.me/BotFather))
   - OpenAI API key
4. Select board "ESP32 Dev Module" and upload

## Workload

| Member | Board | Responsibilities |
|---|---|---|
| Member 1 | MCXC444 | HC-SR04, Photoresistor, Sound ISR, vDistanceTask, vEnvTask |
| Member 2 | MCXC444 | vAlertTask, vCommsTask, RGB LED, UART, RTOS mutexes |
| Member 3 | ESP32 | DHT11, LCD Display (I2C), Active Buzzer, UART parsing |
| Member 4 | ESP32 | Firebase RTDB, Telegram Bot API, OpenAI API, Wi-Fi/HTTP |

## Project Structure

```
source/                  MCXC444 FreeRTOS application
  main.c                 Entry point, task creation, RTOS primitives
  common.h               Shared types and extern handles
  distanceTask.c/.h      HC-SR04 ultrasonic sensor task
esp32/                   ESP32 Arduino application
  esp32.ino              Main loop
  config.h.example       Template for secrets (copy to config.h)
  wifi_manager.cpp/.h    Wi-Fi management
  firebase_client.cpp/.h Firebase RTDB logging
  telegram_bot.cpp/.h    Telegram Bot commands and alerts
  openai_client.cpp/.h   OpenAI advisory generation
  uart_handler.cpp/.h    UART receive/parse from MCXC444
  lcd_display.cpp/.h     LCD display control
```
