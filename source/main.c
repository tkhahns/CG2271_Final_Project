/*
 * Copyright 2016-2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* =========================================================
   PIN DEFINITIONS
   ========================================================= */

#define RED_PIN                 30    /* PTE30 */
#define GREEN_PIN               5     /* PTD5  */
#define BLUE_PIN                29    /* PTE29 */


#define PHOTO_ADC_CH            8     /* PTB0 = ADC0_SE8 */
#define MIC_ADC_CH              9     /* PTB1 = ADC0_SE9 */


#define SW2_START_PIN           3     /* PTC3 = SW2 */
#define SW3_ACK_PIN             4    /* PTA4 = SW3 */

#define SW2_START_IRQ           PORTC_PORTD_IRQn
#define SW3_ACK_IRQ             PORTA_IRQn

#define MIC_P2P_THRESHOLD       9
#define DEBOUNCE_MS             200

/* =========================================================
   TYPES
   ========================================================= */

typedef enum {
    RED, GREEN, BLUE
} TLED;

typedef struct {
    uint16_t lightRaw;
    uint16_t micRaw;
    uint16_t micMin;
    uint16_t micMax;
    uint16_t micP2P;
    uint8_t p2pFlag;
} SensorPacket;

/* =========================================================
   GLOBALS
   ========================================================= */

volatile uint16_t g_adcValue = 0;
volatile bool g_adcDone = false;


volatile bool g_sw2StartPending = false;
volatile bool g_sw3AckPending = false;


volatile TickType_t g_lastSw2IsrTick = 0;
volatile TickType_t g_lastSw3IsrTick = 0;


bool g_systemStarted = false;
bool g_alertLatched = false;
SensorPacket g_latestPacket;

QueueHandle_t sensorQueue;
SemaphoreHandle_t buttonSema;
SemaphoreHandle_t statusMutex;

/* =========================================================
   LED FUNCTIONS
   ========================================================= */

void initLEDs(void) {
    SIM->SCGC5 |= (SIM_SCGC5_PORTD_MASK | SIM_SCGC5_PORTE_MASK);

    PORTE->PCR[RED_PIN] &= ~PORT_PCR_MUX_MASK;
    PORTE->PCR[RED_PIN] |= PORT_PCR_MUX(1);

    PORTE->PCR[BLUE_PIN] &= ~PORT_PCR_MUX_MASK;
    PORTE->PCR[BLUE_PIN] |= PORT_PCR_MUX(1);

    PORTD->PCR[GREEN_PIN] &= ~PORT_PCR_MUX_MASK;
    PORTD->PCR[GREEN_PIN] |= PORT_PCR_MUX(1);

    GPIOE->PDDR |= ((1 << RED_PIN) | (1 << BLUE_PIN));
    GPIOD->PDDR |= (1 << GREEN_PIN);
}

void offLED(TLED led) {
    switch (led) {
        case RED:   GPIOE->PSOR |= (1 << RED_PIN); break;
        case GREEN: GPIOD->PSOR |= (1 << GREEN_PIN); break;
        case BLUE:  GPIOE->PSOR |= (1 << BLUE_PIN); break;
    }
}

void onLED(TLED led) {
    switch (led) {
        case RED:   GPIOE->PCOR |= (1 << RED_PIN); break;
        case GREEN: GPIOD->PCOR |= (1 << GREEN_PIN); break;
        case BLUE:  GPIOE->PCOR |= (1 << BLUE_PIN); break;
    }
}

void offAllLEDs(void) {
    offLED(RED);
    offLED(GREEN);
    offLED(BLUE);
}

void setRGB(bool r, bool g, bool b) {
    if (r) onLED(RED); else offLED(RED);
    if (g) onLED(GREEN); else offLED(GREEN);
    if (b) onLED(BLUE); else offLED(BLUE);
}

/* =========================================================
   ADC FUNCTIONS
   ========================================================= */

void initADCInputs(void) {
    NVIC_DisableIRQ(ADC0_IRQn);

    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;


    PORTB->PCR[0] &= ~PORT_PCR_MUX_MASK;
    PORTB->PCR[0] |= PORT_PCR_MUX(0);


    PORTB->PCR[1] &= ~PORT_PCR_MUX_MASK;
    PORTB->PCR[1] |= PORT_PCR_MUX(0);


    ADC0->SC1[0] = ADC_SC1_AIEN_MASK | ADC_SC1_ADCH(0x1F);


    ADC0->CFG1 = 0;
    ADC0->CFG1 |= ADC_CFG1_MODE(0b01);


    ADC0->SC2 = 0;
    ADC0->SC2 |= ADC_SC2_REFSEL(0b01);

    ADC0->SC3 = 0;

    NVIC_SetPriority(ADC0_IRQn, 2);
    NVIC_ClearPendingIRQ(ADC0_IRQn);
    NVIC_EnableIRQ(ADC0_IRQn);
}

