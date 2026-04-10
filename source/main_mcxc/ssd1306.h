#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>

<<<<<<< HEAD
/*
 * ssd1306.h — SSD1306 128x64 OLED driver for FRDM-MCXC444
 *
 * Interface : I2C1 (hardware peripheral)
 * Pins      : PTC1 = SCL (ALT2),  PTC2 = SDA (ALT2)
 * Header    : Arduino J4 — pin 12 (SCL) and pin 10 (SDA)
 * I2C addr  : 0x3C (SA0 pin low, most common modules)
 *             change SSD1306_I2C_ADDR to 0x3D if SA0 is pulled high
 *
 * No pin conflicts with existing project peripherals:
 *   ADC    : PTB0, PTB1          — different port
 *   Buttons: PTA4, PTC3          — PTC3 conflicts! see note below.
 *   RGB LED: PTE29/30, PTD5      — different ports
 *
 * NOTE: PTC3 is used by SW2 (start button). PTC1 and PTC2 are the
 * I2C pins — these are adjacent but distinct pins, no conflict.
 *
 * Display layout (128x64 pixels, 8 rows of 8px each):
 *
 *   Row 0  (y=0..7 ) : "SYS:  ON " or "SYS: OFF"   — large text
 *   Row 1  (y=8..15) : "ALT:  OK " or "ALT:  AL"   — large text
 *   Row 2  (y=16..23): divider line
 *   Row 3-4(y=24..39): "LIGHT"  label + value       — large text
 *   Row 5-6(y=40..55): "MIC P2P" label + value      — large text
 *   Row 7  (y=56..63): status bar
 */

#define SSD1306_I2C_ADDR    0x3CU   /* 7-bit address */
#define SSD1306_WIDTH       128U
#define SSD1306_HEIGHT      64U

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/** Initialise I2C1 hardware and the SSD1306 controller. Call once. */
void SSD1306_Init(void);

/** Clear the display (all pixels off). */
void SSD1306_Clear(void);

/**
 * Update all four data panels in one call.
 * This is the main function used by alertTask.
 *
 * @param started   true = system running
 * @param alert     true = alert latched
 * @param lightAdc  raw ADC value 0-4095
 * @param micP2P    mic peak-to-peak 0-4095
 */
void SSD1306_ShowAll(bool started, bool alert,
                     uint16_t lightAdc, uint16_t micP2P);

#endif /* SSD1306_H */
=======
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
>>>>>>> testing
