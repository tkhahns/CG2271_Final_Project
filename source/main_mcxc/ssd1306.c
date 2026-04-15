/*
 * ssd1306.c — SSD1306 128x64 OLED driver (I2C) for FRDM-MCXC444
 *
 * Uses I2C1 hardware peripheral:
 *   PTC1  ALT2  I2C1_SCL   → Arduino J4 pin 12
 *   PTC2  ALT2  I2C1_SDA   → Arduino J4 pin 10
 *
 */

#include "ssd1306.h"
#include "MCXC444.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

// I2C Configuration
// I2C baud rate: F_BUS / ((ICR multiplier) * prescaler)

#define I2C_ICR_100KHZ   0x1FU
#define OLED_CHARS_PER_LINE  21


static uint8_t s_frameBuf[SSD1306_HEIGHT / 8U][SSD1306_WIDTH];

/* ================================================================== */
/* 5×7 font (ASCII 0x20..0x7E)                                         */
/* Each character = 5 columns of 8 bits, MSB = bottom pixel.           */
/* Source: classic public-domain 5x7 bitmap font.                      */
/* ================================================================== */
static const uint8_t k_font5x7[][5] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00 }, /*   space */
    { 0x00, 0x00, 0x5F, 0x00, 0x00 }, /* ! */
    { 0x00, 0x07, 0x00, 0x07, 0x00 }, /* " */
    { 0x14, 0x7F, 0x14, 0x7F, 0x14 }, /* # */
    { 0x24, 0x2A, 0x7F, 0x2A, 0x12 }, /* $ */
    { 0x23, 0x13, 0x08, 0x64, 0x62 }, /* % */
    { 0x36, 0x49, 0x55, 0x22, 0x50 }, /* & */
    { 0x00, 0x05, 0x03, 0x00, 0x00 }, /* ' */
    { 0x00, 0x1C, 0x22, 0x41, 0x00 }, /* ( */
    { 0x00, 0x41, 0x22, 0x1C, 0x00 }, /* ) */
    { 0x08, 0x2A, 0x1C, 0x2A, 0x08 }, /* * */
    { 0x08, 0x08, 0x3E, 0x08, 0x08 }, /* + */
    { 0x00, 0x50, 0x30, 0x00, 0x00 }, /* , */
    { 0x08, 0x08, 0x08, 0x08, 0x08 }, /* - */
    { 0x00, 0x60, 0x60, 0x00, 0x00 }, /* . */
    { 0x20, 0x10, 0x08, 0x04, 0x02 }, /* / */
    { 0x3E, 0x51, 0x49, 0x45, 0x3E }, /* 0 */
    { 0x00, 0x42, 0x7F, 0x40, 0x00 }, /* 1 */
    { 0x42, 0x61, 0x51, 0x49, 0x46 }, /* 2 */
    { 0x21, 0x41, 0x45, 0x4B, 0x31 }, /* 3 */
    { 0x18, 0x14, 0x12, 0x7F, 0x10 }, /* 4 */
    { 0x27, 0x45, 0x45, 0x45, 0x39 }, /* 5 */
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, /* 6 */
    { 0x01, 0x71, 0x09, 0x05, 0x03 }, /* 7 */
    { 0x36, 0x49, 0x49, 0x49, 0x36 }, /* 8 */
    { 0x06, 0x49, 0x49, 0x29, 0x1E }, /* 9 */
    { 0x00, 0x36, 0x36, 0x00, 0x00 }, /* : */
    { 0x00, 0x56, 0x36, 0x00, 0x00 }, /* ; */
    { 0x00, 0x08, 0x14, 0x22, 0x41 }, /* < */
    { 0x14, 0x14, 0x14, 0x14, 0x14 }, /* = */
    { 0x41, 0x22, 0x14, 0x08, 0x00 }, /* > */
    { 0x02, 0x01, 0x51, 0x09, 0x06 }, /* ? */
    { 0x32, 0x49, 0x79, 0x41, 0x3E }, /* @ */
    { 0x7E, 0x11, 0x11, 0x11, 0x7E }, /* A */
    { 0x7F, 0x49, 0x49, 0x49, 0x36 }, /* B */
    { 0x3E, 0x41, 0x41, 0x41, 0x22 }, /* C */
    { 0x7F, 0x41, 0x41, 0x22, 0x1C }, /* D */
    { 0x7F, 0x49, 0x49, 0x49, 0x41 }, /* E */
    { 0x7F, 0x09, 0x09, 0x09, 0x01 }, /* F */
    { 0x3E, 0x41, 0x41, 0x51, 0x32 }, /* G */
    { 0x7F, 0x08, 0x08, 0x08, 0x7F }, /* H */
    { 0x00, 0x41, 0x7F, 0x41, 0x00 }, /* I */
    { 0x20, 0x40, 0x41, 0x3F, 0x01 }, /* J */
    { 0x7F, 0x08, 0x14, 0x22, 0x41 }, /* K */
    { 0x7F, 0x40, 0x40, 0x40, 0x40 }, /* L */
    { 0x7F, 0x02, 0x04, 0x02, 0x7F }, /* M */
    { 0x7F, 0x04, 0x08, 0x10, 0x7F }, /* N */
    { 0x3E, 0x41, 0x41, 0x41, 0x3E }, /* O */
    { 0x7F, 0x09, 0x09, 0x09, 0x06 }, /* P */
    { 0x3E, 0x41, 0x51, 0x21, 0x5E }, /* Q */
    { 0x7F, 0x09, 0x19, 0x29, 0x46 }, /* R */
    { 0x46, 0x49, 0x49, 0x49, 0x31 }, /* S */
    { 0x01, 0x01, 0x7F, 0x01, 0x01 }, /* T */
    { 0x3F, 0x40, 0x40, 0x40, 0x3F }, /* U */
    { 0x1F, 0x20, 0x40, 0x20, 0x1F }, /* V */
    { 0x3F, 0x40, 0x38, 0x40, 0x3F }, /* W */
    { 0x63, 0x14, 0x08, 0x14, 0x63 }, /* X */
    { 0x03, 0x04, 0x78, 0x04, 0x03 }, /* Y */
    { 0x61, 0x51, 0x49, 0x45, 0x43 }, /* Z */
    { 0x00, 0x00, 0x7F, 0x41, 0x41 }, /* [ */
    { 0x02, 0x04, 0x08, 0x10, 0x20 }, /* \ */
    { 0x41, 0x41, 0x7F, 0x00, 0x00 }, /* ] */
    { 0x04, 0x02, 0x01, 0x02, 0x04 }, /* ^ */
    { 0x40, 0x40, 0x40, 0x40, 0x40 }, /* _ */
    { 0x00, 0x01, 0x02, 0x04, 0x00 }, /* ` */
    { 0x20, 0x54, 0x54, 0x54, 0x78 }, /* a */
    { 0x7F, 0x48, 0x44, 0x44, 0x38 }, /* b */
    { 0x38, 0x44, 0x44, 0x44, 0x20 }, /* c */
    { 0x38, 0x44, 0x44, 0x48, 0x7F }, /* d */
    { 0x38, 0x54, 0x54, 0x54, 0x18 }, /* e */
    { 0x08, 0x7E, 0x09, 0x01, 0x02 }, /* f */
    { 0x08, 0x54, 0x54, 0x54, 0x3C }, /* g */
    { 0x7F, 0x08, 0x04, 0x04, 0x78 }, /* h */
    { 0x00, 0x44, 0x7D, 0x40, 0x00 }, /* i */
    { 0x20, 0x40, 0x44, 0x3D, 0x00 }, /* j */
    { 0x00, 0x7F, 0x10, 0x28, 0x44 }, /* k */
    { 0x00, 0x41, 0x7F, 0x40, 0x00 }, /* l */
    { 0x7C, 0x04, 0x18, 0x04, 0x78 }, /* m */
    { 0x7C, 0x08, 0x04, 0x04, 0x78 }, /* n */
    { 0x38, 0x44, 0x44, 0x44, 0x38 }, /* o */
    { 0x7C, 0x14, 0x14, 0x14, 0x08 }, /* p */
    { 0x08, 0x14, 0x14, 0x18, 0x7C }, /* q */
    { 0x7C, 0x08, 0x04, 0x04, 0x08 }, /* r */
    { 0x48, 0x54, 0x54, 0x54, 0x20 }, /* s */
    { 0x04, 0x3F, 0x44, 0x40, 0x20 }, /* t */
    { 0x3C, 0x40, 0x40, 0x40, 0x7C }, /* u */
    { 0x1C, 0x20, 0x40, 0x20, 0x1C }, /* v */
    { 0x3C, 0x40, 0x30, 0x40, 0x3C }, /* w */
    { 0x44, 0x28, 0x10, 0x28, 0x44 }, /* x */
    { 0x0C, 0x50, 0x50, 0x50, 0x3C }, /* y */
    { 0x44, 0x64, 0x54, 0x4C, 0x44 }, /* z */
    { 0x00, 0x08, 0x36, 0x41, 0x00 }, /* { */
    { 0x00, 0x00, 0x7F, 0x00, 0x00 }, /* | */
    { 0x00, 0x41, 0x36, 0x08, 0x00 }, /* } */
    { 0x08, 0x04, 0x08, 0x10, 0x08 }, /* ~ */
};

