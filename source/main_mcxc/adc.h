#ifndef ADC_H
#define ADC_H

#include <stdint.h>

#define PHOTO_ADC_CH    8   // PTB0 = ADC0_SE8
#define MIC_ADC_CH      9   // PTB1 = ADC0_SE9

void     ADC_Init(void);
uint16_t ADC_ReadChannel(uint8_t ch);
void     ADC_ReadMicWindow(uint16_t *micRaw, uint16_t *micMin,
                           uint16_t *micMax,  uint16_t *micP2P);

#endif
