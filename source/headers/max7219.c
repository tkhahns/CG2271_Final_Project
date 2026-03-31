/*
 * max7219.c — MAX7219 driver for 4 daisy-chained 8-digit modules
 *
 * Hardware: SPI0 on MCXC444
 *   PTD0  GPIO → CS (LOAD)
 *   PTD1  ALT2 → SPI0_SCK  (CLK)
 *   PTD2  ALT2 → SPI0_MOSI (DIN)
 *
 * ----------------------------------------------------------------
 * Chaining protocol recap
 * ----------------------------------------------------------------
 * To write one register to ALL modules simultaneously, send
 * N×2 bytes (MSB first) before pulsing CS high:
 *
 *   CS low
 *   [addr|data for mod 4] [addr|data for mod 3] ... [addr|data for mod 1]
 *   CS high  ← latches all modules at once
 *
 * To write to ONE module and leave the others unchanged, send a
 * NO-OP (0x00, 0x00) frame for every module that must not change,
 * in the correct position.
 *
 * Module numbering in this file: 1 = rightmost (closest to DOUT),
 * 4 = leftmost (first to receive data from MCU).
 * When building the SPI frame, module 4's bytes go first.
 *
 * ----------------------------------------------------------------
 * Digit layout within each module
 * ----------------------------------------------------------------
 * Each module has digit registers 1..8 (1 = rightmost segment).
 *
 *   Reg:  8    7    6    5  |  4    3    2    1
 *         [  label chars  ] | [   value digits  ]
 *
 * Label digits (8..5): decode mode DISABLED → raw 7-segment bytes.
 * Value digits (4..1): decode mode ENABLED  → Code-B font (0-9, -, E, H, L, P, blank).
 *
 * We manage per-module decode mode with MAX7219_REG_DECODE_MODE = 0x0F
 * (only lower 4 bits = digits 4..1 decoded; upper 4 = raw).
 */

#include "max7219.h"
#include "MCXC444.h"
#include <string.h>
#include <stdbool.h>

/* ================================================================== */
/* Configuration                                                        */
/* ================================================================== */

#define NUM_MOD     MAX7219_NUM_MODULES      // 4
#define DIGS_PER    MAX7219_DIGITS_PER_MOD   // 8

/* SPI0 / GPIO pin numbers on Port D */
#define PIN_CS      0   /* PTD0  GPIO output */
#define PIN_SCK     1   /* PTD1  ALT2 = SPI0_SCK  */
#define PIN_MOSI    2   /* PTD2  ALT2 = SPI0_MOSI */

/* ================================================================== */
/* MAX7219 register addresses                                           */
/* ================================================================== */

#define REG_NOOP         0x00
#define REG_DIGIT(n)     ((uint8_t)(n))
#define REG_DECODE_MODE  0x09
#define REG_INTENSITY    0x0A
#define REG_SCAN_LIMIT   0x0B
#define REG_SHUTDOWN     0x0C
#define REG_DISP_TEST    0x0F

/* Decode mode: lower nibble = digits 1..4 decoded (Code-B),
 * upper nibble = digits 5..8 raw.  0x0F = 0b00001111             */
#define DECODE_LOWER_ONLY  0x0F
#define DECODE_ALL         0xFF
#define DECODE_NONE        0x00

/* ================================================================== */
/* Code-B special codes (used in value digits 1..4)                   */
/* ================================================================== */

#define CB_DASH   0x0A
#define CB_E      0x0B
#define CB_H      0x0C
#define CB_L      0x0D
#define CB_P      0x0E
#define CB_BLANK  0x0F


/* ================================================================== */
/* Raw 7-segment codes for label characters                            */
/* Segment order (bit): g f e d c b a (bit7 = DP, always 0 here)      */
/*                                                                      */
/*   Segments:   _                                                      */
/*              |_|   a=top, b=upper-right, c=lower-right,             */
/*              |_|   d=bottom, e=lower-left, f=upper-left, g=middle   */
/* ================================================================== */

