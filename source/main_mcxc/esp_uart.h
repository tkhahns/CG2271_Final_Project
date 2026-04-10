#ifndef ESP_UART_H
#define ESP_UART_H

#include "app_types.h"
#include <stdbool.h>
#include <stdint.h>

void ESP_UART_Init(void);
void ESP_UART_ServiceRx(void);

void ESP_UART_GetRemoteCount(bool *remoteValid, uint8_t *remoteActiveCount);

void ESP_UART_GetRemoteData(bool *remoteValid,
                            uint8_t *remoteActiveCount,
                            float *remoteTemperatureC,
                            bool *remoteTemperatureValid,
                            float *remoteDistanceCm,
                            bool *remoteDistanceValid);

void ESP_UART_SendTelemetry(const SensorPacket *packet,
                            bool systemStarted,
                            bool alertSuppressed);

#endif