/*
 * main.c -- MCXC444 sensor monitor with SSD1306 OLED display
 */

#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "app_types.h"
#include "led.h"
#include "adc.h"
#include "ssd1306.h"
#include "app_tasks.h"

QueueHandle_t g_sensorQueue = NULL;
SemaphoreHandle_t g_buttonSema = NULL;
SemaphoreHandle_t g_statusMutex = NULL;

bool g_systemStarted = false;
bool g_alertSuppressed = false;
WarningState g_warningState = WARNING_STATE_IDLE;
OledScreenMode g_oledScreenMode = OLED_SCREEN_SENSORS;
SensorPacket g_latestPacket = {0};
char g_suggestionBuf[SUGGESTION_MAX_LEN] = {0};
bool g_suggestionReady = false;

int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    BOARD_InitDebugConsole();
#endif

    LED_Init();
    LED_OffAll();
    SSD1306_Init();
    SSD1306_ShowAll(false, false, 0U, 0U, 0U, false,
                    0.0f, false, 0.0f, false,
                    false, NULL);

    g_sensorQueue = xQueueCreate(1, sizeof(SensorPacket));
    g_buttonSema = xSemaphoreCreateBinary();
    g_statusMutex = xSemaphoreCreateMutex();

    if ((g_sensorQueue == NULL) || (g_buttonSema == NULL) ||
        (g_statusMutex == NULL)) {
        PRINTF("ERROR: RTOS object creation failed\r\n");
        while (1) { __asm volatile ("nop"); }
    }

    xTaskCreate(sensorTask, "sensor", configMINIMAL_STACK_SIZE + 250, NULL, 2, NULL);
    xTaskCreate(remoteTask, "remote", configMINIMAL_STACK_SIZE + 180, NULL, 3, NULL);
    xTaskCreate(buttonTask, "button", configMINIMAL_STACK_SIZE + 200, NULL, 3, NULL);
    xTaskCreate(alertTask, "alert", configMINIMAL_STACK_SIZE + 400, NULL, 2, NULL);
    xTaskCreate(printTask, "print", configMINIMAL_STACK_SIZE + 350, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) { __asm volatile ("nop"); }
    return 0;
}

void vApplicationMallocFailedHook(void) {
    taskDISABLE_INTERRUPTS();
    LED_SetRGB(true, false, false);
    while (1) { __asm volatile ("nop"); }
}

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName) {
    (void)pxTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    LED_SetRGB(true, false, false);
    while (1) { __asm volatile ("nop"); }
}