/*  Character   gfedcba  hex  */
#define SEG_A   0x77   /* 0111 0111 */
#define SEG_C   0x39   /* 0011 1001 */
#define SEG_E   0x79   /* 0111 1001 */
#define SEG_F   0x71   /* 0111 0001 */
#define SEG_G   0x3D   /* 0011 1101 - looks like 6 without top-right */
#define SEG_H   0x76   /* 0111 0110 */
#define SEG_I   0x06   /* 0000 0110 - two right segments */
#define SEG_K   0x76   /* reuse H (no true K on 7-seg) */
#define SEG_L   0x38   /* 0011 1000 */
#define SEG_M   0x37   /* approximate - 7-seg cannot do M well */
#define SEG_N   0x54   /* 0101 0100 - small n */
#define SEG_O   0x3F   /* 0011 1111 - same as 0 */
#define SEG_P   0x73   /* 0111 0011 */
#define SEG_R   0x50   /* 0101 0000 - small r */
#define SEG_S   0x6D   /* 0110 1101 - same as 5 */
#define SEG_T   0x78   /* 0111 1000 */
#define SEG_U   0x3E   /* 0011 1110 */
#define SEG_Y   0x6E   /* 0110 1110 */
#define SEG_COLON  0x00  /* no colon on 7-seg; leave blank, separator handled by layout */
#define SEG_DASH   0x40  /* 0100 0000 - middle segment only */
#define SEG_BLANK  0x00  /* all off */

/* ================================================================== */
/* CS / SPI helpers                                                     */
/* ================================================================== */

#define CS_LOW()   (GPIOD->PCOR = (1U << PIN_CS))
#define CS_HIGH()  (GPIOD->PSOR = (1U << PIN_CS))

static inline void spi_txByte(uint8_t b) {
    while (!(SPI0->S & SPI_S_SPTEF_MASK)) { }
    SPI0->DL = b;
    while (!(SPI0->S & SPI_S_SPRF_MASK))  { }
    (void)SPI0->DL;   /* clear SPRF */
}

/* ================================================================== */
/* Chained write helpers                                               */
/* ================================================================== */

/*
 * Write the SAME register+data to ALL modules simultaneously.
 * Frame layout (CS pulse):  [mod4 addr|data] [mod3] [mod2] [mod1]
 */
static void writeAll(uint8_t reg, uint8_t data) {
    CS_LOW();
    for (uint8_t m = 0; m < NUM_MOD; m++) {
        spi_txByte(reg);
        spi_txByte(data);
    }
    CS_HIGH();
}

/*
 * Write a register to ONE specific module; send NO-OP to the others.
 * module: 1 = rightmost, NUM_MOD = leftmost.
 *
 * SPI frame order: module NUM_MOD first → module 1 last.
 * So "targeted module" bytes sit at position (NUM_MOD - module) from start.
 */
static void writeOne(uint8_t module, uint8_t reg, uint8_t data) {
    CS_LOW();
    for (uint8_t m = NUM_MOD; m >= 1U; m--) {
        if (m == module) {
            spi_txByte(reg);
            spi_txByte(data);
        } else {
            spi_txByte(REG_NOOP);
            spi_txByte(0x00U);
        }
    }
    CS_HIGH();
}

/* ================================================================== */
/* Internal: write an 8-digit module's full display in one burst      */
/* digits[0] = register 8 (leftmost), digits[7] = register 1          */
/* Each call writes all 8 digit registers of the target module.        */
/* ================================================================== */
static void writeModuleDigits(uint8_t module, const uint8_t digits[DIGS_PER]) {
    for (uint8_t d = 0; d < DIGS_PER; d++) {
        /* Register number: digit 8 first (d=0 → reg 8, d=7 → reg 1) */
        uint8_t reg = (uint8_t)(DIGS_PER - d);
        writeOne(module, reg, digits[d]);
    }
}

