#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>

#define SSD1306_I2C_ADDR    0x3CU
#define SSD1306_WIDTH       128U
#define SSD1306_HEIGHT      64U

void SSD1306_Init(void);
void SSD1306_Clear(void);

void SSD1306_ShowAll(bool started,
                     bool alert,
                     uint16_t lightAdc,
                     uint16_t micP2P,
                     uint8_t activeCount,
                     bool showSuggestionScreen,
                     float temperatureC,
                     bool temperatureValid,
                     float distanceCm,
                     bool distanceValid);

#endif