#define FONT_FIRST_CHAR 0x20U
#define FONT_CHAR_W     5U
#define FONT_CHAR_H     8U
#define FONT_CHAR_GAP   1U   /* 1 pixel gap between characters */
#define FONT_CHAR_STEP  (FONT_CHAR_W + FONT_CHAR_GAP)

// I2C Helpers

static void i2c_start(void) {
    I2C1->C1 |= I2C_C1_MST_MASK;   // generate START, enter master mode
    I2C1->C1 |= I2C_C1_TX_MASK;    // transmit mode
}

static void i2c_stop(void) {
    I2C1->C1 &= ~I2C_C1_MST_MASK;  // generate STOP
    I2C1->C1 &= ~I2C_C1_TX_MASK;

    for (volatile uint32_t d = 0; d < 50U; d++) { __asm volatile ("nop"); }
}

static void i2c_waitAck(void) {
    // Wait for interrupt flag (byte transfer complete)
    while (!(I2C1->S & I2C_S_IICIF_MASK)) { }
    I2C1->S |= I2C_S_IICIF_MASK;   // clear flag (w1c)
}

static void i2c_writeByte(uint8_t data) {
    I2C1->D = data;
    i2c_waitAck();
}

// Send a buffer to the SSD1306 with the given control byte prefix
static void ssd1306_write(uint8_t ctrl, const uint8_t *buf, uint16_t len) {
    i2c_start();
    i2c_writeByte((uint8_t)(SSD1306_I2C_ADDR << 1U));  // address + write bit
    i2c_writeByte(ctrl);
    for (uint16_t i = 0; i < len; i++) {
        i2c_writeByte(buf[i]);
    }
    i2c_stop();
}

