#include "slcd.h"
#include "MCXC444.h"
#include <string.h>


/* Back-plane (COM) pins */
#define LCD_COM0_PIN   59U   /* PTE20 */
#define LCD_COM1_PIN   60U   /* PTE21 */
#define LCD_COM2_PIN   14U   /* PTB18 */
#define LCD_COM3_PIN   15U   /* PTB19 */

/* Front-plane (data) pins */
#define LCD_D0_PIN     20U   /* PTC0  */
#define LCD_D1_PIN     24U   /* PTC4  */
#define LCD_D2_PIN     26U   /* PTC5  */
#define LCD_D3_PIN     27U   /* PTC6  */
#define LCD_D4_PIN     40U   /* PTD0  */
#define LCD_D5_PIN     42U   /* PTD2  */
#define LCD_D6_PIN     43U   /* PTD3  */
#define LCD_D7_PIN     44U   /* PTD4  */

/* Arrays for loop access */
static const uint8_t k_comPins[4] = {
    LCD_COM0_PIN, LCD_COM1_PIN, LCD_COM2_PIN, LCD_COM3_PIN
};
static const uint8_t k_dataPins[8] = {
    LCD_D0_PIN, LCD_D1_PIN, LCD_D2_PIN, LCD_D3_PIN,
    LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN
};

/* ================================================================== */
/* Segment encode table (from AN12579 Table 2)                         */
/*                                                                      */
/* Each row = one character, four bytes = {COM0, COM1, COM2, COM3}.    */
/* Within each 2-bit field, bit0 = odd digit (1,3), bit1 = even (0,2). */
/* The slcd_set_bus_data() function places these into WF8B correctly.  */
/*                                                                      */
/*   Index:  0-9 = digits, 10 = blank, 11 = dash, 12 = DP              */
/* ================================================================== */
#define SLCD_NUM_CHARS  13U
#define SLCD_COM_COUNT   4U

static const uint8_t k_segTable[SLCD_NUM_CHARS][SLCD_COM_COUNT] = {
/*         COM0  COM1  COM2  COM3  */
/* 0  */ { 0x1,  0x3,  0x2,  0x3 },
/* 1  */ { 0x0,  0x2,  0x2,  0x0 },
/* 2  */ { 0x1,  0x1,  0x3,  0x2 },
/* 3  */ { 0x1,  0x2,  0x3,  0x2 },
/* 4  */ { 0x0,  0x2,  0x3,  0x1 },
/* 5  */ { 0x1,  0x2,  0x1,  0x3 },
/* 6  */ { 0x1,  0x3,  0x1,  0x3 },
/* 7  */ { 0x0,  0x2,  0x2,  0x2 },
/* 8  */ { 0x1,  0x3,  0x3,  0x3 },
/* 9  */ { 0x1,  0x2,  0x3,  0x3 },
/* 10 blank */ { 0x0, 0x0, 0x0, 0x0 },
/* 11 dash  */ { 0x0, 0x0, 0x1, 0x0 },  /* G segment only */
/* 12 DP    */ { 0x2, 0x0, 0x0, 0x0 },  /* decimal point  */
};

#define SLCD_CHAR_DP_IDX 12U

/* Shadow of current WF8B state for all 4 digit positions */
static uint8_t s_digitCodes[SLCD_COM_COUNT];

/* ================================================================== */
/* Internal: write one COM-step's 8-bit data word into WF8B registers  */
/*                                                                      */
/* The data bus is 8 pins wide. Each pin holds one bit for each of the */
/* 4 COM steps. This function takes the combined 8-bit bus value for   */
/* one COM step and distributes each bit to its pin's WF8B register.   */
/* ================================================================== */
static void slcd_setBusData(uint8_t comIdx, uint8_t busData) {
    uint8_t bitMask = (uint8_t)(1U << comIdx);

    for (uint8_t i = 0U; i < 8U; i++) {
        if (busData & 0x1U) {
            LCD->WF8B[k_dataPins[i]] |= bitMask;
        } else {
            LCD->WF8B[k_dataPins[i]] &= (uint8_t)(~bitMask);
        }
        busData >>= 1U;
    }
}

/* ================================================================== */
/* Internal: flush s_digitCodes[] shadow to all WF8B registers         */
/* ================================================================== */
static void slcd_flush(void) {
    for (uint8_t com = 0U; com < SLCD_COM_COUNT; com++) {
        slcd_setBusData(com, s_digitCodes[com]);
    }
}

