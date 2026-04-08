#include "app_tasks.h"
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

static WarningState warningStateFromCount(uint8_t activeCount) {
    if (activeCount == 0U) {
        return WARNING_STATE_IDLE;
    }
    if (activeCount == 1U) {
        return WARNING_STATE_YELLOW;
    }
    if (activeCount == 2U) {
        return WARNING_STATE_RED;
    }
    return WARNING_STATE_RED_BUZZER;
}

void sensorTask(void *p) {
    (void)p;
    SensorPacket pkt;
    TickType_t lastWake = xTaskGetTickCount();
    bool sensorsInitialized = false;
    bool remoteValid = false;
    uint8_t remoteActiveCount = 0U;
    memset(&pkt, 0, sizeof(pkt));

    while (1) {
        bool systemStarted = false;

        if (xSemaphoreTake(g_statusMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            systemStarted = g_systemStarted;
            xSemaphoreGive(g_statusMutex);
        }

        if (!systemStarted) {
            memset(&pkt, 0, sizeof(pkt));
            xQueueOverwrite(g_sensorQueue, &pkt);
            vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(500));
            continue;
        }

        if (!sensorsInitialized) {
            ADC_Init();
            ESP_UART_Init();
            sensorsInitialized = true;
        }

        pkt.lightRaw = ADC_ReadChannel(PHOTO_ADC_CH);
        ADC_ReadMicWindow(&pkt.micRaw, &pkt.micMin, &pkt.micMax, &pkt.micP2P);

        ESP_UART_GetRemoteCount(&remoteValid, &remoteActiveCount);
        pkt.remoteValid = remoteValid ? 1U : 0U;
        pkt.remoteActiveCount = remoteActiveCount;
        pkt.activeCount = pkt.remoteValid ? pkt.remoteActiveCount : 0U;
        pkt.soundFlag = 0U;
        pkt.lightFlag = 0U;
        pkt.tempFlag = 0U;
        pkt.distanceFlag = 0U;
        pkt.temperatureC = 0.0f;
        pkt.distanceCm = 0.0f;

        xQueueOverwrite(g_sensorQueue, &pkt);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(500));
    }
}

void remoteTask(void *p) {
    (void)p;

    while (1) {
        bool systemStarted = false;

        if (xSemaphoreTake(g_statusMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            systemStarted = g_systemStarted;
            xSemaphoreGive(g_statusMutex);
        }

        if (systemStarted) {
            ESP_UART_Init();
            ESP_UART_ServiceRx();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void buttonTask(void *p) {
    (void)p;

    while (1) {
        if (xSemaphoreTake(g_buttonSema, portMAX_DELAY) == pdTRUE) {
            if (xSemaphoreTake(g_statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (g_sw2StartPending) {
                    g_sw2StartPending = false;
                    if (!g_systemStarted) {
                        g_systemStarted = true;
                        g_alertSuppressed = false;
                        g_warningState = WARNING_STATE_IDLE;
                        memset(&g_latestPacket, 0, sizeof(g_latestPacket));
                        PRINTF("SW2: system STARTED\r\n");
                    } else {
                        g_systemStarted = false;
                        g_alertSuppressed = false;
                        g_warningState = WARNING_STATE_IDLE;
                        memset(&g_latestPacket, 0, sizeof(g_latestPacket));
                        PRINTF("SW2: system STOPPED\r\n");
                    }
                }

                if (g_sw3AckPending) {
                    g_sw3AckPending = false;
                    if (g_systemStarted && (g_warningState >= WARNING_STATE_YELLOW)) {
                        g_alertSuppressed = true;
                        g_warningState = WARNING_STATE_ACKNOWLEDGED;
                        PRINTF("SW3: warning outputs SUPPRESSED\r\n");
                    } else {
                        PRINTF("SW3: no active warning\r\n");
                    }
                }

                xSemaphoreGive(g_statusMutex);
            }
        }
    }
}

void alertTask(void *p) {
    (void)p;
    SensorPacket pkt;

    while (1) {
        bool gotPacket = (xQueueReceive(g_sensorQueue, &pkt, pdMS_TO_TICKS(200)) == pdTRUE);
        bool systemStarted = false;
        bool alertSuppressed = false;

        if (xSemaphoreTake(g_statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (gotPacket) {
                g_latestPacket = pkt;
            } else {
                pkt = g_latestPacket;
            }

            if (!g_systemStarted) {
                g_alertSuppressed = false;
                g_warningState = WARNING_STATE_IDLE;
                memset(&g_latestPacket, 0, sizeof(g_latestPacket));
                memset(&pkt, 0, sizeof(pkt));
                LED_OffAll();
                SLCD_Clear();
            } else if (pkt.activeCount == 0U) {
                g_alertSuppressed = false;
                g_warningState = WARNING_STATE_IDLE;
                LED_OffAll();
                SLCD_ShowNumber(0U);
            } else if (g_alertSuppressed) {
                g_warningState = WARNING_STATE_ACKNOWLEDGED;
                LED_OffAll();
                SLCD_ShowString("ACK ");
            } else {
                g_warningState = warningStateFromCount(pkt.activeCount);

                if (g_warningState == WARNING_STATE_YELLOW) {
                    LED_SetRGB(true, true, false);
                } else {
                    LED_SetRGB(true, false, false);
                }

                SLCD_ShowNumber(pkt.activeCount);
            }

            systemStarted = g_systemStarted;
            alertSuppressed = g_alertSuppressed;
            xSemaphoreGive(g_statusMutex);
        }

        if (gotPacket || !systemStarted) {
            ESP_UART_SendTelemetry(&pkt, systemStarted, alertSuppressed);
        }
    }
}

void printTask(void *p) {
    (void)p;
    TickType_t lastWake = xTaskGetTickCount();
    SensorPacket pkt;
    bool started;
    bool suppressed;
    memset(&pkt, 0, sizeof(pkt));

    PRINTF("\r\n=== MCXC444 SENSOR MONITOR ===\r\n");
    PRINTF("SW2 (PTC3) : Start / Stop\r\n");
    PRINTF("SW3 (PTA4) : Suppress LED / buzzer outputs\r\n");
    PRINTF("Light      : PTB0 (ADC0_SE8) dark <= %u, bright >= %u\r\n", LIGHT_DARK_THRESHOLD, LIGHT_BRIGHT_THRESHOLD);
    PRINTF("Mic        : PTB1 (ADC0_SE9) threshold >= %u\r\n", MIC_P2P_THRESHOLD);
    PRINTF("ESP link   : UART2 on PTE22/PTE23 @ 9600\r\n");
    PRINTF("MCXC TX    : $MCXC,<light>,<sound>,<started>,<suppressed>\\r\\n\r\n");
    PRINTF("ESP32 TX   : $ESP,<cnt>\\r\\n\r\n");

    while (1) {
        if (xSemaphoreTake(g_statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            pkt = g_latestPacket;
            started = g_systemStarted;
            suppressed = g_alertSuppressed;
            xSemaphoreGive(g_statusMutex);
        } else {
            started = false;
            suppressed = false;
            memset(&pkt, 0, sizeof(pkt));
        }

        if (!started) {
            PRINTF("STARTED=0 | SYSTEM IDLE\r\n");
        } else {
            PRINTF("STARTED=%u | SUPP=%u | CNT=%u | LIGHT=%u | SOUND=%u\r\n",
                   started ? 1U : 0U,
                   suppressed ? 1U : 0U,
                   pkt.activeCount,
                   pkt.lightRaw,
                   pkt.micP2P);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
    }
}

