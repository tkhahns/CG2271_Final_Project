/*
 * Copyright 2016-2025 NXP
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
#include "buttons.h"
#include "esp_uart.h"
#include "led.h"
#include "adc.h"
#include "slcd.h"
#include "tasks.h"

/* ------------------------------------------------------------------ */
/* Shared FreeRTOS objects                                             */
/* ------------------------------------------------------------------ */
QueueHandle_t     g_sensorQueue  = NULL;
SemaphoreHandle_t g_buttonSema   = NULL;
SemaphoreHandle_t g_statusMutex  = NULL;

/* ------------------------------------------------------------------ */
/* Shared application state                                            */
/* ------------------------------------------------------------------ */
bool         g_systemStarted = false;
bool         g_alertLatched  = false;
SensorPacket g_latestPacket  = { 0 };

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    BOARD_InitDebugConsole();
#endif

    LED_Init();
    LED_OffAll();
    ADC_Init();
    SLCD_Init();      /* initialise the on-board 4-digit LCD */
    ESP_UART_Init();

    g_sensorQueue = xQueueCreate(1, sizeof(SensorPacket));
    g_buttonSema  = xSemaphoreCreateBinary();
    g_statusMutex = xSemaphoreCreateMutex();

    if ((g_sensorQueue == NULL) || (g_buttonSema == NULL) || (g_statusMutex == NULL)) {
        PRINTF("ERROR: RTOS object creation failed\r\n");
        while (1) { __asm volatile ("nop"); }
    }

    Buttons_Init(g_buttonSema);

    xTaskCreate(sensorTask, "sensor", configMINIMAL_STACK_SIZE + 250, NULL, 2, NULL);
    xTaskCreate(buttonTask, "button", configMINIMAL_STACK_SIZE + 200, NULL, 3, NULL);
    xTaskCreate(alertTask,  "alert",  configMINIMAL_STACK_SIZE + 200, NULL, 2, NULL);
    xTaskCreate(printTask,  "print",  configMINIMAL_STACK_SIZE + 350, NULL, 1, NULL);

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