void ADC0_IRQHandler(void) {
    if (ADC0->SC1[0] & ADC_SC1_COCO_MASK) {
        g_adcValue = (uint16_t)ADC0->R[0];
        g_adcDone = true;
    }
}

uint16_t readADCChannel(uint8_t ch) {
    g_adcDone = false;
    ADC0->SC1[0] = ADC_SC1_AIEN_MASK | ADC_SC1_ADCH(ch);

    while (!g_adcDone) {
    }

    return g_adcValue;
}

void readMicWindow(uint16_t *micRaw, uint16_t *micMin, uint16_t *micMax, uint16_t *micP2P) {
    uint16_t v;
    uint16_t minVal = 0xFFFF;
    uint16_t maxVal = 0;

    for (int i = 0; i < 200; i++) {
        v = readADCChannel(MIC_ADC_CH);
        if (v < minVal) minVal = v;
        if (v > maxVal) maxVal = v;
    }

    *micRaw = readADCChannel(MIC_ADC_CH);
    *micMin = minVal;
    *micMax = maxVal;
    *micP2P = maxVal - minVal;
}

/* =========================================================
   BUTTON IRQ SETUP
   ========================================================= */

void initButtonIRQs(void) {
    NVIC_DisableIRQ(SW2_START_IRQ);
    NVIC_DisableIRQ(SW3_ACK_IRQ);

    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;


    PORTC->PCR[SW2_START_PIN] &= ~PORT_PCR_MUX_MASK;
    PORTC->PCR[SW2_START_PIN] |= PORT_PCR_MUX(1);
    GPIOC->PDDR &= ~(1 << SW2_START_PIN);
    PORTC->PCR[SW2_START_PIN] |= PORT_PCR_PE_MASK;
    PORTC->PCR[SW2_START_PIN] |= PORT_PCR_PS_MASK;
    PORTC->PCR[SW2_START_PIN] &= ~PORT_PCR_IRQC_MASK;
    PORTC->PCR[SW2_START_PIN] |= PORT_PCR_IRQC(0b1010);


    PORTA->PCR[SW3_ACK_PIN] &= ~PORT_PCR_MUX_MASK;
    PORTA->PCR[SW3_ACK_PIN] |= PORT_PCR_MUX(1);
    GPIOA->PDDR &= ~(1 << SW3_ACK_PIN);
    PORTA->PCR[SW3_ACK_PIN] |= PORT_PCR_PE_MASK;
    PORTA->PCR[SW3_ACK_PIN] |= PORT_PCR_PS_MASK;
    PORTA->PCR[SW3_ACK_PIN] &= ~PORT_PCR_IRQC_MASK;
    PORTA->PCR[SW3_ACK_PIN] |= PORT_PCR_IRQC(0b1010);


    PORTC->ISFR |= (1 << SW2_START_PIN);
    PORTA->ISFR |= (1 << SW3_ACK_PIN);

    NVIC_SetPriority(SW2_START_IRQ, 3);
    NVIC_ClearPendingIRQ(SW2_START_IRQ);
    NVIC_EnableIRQ(SW2_START_IRQ);

    NVIC_SetPriority(SW3_ACK_IRQ, 3);
    NVIC_ClearPendingIRQ(SW3_ACK_IRQ);
    NVIC_EnableIRQ(SW3_ACK_IRQ);
}


void PORTA_IRQHandler(void) {
    BaseType_t hpw = pdFALSE;
    TickType_t now = xTaskGetTickCountFromISR();

    if (PORTA->ISFR & (1 << SW3_ACK_PIN)) {
        PORTA->ISFR |= (1 << SW3_ACK_PIN);

        if ((now - g_lastSw3IsrTick) >= pdMS_TO_TICKS(DEBOUNCE_MS)) {
            g_lastSw3IsrTick = now;
            g_sw3AckPending = true;
            xSemaphoreGiveFromISR(buttonSema, &hpw);
        }
    }

    portYIELD_FROM_ISR(hpw);
}

void PORTC_PORTD_IRQHandler(void) {
    BaseType_t hpw = pdFALSE;
    TickType_t now = xTaskGetTickCountFromISR();

    if (PORTC->ISFR & (1 << SW2_START_PIN)) {
        PORTC->ISFR |= (1 << SW2_START_PIN);

        if ((now - g_lastSw2IsrTick) >= pdMS_TO_TICKS(DEBOUNCE_MS)) {
            g_lastSw2IsrTick = now;
            g_sw2StartPending = true;
            xSemaphoreGiveFromISR(buttonSema, &hpw);
        }
    }

    portYIELD_FROM_ISR(hpw);
}

/* =========================================================
   TASKS
   ========================================================= */

static void sensorTask(void *p) {
    (void)p;
    SensorPacket pkt;
    TickType_t lastWake = xTaskGetTickCount();

    memset(&pkt, 0, sizeof(pkt));

    while (1) {
        bool started = false;

        if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            started = g_systemStarted;
            xSemaphoreGive(statusMutex);
        }

        if (started) {
            pkt.lightRaw = readADCChannel(PHOTO_ADC_CH);
            readMicWindow(&pkt.micRaw, &pkt.micMin, &pkt.micMax, &pkt.micP2P);
            pkt.p2pFlag = (pkt.micP2P > MIC_P2P_THRESHOLD) ? 1 : 0;

            xQueueOverwrite(sensorQueue, &pkt);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(300));
    }
}