static void ssd1306_cmd(uint8_t cmd) {
    ssd1306_write(0x00U, &cmd, 1U);
}

static void ssd1306_cmd2(uint8_t cmd, uint8_t arg) {
    uint8_t buf[2] = { cmd, arg };
    ssd1306_write(0x00U, buf, 2U);
}

// Frame Buffer Logic
static void fb_setPixel(uint8_t x, uint8_t y, bool on) {
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;
    uint8_t page = y / 8U;
    uint8_t bit  = (uint8_t)(1U << (y % 8U));
    if (on) {
        s_frameBuf[page][x] |= bit;
    } else {
        s_frameBuf[page][x] &= (uint8_t)(~bit);
    }
}

/* Draw a single 5x7 character at pixel position (x, page).
 * page = 0..7, each page = 8 pixels tall.
 * Returns the x coordinate of the next character. */
static uint8_t fb_drawChar(uint8_t x, uint8_t page, char c) {
    if (c < (char)FONT_FIRST_CHAR ||
        c > (char)(FONT_FIRST_CHAR + (sizeof(k_font5x7) / sizeof(k_font5x7[0])) - 1U)) {
        c = ' ';
    }

    const uint8_t *glyph = k_font5x7[(uint8_t)c - FONT_FIRST_CHAR];

    for (uint8_t col = 0U; col < FONT_CHAR_W; col++) {
        uint8_t colData = glyph[col];
        if ((x + col) >= SSD1306_WIDTH) break;
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            fb_setPixel((uint8_t)(x + col),
                        (uint8_t)(page * 8U + bit),
                        (colData & (1U << bit)) != 0U);
        }
    }
    // Gap column
    if ((x + FONT_CHAR_W) < SSD1306_WIDTH) {
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            fb_setPixel((uint8_t)(x + FONT_CHAR_W),
                        (uint8_t)(page * 8U + bit), false);
        }
    }
    return (uint8_t)(x + FONT_CHAR_STEP);
}

