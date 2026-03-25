
#ifndef COMMON_H
#define COMMON_H

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"


typedef enum {
    SENSOR_TYPE_DISTANCE,
    SENSOR_TYPE_LIGHT,
    SENSOR_TYPE_TEMP,
    SENSOR_TYPE_HUMIDITY
} SensorType_t;

typedef struct {
    SensorType_t sensorType;
    float value;
} SensorData_t;


extern QueueHandle_t xSensorQueue;
extern SemaphoreHandle_t xSoundSemaphore;
extern SemaphoreHandle_t xSystemStatusMutex;

#endif