/* ================================================================== */
/* Internal: 4-digit decimal → Code-B array, leading-zero suppressed  */
/* out[0] = most-significant digit position (register 4 in module)     */
/* out[3] = least-significant digit position (register 1)              */
/* ================================================================== */
static void decimalToCodeB(uint16_t value, uint8_t out[4]) {
    if (value > 9999U) {
        out[0] = CB_DASH; out[1] = CB_DASH;
        out[2] = CB_DASH; out[3] = CB_DASH;
        return;
    }
    out[0] = (uint8_t)(value / 1000U); value %= 1000U;
    out[1] = (uint8_t)(value / 100U);  value %= 100U;
    out[2] = (uint8_t)(value / 10U);   value %= 10U;
    out[3] = (uint8_t)(value);

    /* Suppress leading zeros (keep at least the last digit) */
    for (uint8_t i = 0; i < 3U; i++) {
        if (out[i] == 0U) {
            out[i] = CB_BLANK;
        } else {
            break;
        }
    }
}

/* ================================================================== */
/* Internal: write a labelled panel to one module                      */
/*                                                                      */
/* label[4]: raw segment bytes for digits 8,7,6,5 (left 4 digits)     */
/* value[4]: Code-B bytes   for digits 4,3,2,1 (right 4 digits)       */
/*                                                                      */
/* Decode mode for the module is set to DECODE_LOWER_ONLY so that:     */
/*   digits 1–4  → Code-B decoded                                       */
/*   digits 5–8  → raw segment passthrough                              */
/* ================================================================== */
static void writePanel(uint8_t module,
                       const uint8_t label[4],
                       const uint8_t value[4]) {
    /* Set decode mode: lower 4 digits Code-B, upper 4 raw */
    writeOne(module, REG_DECODE_MODE, DECODE_LOWER_ONLY);

    /* Write digits 8..5 (label, raw segments) */
    for (uint8_t i = 0; i < 4U; i++) {
        uint8_t reg = (uint8_t)(DIGS_PER - i);   /* 8, 7, 6, 5 */
        writeOne(module, reg, label[i]);
    }

    /* Write digits 4..1 (value, Code-B) */
    for (uint8_t i = 0; i < 4U; i++) {
        uint8_t reg = (uint8_t)(4U - i);          /* 4, 3, 2, 1 */
        writeOne(module, reg, value[i]);
    }
}

/* ================================================================== */
/* Public: Init                                                         */
/* ================================================================== */

void MAX7219_Init(void) {
    /* --- Clock gates --- */
    SIM->SCGC4 |= SIM_SCGC4_SPI0_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTD_MASK;

    /* --- Pin mux ---
     *   PTD0 → GPIO output (CS)
     *   PTD1 → ALT2 (SPI0_SCK)
     *   PTD2 → ALT2 (SPI0_MOSI)
     */
    PORTD->PCR[PIN_CS]   = PORT_PCR_MUX(1);
    PORTD->PCR[PIN_SCK]  = PORT_PCR_MUX(2);
    PORTD->PCR[PIN_MOSI] = PORT_PCR_MUX(2);

    GPIOD->PDDR |= (1U << PIN_CS);
    CS_HIGH();   /* deassert */

    /* --- SPI0: master, Mode 0 (CPOL=0 CPHA=0), MSB first, 8-bit ---
     * Baud = BusClock / (SPPR+1) / 2^(SPR+1)
     * At 24 MHz: SPPR=2, SPR=1  → 24e6 / 3 / 4 = 2 MHz  */
    SPI0->C1 = SPI_C1_SPE_MASK | SPI_C1_MSTR_MASK | SPI_C1_SSOE_MASK;
    SPI0->C2 = 0;
    SPI0->BR = SPI_BR_SPPR(2) | SPI_BR_SPR(1);

    /* --- MAX7219 startup sequence (broadcast to all modules) --- */
    writeAll(REG_SHUTDOWN,    0x01U);  /* exit shutdown (normal operation) */
    writeAll(REG_DISP_TEST,   0x00U);  /* disable display test             */
    writeAll(REG_SCAN_LIMIT,  0x07U);  /* scan all 8 digits (0..7 = 1..8)  */
    writeAll(REG_INTENSITY,   0x07U);  /* brightness 0x00–0x0F             */
    writeAll(REG_DECODE_MODE, DECODE_LOWER_ONLY); /* default: lower=Code-B */

    MAX7219_Clear();
}

