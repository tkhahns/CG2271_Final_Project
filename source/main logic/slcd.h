#ifndef SLCD_H
#define SLCD_H

#include <stdint.h>
#include <stdbool.h>

/*
 * slcd.h — On-board SLCD driver for FRDM-MCXC444
 *
 * Display: Lumex LCD-S401M16KR  (4-digit, 7-segment, 4-COM multiplexed)
 *
 * Pin mapping (from board user manual UM12120 Table 13 and AN12579):
 *
 *   Functional  SLCD pin  MCU pin   Role
 *   LCD-01      P59       PTE20     COM0  (back plane)
 *   LCD-02      P60       PTE21     COM1  (back plane)
 *   LCD-03      P14       PTB18     COM2  (back plane)
 *   LCD-04      P15       PTB19     COM3  (back plane)
 *   LCD-05      P20       PTC0      D0    (front plane)
 *   LCD-06      P24       PTC4      D1    (front plane)
 *   LCD-07      P26       PTC5      D2    (front plane)
 *   LCD-08      P27       PTC6      D3    (front plane)
 *   LCD-09      P40       PTD0      D4    (front plane)
 *   LCD-10      P42       PTD2      D5    (front plane)
 *   LCD-11      P43       PTD3      D6    (front plane)
 *   LCD-12      P44       PTD4      D7    (front plane)
 *
 * Digit layout on the glass (left to right):
 *   Digit 0  Digit 1  :  Digit 2  Digit 3
 *
 * Segment/COM matrix (per AN12579 Table 1):
 *   COM0: D, DP   for each digit pair
 *   COM1: E, C
 *   COM2: G, B
 *   COM3: F, A
 *
 * IMPORTANT — conflict with SPI on PTD4:
 *   PTD4 is shared between LCD D7 (ALT0) and SPI1_SS (ALT2).
 *   If you use the SLCD you must NOT simultaneously use SPI on
 *   the Arduino/mikroBUS headers that route through PTD4.
 *   Likewise PTD0, PTD2, PTD3 conflict with SPI_CS/MOSI/MISO
 *   used by the MAX7219. You cannot run both at the same time
 *   without hardware changes.
 */

/* Digit position indices (left to right: 0..3) */
#define SLCD_DIGIT_0    0U
#define SLCD_DIGIT_1    1U
#define SLCD_DIGIT_2    2U
#define SLCD_DIGIT_3    3U

/* Special display values for SLCD_SetDigit() */
#define SLCD_CHAR_BLANK 10U   /* blank / off           */
#define SLCD_CHAR_DASH  11U   /* minus sign (middle segment only) */

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/** Initialise clocks, pin mux, and the LCD peripheral. Call once. */
void SLCD_Init(void);

/** Start the LCD waveform generator (display becomes visible). */
void SLCD_Start(void);

/** Stop the LCD waveform generator (display goes blank). */
void SLCD_Stop(void);

/**
 * Set a single digit position.
 * @param pos     Digit index 0–3 (left to right).
 * @param value   0–9 to show a digit, SLCD_CHAR_BLANK to clear,
 *                SLCD_CHAR_DASH for a minus sign.
 * @param dot     true = illuminate the decimal point for this digit.
 */
void SLCD_SetDigit(uint8_t pos, uint8_t value, bool dot);

/**
 * Display a 4-digit unsigned integer (0–9999).
 * Leading zeros are suppressed.
 * Values > 9999 display "----".
 */
void SLCD_ShowNumber(uint16_t value);

/**
 * Display a raw 4-character string.
 * Supported characters: '0'–'9', '-', ' ' (space/blank).
 * Unsupported characters are shown as blank.
 */
void SLCD_ShowString(const char *str);

/** Clear all four digits. */
void SLCD_Clear(void);

#endif /* SLCD_H */