// Draw a string at (x, page). Returns x after last character.
static uint8_t fb_drawString(uint8_t x, uint8_t page, const char *str) {
    while (str != NULL && *str != '\0' && x < SSD1306_WIDTH) {
        x = fb_drawChar(x, page, *str);
        str++;
    }
    return x;
}

// Draw a scaled-up character (scale = integer multiplier).
// Used for big number display.
static uint8_t fb_drawCharBig(uint8_t x, uint8_t startPage,
                               char c, uint8_t scale) {
    if (c < (char)FONT_FIRST_CHAR ||
        c > (char)(FONT_FIRST_CHAR + (sizeof(k_font5x7) / sizeof(k_font5x7[0])) - 1U)) {
        c = ' ';
    }
    const uint8_t *glyph = k_font5x7[(uint8_t)c - FONT_FIRST_CHAR];

    for (uint8_t col = 0U; col < FONT_CHAR_W; col++) {
        uint8_t colData = glyph[col];
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            bool on = (colData & (1U << bit)) != 0U;
            for (uint8_t sy = 0U; sy < scale; sy++) {
                uint8_t py = (uint8_t)(startPage * 8U + bit * scale + sy);
                for (uint8_t sx = 0U; sx < scale; sx++) {
                    uint8_t px = (uint8_t)(x + col * scale + sx);
                    fb_setPixel(px, py, on);
                }
            }
        }
    }
    /* gap */
    for (uint8_t sy = 0U; sy < (uint8_t)(8U * scale); sy++) {
        for (uint8_t sx = 0U; sx < scale; sx++) {
            fb_setPixel((uint8_t)(x + FONT_CHAR_W * scale + sx),
                        (uint8_t)(startPage * 8U + sy), false);
        }
    }
    return (uint8_t)(x + (FONT_CHAR_W + FONT_CHAR_GAP) * scale);
}

static uint8_t fb_drawStringBig(uint8_t x, uint8_t startPage,
                                 const char *str, uint8_t scale) {
    while (str != NULL && *str != '\0' && x < SSD1306_WIDTH) {
        x = fb_drawCharBig(x, startPage, *str, scale);
        str++;
    }
    return x;
}

// Draw a horizontal line across the full width at pixel row y
static void fb_hLine(uint8_t y) {
    for (uint8_t x = 0U; x < SSD1306_WIDTH; x++) {
        fb_setPixel(x, y, true);
    }
}

// Clear a page-row (8 pixel tall band)
static void fb_clearPage(uint8_t page) {
    memset(s_frameBuf[page], 0, SSD1306_WIDTH);
}

// Flush the entire frame buffer to the display
static void SSD1306_Flush(void) {
    ssd1306_cmd(0x21U);   // set column address
    ssd1306_cmd(0x00U);   // start = 0
    ssd1306_cmd(0x7FU);   // end   = 127
    ssd1306_cmd(0x22U);   // set page address
    ssd1306_cmd(0x00U);   // start page = 0
    ssd1306_cmd(0x07U);   // end page   = 7

    // Send all 1024 bytes as display data
    for (uint8_t page = 0U; page < (SSD1306_HEIGHT / 8U); page++) {
        ssd1306_write(0x40U, s_frameBuf[page], SSD1306_WIDTH);
    }
}

