#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#define MIC_P2P_THRESHOLD          9U
#define LIGHT_DARK_THRESHOLD       100U
#define LIGHT_BRIGHT_THRESHOLD     210U
#define TEMP_HIGH_THRESHOLD_C      32.0f
#define DIST_CLOSE_THRESHOLD_CM    15.0f

typedef enum {
    WARNING_STATE_IDLE = 0,
    WARNING_STATE_ACKNOWLEDGED = 1,
    WARNING_STATE_YELLOW = 2,
    WARNING_STATE_RED = 3,
    WARNING_STATE_RED_BUZZER = 4
} WarningState;

typedef enum {
    OLED_SCREEN_SENSORS = 0,
    OLED_SCREEN_SUGGESTION = 1
} OledScreenMode;

typedef struct {
    uint16_t lightRaw;
    uint16_t micRaw;
    uint16_t micMin;
    uint16_t micMax;
    uint16_t micP2P;
    uint8_t  soundFlag;
    uint8_t  lightFlag;
    float    temperatureC;
    float    distanceCm;
    uint8_t  tempFlag;
    uint8_t  distanceFlag;
    uint8_t  activeCount;
    uint8_t  remoteValid;
    uint8_t  remoteActiveCount;
} SensorPacket;

extern QueueHandle_t     g_sensorQueue;
extern SemaphoreHandle_t g_buttonSema;
extern SemaphoreHandle_t g_statusMutex;

extern bool           g_systemStarted;
extern bool           g_alertSuppressed;
extern WarningState   g_warningState;
extern OledScreenMode g_oledScreenMode;
extern SensorPacket   g_latestPacket;

#endif
