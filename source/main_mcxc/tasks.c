#include "tasks.h"
#include "app_types.h"
#include "adc.h"
#include "led.h"
#include "buttons.h"
#include "esp_uart.h"
#include "slcd.h"
#include "fsl_debug_console.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include <string.h>
#include <stdbool.h>

/* ================================================================== */
/* sensorTask — reads ADC, sends telemetry, pushes SensorPacket        */
/* ================================================================== */
void sensorTask(void *p) {
    (void)p;
    SensorPacket pkt;
    TickType_t lastWake = xTaskGetTickCount();
    memset(&pkt, 0, sizeof(pkt));

    while (1) {
        bool systemStarted;
        bool alertLatched;

        pkt.lightRaw = ADC_ReadChannel(PHOTO_ADC_CH);
        ADC_ReadMicWindow(&pkt.micRaw, &pkt.micMin, &pkt.micMax, &pkt.micP2P);
        pkt.p2pFlag  = (pkt.micP2P > MIC_P2P_THRESHOLD) ? 1U : 0U;
        pkt.darkFlag = (pkt.lightRaw < LIGHT_DARK_THRESHOLD) ? 1U : 0U;

        if (xSemaphoreTake(g_statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            systemStarted = g_systemStarted;
            alertLatched  = g_alertLatched;
            xSemaphoreGive(g_statusMutex);
        } else {
            systemStarted = false;
            alertLatched  = false;
        }

        ESP_UART_SendTelemetry(pkt.lightRaw,
                               pkt.micP2P,
                               systemStarted,
                               alertLatched);

        xQueueOverwrite(g_sensorQueue, &pkt);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(300));
    }
}

/* ================================================================== */
/* buttonTask — waits on semaphore, handles SW2/SW3 logic              */
/* ================================================================== */
void buttonTask(void *p) {
    (void)p;

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

/* ==================================================================  */
/* alertTask — drives RGB LED and SLCD based on system state           */
/* ==================================================================  */
void alertTask(void *p) {
    (void)p;
    SensorPacket pkt;
    bool showingAlert = false;
    TickType_t lastAlternate = 0;

    while (1) {
        bool gotPacket = (xQueueReceive(g_sensorQueue, &pkt, pdMS_TO_TICKS(200)) == pdTRUE);

        if (xSemaphoreTake(g_statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {

            if (gotPacket) {
                g_latestPacket = pkt;
            }

            if (!g_systemStarted) {
                g_alertLatched = false;
                LED_OffAll();
                SLCD_Clear();
            } else {
                bool badEnvironment = (pkt.p2pFlag || pkt.darkFlag || pkt.tempFlag);
                bool personTooClose = pkt.closeFlag;

                if (gotPacket && badEnvironment && personTooClose) {
                    g_alertLatched = true;
                }

                if (g_alertLatched) {
                    LED_SetRGB(true, false, false);

                    TickType_t now = xTaskGetTickCount();
                    if ((now - lastAlternate) >= pdMS_TO_TICKS(1000)) {
                        lastAlternate  = now;
                        showingAlert   = !showingAlert;
                    }

                    if (showingAlert) {
                        SLCD_ShowString("AL  ");
                    } else {
                        if (g_latestPacket.p2pFlag) {
                            SLCD_ShowNumber(g_latestPacket.micP2P);
                        } else if (g_latestPacket.tempFlag) {
                            SLCD_ShowNumber((uint16_t)g_latestPacket.temperature);
                        } else {
                            SLCD_ShowNumber(g_latestPacket.lightRaw);
                        }
                    }
                } else {
                    LED_SetRGB(false, true, false);
                    showingAlert  = false;
                    SLCD_ShowNumber(g_latestPacket.lightRaw);
                }
            }

            xSemaphoreGive(g_statusMutex);
        }
    }
}

/* ================================================================== */
/* printTask — UART log every 1 s                                     */
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
    PRINTF("ESP link   : UART2 on PTE22/PTE23 @ 115200\r\n");
    PRINTF("Frame      : $MCXC,<light>,<mic_p2p>,<started>,<alert>\\r\\n\r\n");

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

        PRINTF("STARTED=%u | ALERT=%u | LIGHT=%u | MIC_P2P=%u\r\n",
               started ? 1U : 0U,
               alert   ? 1U : 0U,
               pkt.lightRaw,
               pkt.micP2P);

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
    }
}
