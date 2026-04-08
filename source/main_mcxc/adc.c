#include "adc.h"
#include "MCXC444.h"
#include <stdbool.h>

static volatile uint16_t s_adcValue = 0;
static volatile bool     s_adcDone  = false;
static volatile bool     s_adcInitialized = false;

void ADC_Init(void) {
    if (s_adcInitialized) {
        return;
    }

    NVIC_DisableIRQ(ADC0_IRQn);

    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;

    /* PTB0 -> ADC0_SE8, PTB1 -> ADC0_SE9: analog (MUX=0) */
    PORTB->PCR[0] = PORT_PCR_MUX(0);
    PORTB->PCR[1] = PORT_PCR_MUX(0);

    /* Disable channel (ADCH=11111b = disabled) */
    ADC0->SC1[0] = ADC_SC1_AIEN_MASK | ADC_SC1_ADCH(0x1F);

    /* 12-bit single-ended, bus clock / 1 */
    ADC0->CFG1 = ADC_CFG1_MODE(0b01);

    /* Software trigger, alternate voltage reference (VREFH/VREFL) */
    ADC0->SC2 = ADC_SC2_REFSEL(0b01);
    ADC0->SC3 = 0;

    NVIC_SetPriority(ADC0_IRQn, 2);
    NVIC_ClearPendingIRQ(ADC0_IRQn);
    NVIC_EnableIRQ(ADC0_IRQn);
    s_adcInitialized = true;
}

void ADC0_IRQHandler(void) {
    if (ADC0->SC1[0] & ADC_SC1_COCO_MASK) {
        s_adcValue = (uint16_t)ADC0->R[0];
        s_adcDone  = true;
    }
}

uint16_t ADC_ReadChannel(uint8_t ch) {
    s_adcDone = false;
    ADC0->SC1[0] = ADC_SC1_AIEN_MASK | ADC_SC1_ADCH(ch);
    while (!s_adcDone) { /* wait for ISR */ }
    return s_adcValue;
}

void ADC_ReadMicWindow(uint16_t *micRaw, uint16_t *micMin,
                       uint16_t *micMax,  uint16_t *micP2P) {
    uint16_t v;
    uint16_t minVal = 0xFFFFU;
    uint16_t maxVal = 0U;

    for (int i = 0; i < 200; i++) {
        v = ADC_ReadChannel(MIC_ADC_CH);
        if (v < minVal) minVal = v;
        if (v > maxVal) maxVal = v;
    }

    *micRaw  = ADC_ReadChannel(MIC_ADC_CH);
    *micMin  = minVal;
    *micMax  = maxVal;
    *micP2P  = maxVal - minVal;
}
