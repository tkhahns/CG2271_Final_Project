#ifndef ESP_UART_H
#define ESP_UART_H

#include <stdbool.h>
#include <stdint.h>

void ESP_UART_Init(void);
void ESP_UART_SendTelemetry(uint16_t lightRaw, uint16_t micP2P, bool systemStarted, bool alertLatched);

#endif