static void buttonTask(void *p) {
    (void)p;

    while (1) {
        if (xSemaphoreTake(buttonSema, portMAX_DELAY) == pdTRUE) {
            if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {

                if (g_sw2StartPending) {
                    g_sw2StartPending = false;

                    if (!g_systemStarted) {
                        g_systemStarted = true;
                        g_alertLatched = false;
                        PRINTF("SW2 pressed: system started\r\n");
                    } else {
                        g_systemStarted = false;
                        g_alertLatched = false;
                        PRINTF("SW2 pressed: system stopped\r\n");
                    }
                }

                if (g_sw3AckPending) {
                    g_sw3AckPending = false;

                    if (g_systemStarted && g_alertLatched) {
                        g_alertLatched = false;
                        PRINTF("SW3 pressed: alert acknowledged\r\n");
                    } else {
                        PRINTF("SW3 pressed: no active alert\r\n");
                    }
                }

                xSemaphoreGive(statusMutex);
            }
        }
    }
}

static void alertTask(void *p) {
    (void)p;
    SensorPacket pkt;

    while (1) {
        if (xQueueReceive(sensorQueue, &pkt, pdMS_TO_TICKS(200)) == pdTRUE) {
            if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                g_latestPacket = pkt;

                if (!g_systemStarted) {
                    g_alertLatched = false;
                    offAllLEDs();
                } else {
                    if (pkt.p2pFlag) {
                        g_alertLatched = true;
                    }

                    if (g_alertLatched) {
                        setRGB(true, false, false);
                    } else {
                        setRGB(false, true, false);
                    }
                }

                xSemaphoreGive(statusMutex);
            }
        } else {
            if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (!g_systemStarted) {
                    g_alertLatched = false;
                    offAllLEDs();
                } else if (g_alertLatched) {
                    setRGB(true, false, false);
                } else {
                    setRGB(false, true, false);
                }
                xSemaphoreGive(statusMutex);
            }
        }
    }
}

static void printTask(void *p) {
    (void)p;
    TickType_t lastWake = xTaskGetTickCount();
    SensorPacket pkt;
    bool started, alert;

    memset(&pkt, 0, sizeof(pkt));

    PRINTF("\r\n=== MCXC FINAL SENSOR TEST START ===\r\n");
    PRINTF("SW2 (PTC3): Start monitoring\r\n");
    PRINTF("SW3 (PTA4): Reset / acknowledge alert\r\n");
    PRINTF("Photoresistor: PTB0 / ADC0_SE8\r\n");
    PRINTF("Microphone A0: PTB1 / ADC0_SE9\r\n");
    PRINTF("P2P threshold: > %u\r\n\r\n", MIC_P2P_THRESHOLD);

    while (1) {
        if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            pkt = g_latestPacket;
            started = g_systemStarted;
            alert = g_alertLatched;
            xSemaphoreGive(statusMutex);
        } else {
            started = false;
            alert = false;
            memset(&pkt, 0, sizeof(pkt));
        }

        PRINTF("STARTED=%u | ALERT=%u | LIGHT_ADC=%u | MIC_RAW=%u | MIC_MIN=%u | MIC_MAX=%u | MIC_P2P=%u | P2P_FLAG=%u\r\n",
               started ? 1 : 0,
               alert ? 1 : 0,
               pkt.lightRaw,
               pkt.micRaw,
               pkt.micMin,
               pkt.micMax,
               pkt.micP2P,
               pkt.p2pFlag);

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
    }
}

/* =========================================================
   MAIN
   ========================================================= */

int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    BOARD_InitDebugConsole();
#endif

    initLEDs();
    offAllLEDs();
    initADCInputs();

    sensorQueue = xQueueCreate(1, sizeof(SensorPacket));
    buttonSema = xSemaphoreCreateBinary();
    statusMutex = xSemaphoreCreateMutex();

    if ((sensorQueue == NULL) || (buttonSema == NULL) || (statusMutex == NULL)) {
        PRINTF("RTOS object creation failed\r\n");
        while (1) {
        }
    }

    initButtonIRQs();

    xTaskCreate(sensorTask, "sensor_task",
                configMINIMAL_STACK_SIZE + 250, NULL, 2, NULL);

    xTaskCreate(buttonTask, "button_task",
                configMINIMAL_STACK_SIZE + 200, NULL, 3, NULL);

    xTaskCreate(alertTask, "alert_task",
                configMINIMAL_STACK_SIZE + 200, NULL, 2, NULL);

    xTaskCreate(printTask, "print_task",
                configMINIMAL_STACK_SIZE + 250, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) {
        __asm volatile ("nop");
    }

    return 0;
}
