#include "led.h"
#include "board.h"
#include "MCXC444.h"

#define RED_PIN     BOARD_LED_RED_GPIO_PIN
#define GREEN_PIN   BOARD_LED_GREEN_GPIO_PIN
#define BLUE_PIN    BOARD_LED_BLUE_GPIO_PIN

void LED_Init(void) {
    SIM->SCGC5 |= (SIM_SCGC5_PORTD_MASK | SIM_SCGC5_PORTE_MASK);

    PORTE->PCR[RED_PIN] = PORT_PCR_MUX(1);
    PORTE->PCR[BLUE_PIN] = PORT_PCR_MUX(1);
    PORTD->PCR[GREEN_PIN] = PORT_PCR_MUX(1);

    GPIOE->PDDR |= (1U << RED_PIN) | (1U << BLUE_PIN);
    GPIOD->PDDR |= (1U << GREEN_PIN);

    LED_OffAll();
}

void LED_On(TLED led) {
    switch (led) {
        case RED:   GPIOE->PCOR = (1U << RED_PIN); break;
        case GREEN: GPIOD->PCOR = (1U << GREEN_PIN); break;
        case BLUE:  GPIOE->PCOR = (1U << BLUE_PIN); break;
    }
}

void LED_Off(TLED led) {
    switch (led) {
        case RED:   GPIOE->PSOR = (1U << RED_PIN); break;
        case GREEN: GPIOD->PSOR = (1U << GREEN_PIN); break;
        case BLUE:  GPIOE->PSOR = (1U << BLUE_PIN); break;
    }
}

void LED_OffAll(void) {
    LED_Off(RED);
    LED_Off(GREEN);
    LED_Off(BLUE);
}

void LED_SetRGB(bool r, bool g, bool b) {
    if (r) LED_On(RED); else LED_Off(RED);
    if (g) LED_On(GREEN); else LED_Off(GREEN);
    if (b) LED_On(BLUE); else LED_Off(BLUE);
}
