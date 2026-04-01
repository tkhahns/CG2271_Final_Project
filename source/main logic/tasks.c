#include "tasks.h"
#include "app_types.h"
#include "adc.h"
#include "led.h"
#include "buttons.h"
#include "slcd.h"
#include "fsl_debug_console.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include <string.h>
#include <stdbool.h>

/* ================================================================== */
/* sensorTask — reads ADC, pushes SensorPacket onto queue (300 ms)     */
/* ================================================================== */
void sensorTask(void *p) {
    (void)p;
    SensorPacket pkt;
    TickType_t lastWake = xTaskGetTickCount();
    memset(&pkt, 0, sizeof(pkt));

    while (1) {
        bool started = false;

        if (xSemaphoreTake(g_statusMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            started = g_systemStarted;
            xSemaphoreGive(g_statusMutex);
        }

        if (started) {
            pkt.lightRaw = ADC_ReadChannel(PHOTO_ADC_CH);
            ADC_ReadMicWindow(&pkt.micRaw, &pkt.micMin, &pkt.micMax, &pkt.micP2P);
            pkt.p2pFlag  = (pkt.micP2P > MIC_P2P_THRESHOLD) ? 1U : 0U;
            xQueueOverwrite(g_sensorQueue, &pkt);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(300));
    }
}

/* ================================================================== */
/* buttonTask — waits on semaphore, handles SW2/SW3 logic              */
/* ================================================================== */
void buttonTask(void *p) {
    (void)p;
    Buttons_Init(g_buttonSema);

    while (1) {
        if (xSemaphoreTake(g_buttonSema, portMAX_DELAY) == pdTRUE) {
            if (xSemaphoreTake(g_statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {

                if (g_sw2StartPending) {
                    g_sw2StartPending = false;
                    if (!g_systemStarted) {
                        g_systemStarted = true;
                        g_alertLatched  = false;
                        PRINTF("SW2: system STARTED\r\n");
                    } else {
                        g_systemStarted = false;
                        g_alertLatched  = false;
                        PRINTF("SW2: system STOPPED\r\n");
                    }
                }

                if (g_sw3AckPending) {
                    g_sw3AckPending = false;
                    if (g_systemStarted && g_alertLatched) {
                        g_alertLatched = false;
                        PRINTF("SW3: alert ACKNOWLEDGED\r\n");
                    } else {
                        PRINTF("SW3: no active alert\r\n");
                    }
                }

                xSemaphoreGive(g_statusMutex);
            }
        }
    }
}

/* ================================================================== */
/* alertTask — drives RGB LED and SLCD based on system state           */
/*                                                                      */
/* SLCD display layout (4 digits):                                      */
/*   System OFF  : "    " (blank)                                       */
/*   Running, OK : light ADC value  e.g. "2048"                        */
/*   Alert       : mic P2P value    e.g. "  42" (leading zeros hidden) */
/*                 alternating every 1 second with "AL  "              */
/* ================================================================== */
void alertTask(void *p) {
    (void)p;
    SensorPacket pkt;
    bool showingAlert = false;       /* used to alternate SLCD content on alert */
    TickType_t lastAlternate = 0;

    while (1) {
        bool gotPacket = (xQueueReceive(g_sensorQueue, &pkt, pdMS_TO_TICKS(200)) == pdTRUE);

        if (xSemaphoreTake(g_statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {

            if (gotPacket) {
                g_latestPacket = pkt;
            }

            if (!g_systemStarted) {
                /* System off: everything blank */
                g_alertLatched = false;
                LED_OffAll();
                SLCD_Clear();
            } else {
                if (gotPacket && pkt.p2pFlag) {
                    g_alertLatched = true;
                }

                if (g_alertLatched) {
                    /* Red LED */
                    LED_SetRGB(true, false, false);

                    /* Alternate SLCD between "AL  " and mic P2P value every 1 s */
                    TickType_t now = xTaskGetTickCount();
                    if ((now - lastAlternate) >= pdMS_TO_TICKS(1000)) {
                        lastAlternate  = now;
                        showingAlert   = !showingAlert;
                    }

                    if (showingAlert) {
                        SLCD_ShowString("AL  ");
                    } else {
                        SLCD_ShowNumber(g_latestPacket.micP2P);
                    }
                } else {
                    /* Green LED, show light ADC value */
                    LED_SetRGB(false, true, false);
                    showingAlert  = false;
                    lastAlternate = xTaskGetTickCount();
                    SLCD_ShowNumber(g_latestPacket.lightRaw);
                }
            }

            xSemaphoreGive(g_statusMutex);
        }
    }
}

/* ================================================================== */
/* printTask — UART log every 1 s                                       */
/* ================================================================== */
void printTask(void *p) {
    (void)p;
    TickType_t lastWake = xTaskGetTickCount();
    SensorPacket pkt;
    bool started, alert;
    memset(&pkt, 0, sizeof(pkt));

    PRINTF("\r\n=== MCXC444 SENSOR MONITOR ===\r\n");
    PRINTF("SW2 (PTC3) : Start / Stop\r\n");
    PRINTF("SW3 (PTA4) : Acknowledge alert\r\n");
    PRINTF("Light      : PTB0 (ADC0_SE8)\r\n");
    PRINTF("Mic        : PTB1 (ADC0_SE9)\r\n");
    PRINTF("SLCD       : on-board DS1 (4-digit 7-seg)\r\n");
    PRINTF("P2P thresh : >%u\r\n\r\n", MIC_P2P_THRESHOLD);

    while (1) {
        if (xSemaphoreTake(g_statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            pkt     = g_latestPacket;
            started = g_systemStarted;
            alert   = g_alertLatched;
            xSemaphoreGive(g_statusMutex);
        } else {
            started = false;
            alert   = false;
            memset(&pkt, 0, sizeof(pkt));
        }

        PRINTF("STARTED=%u | ALERT=%u | LIGHT=%u | MIC_P2P=%u"
               " | MIC_MIN=%u | MIC_MAX=%u | RAW=%u | FLAG=%u\r\n",
               started ? 1U : 0U,
               alert   ? 1U : 0U,
               pkt.lightRaw,
               pkt.micP2P,
               pkt.micMin,
               pkt.micMax,
               pkt.micRaw,
               pkt.p2pFlag);

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
    }
}