/* ================================================================== */
/* Public: Clear                                                        */
/* ================================================================== */

void MAX7219_Clear(void) {
    /* Set all decode modes and blank every digit register */
    writeAll(REG_DECODE_MODE, DECODE_LOWER_ONLY);
    for (uint8_t reg = 1U; reg <= DIGS_PER; reg++) {
        writeAll(reg, CB_BLANK);
    }
}

/* ================================================================== */
/* Public: ShowSystemStatus — Module 4                                  */
/*                                                                      */
/* Label digits 8..5: S Y S :                                          */
/* Value digits 4..1:   ON  or OFF                                     */
/*                                                                      */
/*  "SYS:  ON"  →  S Y S colon | blank blank O N                      */
/*  "SYS: OFF"  →  S Y S colon | blank O F F                          */
/* ================================================================== */

void MAX7219_ShowSystemStatus(bool started) {
    /* Label: S  Y  S  : */
    const uint8_t label[4] = { SEG_S, SEG_Y, SEG_S, SEG_COLON };

    uint8_t value[4];
    if (started) {
        /* "  ON" */
        value[0] = CB_BLANK;
        value[1] = CB_BLANK;
        value[2] = 0U;       /* Code-B '0' looks like 'O' */
        value[3] = CB_H;     /* Code-B 'H' is closest to 'N'; no true N in Code-B */
        /*
         * Note: Code-B has no 'N'. Best approximation on digit 1:
         * Use raw segment for digit 1 only by switching decode mode.
         * We handle this by splitting: digits 4..2 decoded, digit 1 raw.
         *
         * For clarity we write digit 1 ('n') as raw and set decode = 0x0E
         * (digits 4..2 decoded = bits 3..1, digit 1 raw = bit 0 cleared).
         */
        writeOne(4U, REG_DECODE_MODE, 0x0EU);  /* bits 3..1 decoded, bit 0 raw */

        /* Label */
        writeOne(4U, REG_DIGIT(8), label[0]);
        writeOne(4U, REG_DIGIT(7), label[1]);
        writeOne(4U, REG_DIGIT(6), label[2]);
        writeOne(4U, REG_DIGIT(5), label[3]);

        /* Value */
        writeOne(4U, REG_DIGIT(4), CB_BLANK);
        writeOne(4U, REG_DIGIT(3), CB_BLANK);
        writeOne(4U, REG_DIGIT(2), 0U);        /* 'O' (Code-B 0) */
        writeOne(4U, REG_DIGIT(1), SEG_N);     /* 'n' raw segment */
    } else {
        /* "OFF" — all Code-B: use E for 'F' approximation
         * Code-B has no F; use raw for digits 2,1 (F,F).
         * Decode mode 0x08 = only digit 4 decoded, rest raw.          */
        writeOne(4U, REG_DECODE_MODE, 0x00U);  /* all raw */

        writeOne(4U, REG_DIGIT(8), label[0]);
        writeOne(4U, REG_DIGIT(7), label[1]);
        writeOne(4U, REG_DIGIT(6), label[2]);
        writeOne(4U, REG_DIGIT(5), label[3]);

        writeOne(4U, REG_DIGIT(4), SEG_BLANK); /* leading blank */
        writeOne(4U, REG_DIGIT(3), SEG_O);     /* O */
        writeOne(4U, REG_DIGIT(2), SEG_F);     /* F */
        writeOne(4U, REG_DIGIT(1), SEG_F);     /* F */
    }
}

/* ================================================================== */
/* Public: ShowAlertStatus — Module 3                                   */
/*                                                                      */
/* Label digits 8..5: A L T :                                          */
/* Value digits 4..1:   AL  or  OK                                     */
/* ================================================================== */