/* ================================================================== */
/* SLCD_Init                                                            */
/* ================================================================== */
void SLCD_Init(void) {
    /* ---- Clock gates ---- */
    SIM->SCGC5 |= SIM_SCGC5_SLCD_MASK    /* LCD controller  */
               |  SIM_SCGC5_PORTB_MASK   /* PTB18, PTB19    */
               |  SIM_SCGC5_PORTC_MASK   /* PTC0,4,5,6      */
               |  SIM_SCGC5_PORTD_MASK   /* PTD0,2,3,4      */
               |  SIM_SCGC5_PORTE_MASK;  /* PTE20, PTE21    */

    /* ---- Pin mux: all LCD pins → ALT0 (analog/LCD function) ---- */
    /* COM pins */
    PORTE->PCR[20] = PORT_PCR_MUX(0);    /* PTE20 → LCD_P59 COM0 */
    PORTE->PCR[21] = PORT_PCR_MUX(0);    /* PTE21 → LCD_P60 COM1 */
    PORTB->PCR[18] = PORT_PCR_MUX(0);    /* PTB18 → LCD_P14 COM2 */
    PORTB->PCR[19] = PORT_PCR_MUX(0);    /* PTB19 → LCD_P15 COM3 */

    /* Data pins */
    PORTC->PCR[0]  = PORT_PCR_MUX(0);    /* PTC0  → LCD_P20 D0   */
    PORTC->PCR[4]  = PORT_PCR_MUX(0);    /* PTC4  → LCD_P24 D1   */
    PORTC->PCR[5]  = PORT_PCR_MUX(0);    /* PTC5  → LCD_P26 D2   */
    PORTC->PCR[6]  = PORT_PCR_MUX(0);    /* PTC6  → LCD_P27 D3   */
    PORTD->PCR[0]  = PORT_PCR_MUX(0);    /* PTD0  → LCD_P40 D4   */
    PORTD->PCR[2]  = PORT_PCR_MUX(0);    /* PTD2  → LCD_P42 D5   */
    PORTD->PCR[3]  = PORT_PCR_MUX(0);    /* PTD3  → LCD_P43 D6   */
    PORTD->PCR[4]  = PORT_PCR_MUX(0);    /* PTD4  → LCD_P44 D7   */

    /* ---- LCD General Control Register ----
     * DUTY=3    : 4 back-plane (COM) lines
     * LCLK=2    : LCD clock prescaler (divides the 32kHz oscillator)
     * SOURCE=0  : use default 32.768 kHz oscillator (Y1 on the board)
     * LCDEN=0   : keep disabled while configuring
     * CPSEL=1   : use internal charge pump (JP7 must be OPEN — see note in header)
     * LADJ=1    : glass capacitance adjust
     * VSUPPLY=0 : drive LCD from internal VDD
     * RVTRIM=8  : regulated voltage trim (mid-range)
     * RVEN=0    : regulated voltage disabled
     * LCDDOZE=0 : stay active in DOZE mode
     * LCDSTP=0  : stay active in STOP mode
     */
    LCD->GCR = LCD_GCR_DUTY(3)
             | LCD_GCR_LCLK(2)
             | LCD_GCR_SOURCE(0)
             | LCD_GCR_LCDEN(0)
             | LCD_GCR_LCDSTP(0)
             | LCD_GCR_LCDDOZE(0)
             | LCD_GCR_FFR(0)
             | LCD_GCR_ALTSOURCE(0)
             | LCD_GCR_ALTDIV(0)
             | LCD_GCR_FDCIEN(0)
             | LCD_GCR_PADSAFE(0)
             | LCD_GCR_VSUPPLY(0)
             | LCD_GCR_LADJ(1)
             | LCD_GCR_CPSEL(1)
             | LCD_GCR_RVTRIM(8)
             | LCD_GCR_RVEN(0);

    /* ---- Enable LCD pins in LCD_PENx registers ----
     * PEN0 covers pins 0–31, PEN1 covers pins 32–63.
     * Set a bit to enable that pin for LCD use.
     */
    LCD->PEN[0] = 0U;
    LCD->PEN[1] = 0U;

    /* Front-plane (data) pins */
    LCD->PEN[0] |= (1UL << LCD_D0_PIN)   /* P20 */
                |  (1UL << LCD_D1_PIN)   /* P24 */
                |  (1UL << LCD_D2_PIN)   /* P26 */
                |  (1UL << LCD_D3_PIN);  /* P27 */

    LCD->PEN[1] |= (1UL << (LCD_D4_PIN - 32U))   /* P40 */
                |  (1UL << (LCD_D5_PIN - 32U))   /* P42 */
                |  (1UL << (LCD_D6_PIN - 32U))   /* P43 */
                |  (1UL << (LCD_D7_PIN - 32U));  /* P44 */

    /* Back-plane (COM) pins */
    LCD->PEN[0] |= (1UL << LCD_COM2_PIN)   /* P14 */
                |  (1UL << LCD_COM3_PIN);  /* P15 */

    LCD->PEN[1] |= (1UL << (LCD_COM0_PIN - 32U))   /* P59 */
                |  (1UL << (LCD_COM1_PIN - 32U));   /* P60 */

    /* ---- Back-plane enable (BPEN) — marks COM pins as back-planes ---- */
    LCD->BPEN[0] = 0U;
    LCD->BPEN[1] = 0U;

    LCD->BPEN[0] |= (1UL << LCD_COM2_PIN)   /* P14 = COM2 */
                 |  (1UL << LCD_COM3_PIN);  /* P15 = COM3 */

    LCD->BPEN[1] |= (1UL << (LCD_COM0_PIN - 32U))   /* P59 = COM0 */
                 |  (1UL << (LCD_COM1_PIN - 32U));   /* P60 = COM1 */

    /* ---- Configure back-plane waveforms ----
     * Each COM pin's WF8B register controls which steps it is active.
     * For a 4-COM display, COM0 is active in step 0, COM1 in step 1, etc.
     * Set bit N in COMn's WF8B to make it drive during step N.
     */
    LCD->WF8B[LCD_COM0_PIN] = 0x01U;   /* active in step 0 */
    LCD->WF8B[LCD_COM1_PIN] = 0x02U;   /* active in step 1 */
    LCD->WF8B[LCD_COM2_PIN] = 0x04U;   /* active in step 2 */
    LCD->WF8B[LCD_COM3_PIN] = 0x08U;   /* active in step 3 */

    /* ---- Clear all front-plane waveform registers ---- */
    for (uint8_t i = 0U; i < 8U; i++) {
        LCD->WF8B[k_dataPins[i]] = 0x00U;
    }

    /* Clear shadow state */
    memset(s_digitCodes, 0, sizeof(s_digitCodes));

    SLCD_Start();
}

