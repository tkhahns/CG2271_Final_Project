#include "app_tasks.h"
#include "app_types.h"
#include "adc.h"
#include "led.h"
#include "buttons.h"
<<<<<<< HEAD
=======
#include "esp_uart.h"
#include "slcd.h"
>>>>>>> testing
#include "ssd1306.h"
#include "fsl_debug_console.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include <string.h>
#include <stdbool.h>

<<<<<<< HEAD
/* ================================================================== */
/* sensorTask — reads ADC every 300 ms, pushes to queue                */
/* ================================================================== */
=======
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

>>>>>>> testing
void sensorTask(void *p) {
    (void)p;
    SensorPacket pkt;
    TickType_t lastWake = xTaskGetTickCount();
    bool sensorsInitialized = false;
    bool remoteValid = false;
    uint8_t remoteActiveCount = 0U;
    float remoteTemperatureC = 0.0f;
    bool remoteTemperatureValid = false;
    float remoteDistanceCm = 0.0f;
    bool remoteDistanceValid = false;
    memset(&pkt, 0, sizeof(pkt));

    while (1) {
<<<<<<< HEAD
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
/* buttonTask — handles SW2 (start/stop) and SW3 (ack alert)          */
/* ================================================================== */
=======
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

        ESP_UART_GetRemoteData(&remoteValid,
                               &remoteActiveCount,
                               &remoteTemperatureC,
                               &remoteTemperatureValid,
                               &remoteDistanceCm,
                               &remoteDistanceValid);

        pkt.remoteValid = remoteValid ? 1U : 0U;
        pkt.remoteActiveCount = remoteActiveCount;
        pkt.activeCount = pkt.remoteValid ? pkt.remoteActiveCount : 0U;
        pkt.soundFlag = 0U;
        pkt.lightFlag = 0U;
        pkt.tempFlag = remoteTemperatureValid ? 1U : 0U;
        pkt.distanceFlag = remoteDistanceValid ? 1U : 0U;
        pkt.temperatureC = remoteTemperatureC;
        pkt.distanceCm = remoteDistanceCm;

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

>>>>>>> testing
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
                        g_alertSuppressed = false;
                        g_warningState = WARNING_STATE_IDLE;
                        g_oledScreenMode = OLED_SCREEN_SENSORS;
                        memset(&g_latestPacket, 0, sizeof(g_latestPacket));
                        PRINTF("SW2: system STARTED\r\n");
                    } else {
                        g_systemStarted = false;
                        g_alertSuppressed = false;
                        g_warningState = WARNING_STATE_IDLE;
                        g_oledScreenMode = OLED_SCREEN_SENSORS;
                        memset(&g_latestPacket, 0, sizeof(g_latestPacket));
                        PRINTF("SW2: system STOPPED\r\n");
                    }
                }

                if (g_sw3AckPending) {
                    g_sw3AckPending = false;
                    if (g_oledScreenMode == OLED_SCREEN_SENSORS) {
                        g_oledScreenMode = OLED_SCREEN_SUGGESTION;
                        PRINTF("SW3: OLED -> suggestion view\r\n");
                    } else {
                        g_oledScreenMode = OLED_SCREEN_SENSORS;
                        PRINTF("SW3: OLED -> sensor view\r\n");
                    }
                }

                xSemaphoreGive(g_statusMutex);
            }
        }
    }
}

