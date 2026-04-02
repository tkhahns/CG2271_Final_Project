#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#define MIC_P2P_THRESHOLD   9
#define LIGHT_DARK_THRESHOLD 500
#define TEMP_HIGH_THRESHOLD   30.0f
#define DIST_CLOSE_THRESHOLD  40.0f


typedef struct {
    uint16_t lightRaw;
    uint16_t micRaw;
    uint16_t micMin;
    uint16_t micMax;
    uint16_t micP2P;
    uint8_t  p2pFlag;
    uint8_t  darkFlag;

    // Remote ESP32 Sensors
	float    temperature;
	float    distance;

	// Trigger Flags
//	uint8_t  p2pFlag;
//	uint8_t  darkFlag;
	uint8_t  tempFlag;
	uint8_t  closeFlag;

} SensorPacket;

// FreeRTOS objects defined in main
extern QueueHandle_t     g_sensorQueue;
extern SemaphoreHandle_t g_buttonSema;
extern SemaphoreHandle_t g_statusMutex;

// Shared application state (guarded by g_statusMutex)

extern bool         g_systemStarted;
extern bool         g_alertLatched;
extern SensorPacket g_latestPacket;

#endif