static void u16ToString(uint16_t val, char *buf, uint8_t bufLen) {
    /* Write digits right-to-left, then reverse */
    uint8_t i = 0;
    if (val == 0U) {
        buf[i++] = '0';
    } else {
        while (val > 0U && i < (bufLen - 1U)) {
            buf[i++] = (char)('0' + (val % 10U));
            val /= 10U;
        }
        /* reverse */
        for (uint8_t a = 0U, b = (uint8_t)(i - 1U); a < b; a++, b--) {
            char tmp = buf[a]; buf[a] = buf[b]; buf[b] = tmp;
        }
    }
    buf[i] = '\0';
}

void SSD1306_Init(void) {
    // Clock Gates
    SIM->SCGC4 |= SIM_SCGC4_I2C1_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;

    // Pin mux: PTC1 = SCL (ALT2), PTC2 = SDA (ALT2)
    // Open-drain with internal pull-ups enabled
    PORTC->PCR[1] = PORT_PCR_MUX(2) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTC->PCR[2] = PORT_PCR_MUX(2) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;


    //ICR=0x1F → ~100 kHz at 24 MHz bus clock
    // MULT=0   → multiplier = 1
    I2C1->F  = I2C_F_ICR(I2C_ICR_100KHZ) | I2C_F_MULT(0);
    I2C1->C1 = I2C_C1_IICEN_MASK;   /* enable I2C, master mode off until START */
    I2C1->C2 = 0x00U;
    I2C1->S  = I2C_S_IICIF_MASK;    /* clear any stale interrupt flag */

    // ---- SSD1306 initialisation sequence (128x64) ----
    ssd1306_cmd(0xAEU);             // display off
    ssd1306_cmd2(0xD5U, 0x80U);     // set display clock divide
    ssd1306_cmd2(0xA8U, 0x3FU);     // set multiplex ratio = 63 (64 rows)
    ssd1306_cmd2(0xD3U, 0x00U);     // set display offset = 0
    ssd1306_cmd(0x40U);             // set start line = 0
    ssd1306_cmd2(0x8DU, 0x14U);     // enable charge pump
    ssd1306_cmd2(0x20U, 0x00U);     // horizontal addressing mode
    ssd1306_cmd(0xA1U);             // segment remap (col 127 = SEG0)
    ssd1306_cmd(0xC8U);             // COM scan direction: remapped
    ssd1306_cmd2(0xDAU, 0x12U);     // COM pins config (alt, no remap)
    ssd1306_cmd2(0x81U, 0xCFU);     // contrast = 207
    ssd1306_cmd2(0xD9U, 0xF1U);     // pre-charge period
    ssd1306_cmd2(0xDBU, 0x40U);     // VCOMH deselect level
    ssd1306_cmd(0xA4U);             // display from RAM (not all-on)
    ssd1306_cmd(0xA6U);             // normal display (not inverted)

    SSD1306_Clear();

    ssd1306_cmd(0xAFU);
}


void SSD1306_Clear(void) {
    memset(s_frameBuf, 0, sizeof(s_frameBuf));
    SSD1306_Flush();
}

static void fb_drawWrapped(uint8_t startPage, uint8_t maxPages,
                            const char *str, uint8_t maxCols)
{
    char    lineBuf[32];
    uint8_t page = startPage;

    while (*str != '\0' && page < (startPage + maxPages))
    {
        uint8_t n = 0U;

        /* Count up to maxCols characters */
        while (n < maxCols && str[n] != '\0') { n++; }

        /* If we didn't reach the end, back up to the last space */
        if (str[n] != '\0' && n == maxCols)
        {
            uint8_t k = n;
            while (k > 0U && str[k] != ' ') { k--; }
            if (k > 0U) { n = k; }
        }

        memcpy(lineBuf, str, n);
        lineBuf[n] = '\0';
        fb_drawString(0U, page, lineBuf);

        str += n;
        if (*str == ' ') { str++; }   /* skip the break space */
        page++;
    }
}

