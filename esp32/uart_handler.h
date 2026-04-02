#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include <Arduino.h>

typedef struct {
    float temperature;
    float humidity;
    int   light;
    int   noise;       // 0 = quiet, 1 = noisy
    float distance;
    int   warningLevel; // 0 = Normal, 1 = Warning, 2 = Critical
    bool  valid;
} SensorData_t;

void          initUART();
SensorData_t  readUART();
void          sendUART(const char* message);

#endif
