#ifndef MAX7219_H
#define MAX7219_H

#include <stdint.h>
#include <stdbool.h>

/*
 * MAX7219 driver — 4 chained 8-digit modules
 *
 * Wiring (SPI0 on MCXC444):
 *   PTD0  GPIO output  → CS  (LOAD)
 *   PTD1  ALT2         → CLK (SPI0_SCK)
 *   PTD2  ALT2         → DIN (SPI0_MOSI)
 *
 * Chain order — MCU DIN feeds Module 4 first:
 *   MCU → [Mod 4] → [Mod 3] → [Mod 2] → [Mod 1]
 *
 * Display layout (left → right):
 *
 *   Module 4          Module 3          Module 2          Module 1
 *   ┌──────────┐      ┌──────────┐      ┌──────────┐      ┌──────────┐
 *   │ SYS: _ON │      │ ALT: _AL │      │ LIT:XXXX │      │ MIC:XXXX │
 *   └──────────┘      └──────────┘      └──────────┘      └──────────┘
 *   System status     Alert status      Light ADC 0-4095  Mic P2P 0-4095
 *
 * Each module: digits 8..5 = 4-char label, digits 4..1 = value / status word.
 * Label digits use raw segment data; value digits use Code-B decode.
 */

#define MAX7219_NUM_MODULES    4
#define MAX7219_DIGITS_PER_MOD 8

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/** Initialise SPI0 and all 4 MAX7219 modules. Call once at startup. */
void MAX7219_Init(void);

/** Blank all 32 digits. */
void MAX7219_Clear(void);

/**
 * Module 4 — System status.
 * Displays "SYS:  ON" (started) or "SYS: OFF" (stopped).
 */
void MAX7219_ShowSystemStatus(bool started);

/**
 * Module 3 — Alert status.
 * Displays "ALT:  AL" (alert) or "ALT:  OK" (clear).
 */
void MAX7219_ShowAlertStatus(bool alert);

/**
 * Module 2 — Light ADC value.
 * Displays "LIT:XXXX" where XXXX = 0–4095. Leading zeros suppressed.
 */
void MAX7219_ShowLightAdc(uint16_t lightAdc);

/**
 * Module 1 — Mic peak-to-peak value.
 * Displays "MIC:XXXX" where XXXX = 0–4095. Leading zeros suppressed.
 */
void MAX7219_ShowMicP2P(uint16_t micP2P);

/** Update all four panels in a single call. */
void MAX7219_ShowAll(bool started, bool alert,
                     uint16_t lightAdc, uint16_t micP2P);

#endif /* MAX7219_H */