/* ================================================================== */
/* SLCD_Start / SLCD_Stop                                              */
/* ================================================================== */
void SLCD_Start(void) {
    LCD->GCR |= LCD_GCR_LCDEN_MASK;
}

void SLCD_Stop(void) {
    LCD->GCR &= ~LCD_GCR_LCDEN_MASK;
}

/* ================================================================== */
/* SLCD_SetDigit                                                        */
/* ================================================================== */
void SLCD_SetDigit(uint8_t pos, uint8_t value, bool dot) {
    if (pos > 3U) return;
    if (value >= SLCD_NUM_CHARS) value = SLCD_CHAR_BLANK;

    /* Each digit occupies a 2-bit field in each COM's bus byte.
     * Digit 0 → bits [1:0], digit 1 → bits [3:2],
     * digit 2 → bits [5:4], digit 3 → bits [7:6].
     */
    uint8_t shift  = (uint8_t)(2U * pos);
    uint8_t mask   = (uint8_t)(0x03U << shift);

    for (uint8_t com = 0U; com < SLCD_COM_COUNT; com++) {
        uint8_t code = (uint8_t)(k_segTable[value][com] << shift);

        if (dot) {
            code |= (uint8_t)(k_segTable[SLCD_CHAR_DP_IDX][com] << shift);
        }

        s_digitCodes[com] = (uint8_t)((s_digitCodes[com] & ~mask) | (code & mask));
    }

    slcd_flush();
}

/* ================================================================== */
/* SLCD_Clear                                                           */
/* ================================================================== */
void SLCD_Clear(void) {
    memset(s_digitCodes, 0, sizeof(s_digitCodes));
    slcd_flush();
}

/* ================================================================== */
/* SLCD_ShowNumber                                                      */
/* ================================================================== */
void SLCD_ShowNumber(uint16_t value) {
    SLCD_Stop();

    if (value > 9999U) {
        /* Show "----" */
        for (uint8_t i = 0U; i < 4U; i++) {
            SLCD_SetDigit(i, SLCD_CHAR_DASH, false);
        }
        SLCD_Start();
        return;
    }

    uint8_t digits[4];
    digits[0] = (uint8_t)(value / 1000U);
    digits[1] = (uint8_t)((value / 100U) % 10U);
    digits[2] = (uint8_t)((value / 10U)  % 10U);
    digits[3] = (uint8_t)(value % 10U);

    bool leading = true;
    memset(s_digitCodes, 0, sizeof(s_digitCodes));

    for (uint8_t i = 0U; i < 4U; i++) {
        if (leading && digits[i] == 0U && i < 3U) {
            SLCD_SetDigit(i, SLCD_CHAR_BLANK, false);
        } else {
            leading = false;
            SLCD_SetDigit(i, digits[i], false);
        }
    }

    SLCD_Start();
}

/* ================================================================== */
/* SLCD_ShowString                                                      */
/* ================================================================== */
void SLCD_ShowString(const char *str) {
    if (str == NULL) return;

    SLCD_Stop();
    memset(s_digitCodes, 0, sizeof(s_digitCodes));

    for (uint8_t i = 0U; i < 4U; i++) {
        char c = str[i];
        if (c == '\0') {
            SLCD_SetDigit(i, SLCD_CHAR_BLANK, false);
        } else if (c >= '0' && c <= '9') {
            SLCD_SetDigit(i, (uint8_t)(c - '0'), false);
        } else if (c == '-') {
            SLCD_SetDigit(i, SLCD_CHAR_DASH, false);
        } else {
            SLCD_SetDigit(i, SLCD_CHAR_BLANK, false);
        }
    }

    SLCD_Start();
}