/* ================================================================== */
/* Public                                                     */
/*                                                                      */
/* Screen layout (each row = 1 page = 8px tall):                       */
/*                                                                      */
/*   Page 0: "SYS:  ON " or "SYS: OFF"   (small font)                 */
/*   Page 1: "ALT:  OK " or "ALT: ***"   (small font, inverted if AL) */
/*   Page 2: horizontal divider line                                   */
/*   Page 3: "LIGHT"                      (small font label)           */
/*   Page 4-5: value e.g. "2048"          (2x scaled font)             */
/*   Page 6: "MIC P2P"                   (small font label)            */
/*   Page 7: value e.g. "  42"            (small font, fits one page)  */
/* ================================================================== */
void SSD1306_ShowAll(bool started,
                     bool alert,
                     uint16_t lightAdc,
                     uint16_t micP2P,
                     uint8_t activeCount,
                     bool showSuggestionScreen,
                     float temperatureC,
                     bool temperatureValid,
                     float distanceCm,
                     bool distanceValid,
                     bool suggestionReady,
                     const char *suggestionBuf)
{
    char numBuf[16];

    memset(s_frameBuf, 0, sizeof(s_frameBuf));

    if (!started) {
        fb_drawString(16U, 3U, "System stopped");
        fb_drawString(28U, 4U, "Press SW2");
        SSD1306_Flush();
        return;
    }

    fb_drawString(0U,  0U, "SYS: ON");
    fb_drawString(56U, 0U, "ALT:");
    fb_drawString(86U, 0U, alert ? "YES" : "NO ");

    if (showSuggestionScreen)
    {
        fb_drawString(0U, 1U, "VIEW: SUGGESTION");
        fb_hLine(19U);

        if (activeCount >= 3U)
        {
            fb_drawString(0U, 3U, "Go check your");
            fb_drawString(0U, 4U, "phone for");
            fb_drawString(0U, 5U, "Telegram message");
        }
        else if (suggestionReady &&
            suggestionBuf != NULL &&
            suggestionBuf[0] != '\0')
        {
            /* pages 3-7 = 5 lines of 21 chars each */
            fb_drawWrapped(3U, 5U, suggestionBuf, OLED_CHARS_PER_LINE);
        }
        else
        {
            fb_drawString(0U, 3U, "You are good now");
            fb_drawString(0U, 5U, "Press SW3 to toggle");
        }
    }
    else
    {
        fb_drawString(0U, 1U, "VIEW: SENSORS");
        fb_hLine(19U);

        fb_drawString(0U,  3U, "LIGHT:");
        u16ToString(lightAdc, numBuf, sizeof(numBuf));
        fb_drawString(42U, 3U, numBuf);

        fb_drawString(0U,  4U, "SOUND:");
        u16ToString(micP2P, numBuf, sizeof(numBuf));
        fb_drawString(42U, 4U, numBuf);

        fb_drawString(0U,  5U, "CNT:");
        u16ToString(activeCount, numBuf, sizeof(numBuf));
        fb_drawString(30U, 5U, numBuf);

        fb_drawString(0U,  6U, "TEMP:");
        if (temperatureValid) {
            const int tempDeci = (int)(temperatureC * 10.0f);
            (void)snprintf(numBuf, sizeof(numBuf), "%d.%dC",
                           tempDeci / 10, abs(tempDeci % 10));
            fb_drawString(36U, 6U, numBuf);
        } else {
            fb_drawString(36U, 6U, "N/A");
        }

        fb_drawString(0U,  7U, "DIST:");
        if (distanceValid) {
            const int distDeci = (int)(distanceCm * 10.0f);
            (void)snprintf(numBuf, sizeof(numBuf), "%d.%dcm",
                           distDeci / 10, abs(distDeci % 10));
            fb_drawString(36U, 7U, numBuf);
        } else {
            fb_drawString(36U, 7U, "N/A");
        }
    }

    SSD1306_Flush();
}