<<<<<<< HEAD
/* ================================================================== */
/* alertTask — drives RGB LED and SSD1306 display                      */
/*                                                                      */
/* The display update is rate-limited to once per 500 ms to avoid      */
/* hammering the I2C bus on every queue receive timeout.               */
/* ================================================================== */
void alertTask(void *p) {
    (void)p;
    SensorPacket pkt;
    TickType_t lastDisplayUpdate = 0;

    while (1) {
        bool gotPacket = (xQueueReceive(g_sensorQueue, &pkt,
                                        pdMS_TO_TICKS(200)) == pdTRUE);
=======
void alertTask(void *p) {
    (void)p;
    SensorPacket pkt;

    while (1) {
        bool gotPacket = (xQueueReceive(g_sensorQueue, &pkt, pdMS_TO_TICKS(200)) == pdTRUE);
        bool systemStarted = false;
        bool alertSuppressed = false;
        bool oledStarted = false;
        bool oledAlert = false;
        uint16_t oledLight = 0U;
        uint16_t oledMic = 0U;
        uint8_t oledCount = 0U;
        bool oledSuggestionScreen = false;
        float oledTemperatureC = 0.0f;
        bool oledTemperatureValid = false;
        float oledDistanceCm = 0.0f;
        bool oledDistanceValid = false;
        bool oledRefresh = false;
>>>>>>> testing

        if (xSemaphoreTake(g_statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (gotPacket) {
                g_latestPacket = pkt;
<<<<<<< HEAD
                if (pkt.p2pFlag) {
                    g_alertLatched = true;
                }
=======
            } else {
                pkt = g_latestPacket;
>>>>>>> testing
            }

            /* Update RGB LED immediately */
            if (!g_systemStarted) {
                g_alertSuppressed = false;
                g_warningState = WARNING_STATE_IDLE;
                memset(&g_latestPacket, 0, sizeof(g_latestPacket));
                memset(&pkt, 0, sizeof(pkt));
                LED_OffAll();
<<<<<<< HEAD
            } else if (g_alertLatched) {
                LED_SetRGB(true, false, false);   /* RED   */
            } else {
                LED_SetRGB(false, true, false);   /* GREEN */
            }

            /* Update SSD1306 at most every 500 ms — I2C is slow (1024
             * bytes at 100 kHz takes ~90 ms) so don't call on every loop */
            TickType_t now = xTaskGetTickCount();
            if ((now - lastDisplayUpdate) >= pdMS_TO_TICKS(500)) {
                lastDisplayUpdate = now;
                SSD1306_ShowAll(g_systemStarted,
                                g_alertLatched,
                                g_latestPacket.lightRaw,
                                g_latestPacket.micP2P);
=======
                SLCD_Clear();
            } else if (pkt.activeCount == 0U) {
                g_alertSuppressed = false;
                g_warningState = WARNING_STATE_IDLE;
                LED_OffAll();
                SLCD_ShowNumber(0U);
            } else {
                g_warningState = warningStateFromCount(pkt.activeCount);

                if (g_warningState == WARNING_STATE_YELLOW) {
                    LED_SetRGB(true, true, false);
                } else {
                    LED_SetRGB(true, false, false);
                }

                SLCD_ShowNumber(pkt.activeCount);
>>>>>>> testing
            }

            systemStarted = g_systemStarted;
            g_alertSuppressed = false;
            alertSuppressed = false;

            oledStarted = systemStarted;
            oledAlert = (g_warningState >= WARNING_STATE_YELLOW);
            oledLight = pkt.lightRaw;
            oledMic = pkt.micP2P;
            oledCount = pkt.activeCount;
            oledSuggestionScreen = (g_oledScreenMode == OLED_SCREEN_SUGGESTION);
            oledTemperatureC = pkt.temperatureC;
            oledTemperatureValid = (pkt.tempFlag != 0U);
            oledDistanceCm = pkt.distanceCm;
            oledDistanceValid = (pkt.distanceFlag != 0U);
            oledRefresh = true;
            xSemaphoreGive(g_statusMutex);
        }

        if (oledRefresh) {
            SSD1306_ShowAll(oledStarted,
                            oledAlert,
                            oledLight,
                            oledMic,
                            oledCount,
                            oledSuggestionScreen,
                            oledTemperatureC,
                            oledTemperatureValid,
                            oledDistanceCm,
                            oledDistanceValid);
        }

        if (gotPacket || !systemStarted) {
            ESP_UART_SendTelemetry(&pkt, systemStarted, alertSuppressed);
        }
    }
}

<<<<<<< HEAD
/* ================================================================== */
/* printTask — UART log every 1 s                                       */
/* ================================================================== */
=======
>>>>>>> testing
void printTask(void *p) {
    (void)p;
    TickType_t lastWake = xTaskGetTickCount();
    SensorPacket pkt;
    bool started;
    bool suppressed;
    OledScreenMode oledMode;
    memset(&pkt, 0, sizeof(pkt));

    PRINTF("\r\n=== MCXC444 SENSOR MONITOR ===\r\n");
    PRINTF("SW2 (PTC3) : Start / Stop\r\n");
<<<<<<< HEAD
    PRINTF("SW3 (PTA4) : Acknowledge alert\r\n");
    PRINTF("Light      : PTB0 (ADC0_SE8)\r\n");
    PRINTF("Mic        : PTB1 (ADC0_SE9)\r\n");
    PRINTF("SSD1306    : I2C1  PTC1=SCL  PTC2=SDA  addr=0x3C\r\n");
    PRINTF("P2P thresh : >%u\r\n\r\n", MIC_P2P_THRESHOLD);
=======
    PRINTF("SW3 (PTA4) : Toggle OLED screen\r\n");
    PRINTF("Light      : PTB0 (ADC0_SE8) dark <= %u, bright >= %u\r\n", LIGHT_DARK_THRESHOLD, LIGHT_BRIGHT_THRESHOLD);
    PRINTF("Mic        : PTB1 (ADC0_SE9) threshold >= %u\r\n", MIC_P2P_THRESHOLD);
    PRINTF("ESP link   : UART2 on PTE22/PTE23 @ 9600\r\n");
    PRINTF("MCXC TX    : $MCXC,<light>,<sound>,<started>,<suppressed>\\r\\n\r\n");
    PRINTF("ESP32 TX   : $ESP,<cnt>,<temp_x10>,<dist_x10>\\r\\n\r\n");
>>>>>>> testing

    while (1) {
        if (xSemaphoreTake(g_statusMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            pkt = g_latestPacket;
            started = g_systemStarted;
            suppressed = g_alertSuppressed;
            oledMode = g_oledScreenMode;
            xSemaphoreGive(g_statusMutex);
        } else {
            started = false;
            suppressed = false;
            oledMode = OLED_SCREEN_SENSORS;
            memset(&pkt, 0, sizeof(pkt));
        }

<<<<<<< HEAD
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
=======
        if (!started) {
            PRINTF("STARTED=0 | SYSTEM IDLE\r\n");
        } else {
            PRINTF("STARTED=%u | SUPP=%u | CNT=%u | LIGHT=%u | SOUND=%u | TEMP=%.1f | DIST=%.1f | VIEW=%s\r\n",
                   started ? 1U : 0U,
                   suppressed ? 1U : 0U,
                   pkt.activeCount,
                   pkt.lightRaw,
                   pkt.micP2P,
                   pkt.temperatureC,
                   pkt.distanceCm,
                   (oledMode == OLED_SCREEN_SUGGESTION) ? "AI" : "SNS");
        }
>>>>>>> testing

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
    }
}