void MAX7219_ShowAlertStatus(bool alert) {
    /* All raw — Code-B has no A, K */
    writeOne(3U, REG_DECODE_MODE, 0x00U);

    /* Label: A L T : */
    writeOne(3U, REG_DIGIT(8), SEG_A);
    writeOne(3U, REG_DIGIT(7), SEG_L);
    writeOne(3U, REG_DIGIT(6), SEG_T);
    writeOne(3U, REG_DIGIT(5), SEG_COLON);

    if (alert) {
        /* " AL" */
        writeOne(3U, REG_DIGIT(4), SEG_BLANK);
        writeOne(3U, REG_DIGIT(3), SEG_BLANK);
        writeOne(3U, REG_DIGIT(2), SEG_A);
        writeOne(3U, REG_DIGIT(1), SEG_L);
    } else {
        /* " OK" */
        writeOne(3U, REG_DIGIT(4), SEG_BLANK);
        writeOne(3U, REG_DIGIT(3), SEG_BLANK);
        writeOne(3U, REG_DIGIT(2), SEG_O);
        writeOne(3U, REG_DIGIT(1), SEG_BLANK); /* 7-seg can't do K cleanly; use blank */
        /*
         * Displaying full "OK": 'O' is fine; 'K' is not standard on 7-seg.
         * Best 7-seg approximation of K is 0x76 (same as H), which is
         * ambiguous. Using SEG_H for K is common practice:
         */
        writeOne(3U, REG_DIGIT(1), SEG_H);     /* 'K' approximated as H */
    }
}

/* ================================================================== */
/* Public: ShowLightAdc — Module 2                                      */
/*                                                                      */
/* Label digits 8..5: L I T :                                          */
/* Value digits 4..1: 0-4095 (leading zeros suppressed)                */
/* ================================================================== */

void MAX7219_ShowLightAdc(uint16_t lightAdc) {
    uint8_t value[4];
    decimalToCodeB(lightAdc, value);

    /* Label: all raw; value: Code-B on lower 4 */
    writeOne(2U, REG_DECODE_MODE, DECODE_LOWER_ONLY);

    writeOne(2U, REG_DIGIT(8), SEG_L);
    writeOne(2U, REG_DIGIT(7), SEG_I);
    writeOne(2U, REG_DIGIT(6), SEG_T);
    writeOne(2U, REG_DIGIT(5), SEG_COLON);

    writeOne(2U, REG_DIGIT(4), value[0]);
    writeOne(2U, REG_DIGIT(3), value[1]);
    writeOne(2U, REG_DIGIT(2), value[2]);
    writeOne(2U, REG_DIGIT(1), value[3]);
}

/* ================================================================== */
/* Public: ShowMicP2P — Module 1 (rightmost)                           */
/*                                                                      */
/* Label digits 8..5: n I C :                                          */
/* Value digits 4..1: 0-4095 (leading zeros suppressed)                */
/* ================================================================== */

void MAX7219_ShowMicP2P(uint16_t micP2P) {
    uint8_t value[4];
    decimalToCodeB(micP2P, value);

    /* Label: all raw (no 'M' in Code-B, 'n' is raw too); value: Code-B */
    writeOne(1U, REG_DECODE_MODE, DECODE_LOWER_ONLY);

    writeOne(1U, REG_DIGIT(8), SEG_N);      /* 'n' (small n = best 'M' approx on 7-seg) */
    writeOne(1U, REG_DIGIT(7), SEG_I);
    writeOne(1U, REG_DIGIT(6), SEG_C);
    writeOne(1U, REG_DIGIT(5), SEG_COLON);

    writeOne(1U, REG_DIGIT(4), value[0]);
    writeOne(1U, REG_DIGIT(3), value[1]);
    writeOne(1U, REG_DIGIT(2), value[2]);
    writeOne(1U, REG_DIGIT(1), value[3]);
}

/* ================================================================== */
/* Public: ShowAll                                                      */
/* ================================================================== */

void MAX7219_ShowAll(bool started, bool alert,
                     uint16_t lightAdc, uint16_t micP2P) {
    MAX7219_ShowSystemStatus(started);
    MAX7219_ShowAlertStatus(alert);
    MAX7219_ShowLightAdc(lightAdc);
    MAX7219_ShowMicP2P(micP2P);
}
