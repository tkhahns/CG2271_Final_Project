/*
 * Copyright 2016-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    Final_Project_CG2271.c
 * @brief   Application entry point.
 */
#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
/* TODO: insert other include files here. */
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
/* TODO: insert other definitions and declarations here. */

// Data Structures
typedef enum{
	SENSOR_TYPE_DISTANCE,
	SENSOR_TYPE_LIGHT

} SensorType_t;

typedef struct {
    SensorType_t sensorType;
    float value; // Could be distance in cm, or light ADC value
} SensorData_t;

typedef struct {
    uint8_t warningLevel; // 0: Normal, 1: Warning, 2: Critical
} SystemStatus_t;

SystemStatus_t globalSystemStatus;

// Global handlers for RTOS
QueueHandle_t xSensorQueue;
SemaphoreHandle_t xSoundSemaphore;
SemaphoreHandle_t xSystemStatusMutex;

// Task prototypes
void vEnvTask(void *pvParameters);
void vDistanceTask(void *pvParameters);
void vAlertTask(void *pvParameters);
void vCommsTask(void *pvParameters);

/* --- Temporary Task Stubs --- */
// These prevent linker errors until you move them to their own files

void vAlertTask(void *pvParameters) {
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vCommsTask(void *pvParameters) {
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vEnvTask(void *pvParameters) {
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*
 * @brief   Application entry point.
 */
int main(void) {

    /* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    /* Initialise FSL debug console. */
    BOARD_InitDebugConsole();
#endif

    PRINTF("Starting Smart Desk Monitoring...");
    // Setting up the queus semarphores and mutexes:

    xSensorQueue = xQueueCreate(10, sizeof(SensorData_t));
    xSoundSemaphore = xSemaphoreCreateBinary(); // bin semaphore
    xSystemStatusMutex = xSemaphoreCreateMutex(); // mutex

    if (xSensorQueue != NULL && xSoundSemaphore != NULL && xSystemStatusMutex != NULL){
    	xTaskCreate(vAlertTask, "AlertTask", configMINIMAL_STACK_SIZE + 128, NULL, configMAX_PRIORITIES - 1, NULL);
    	xTaskCreate(vCommsTask, "CommsTask", configMINIMAL_STACK_SIZE + 128, NULL, configMAX_PRIORITIES - 2, NULL);
    	xTaskCreate(vEnvTask, "EnvTask", configMINIMAL_STACK_SIZE + 128, NULL, tskIDLE_PRIORITY + 1, NULL);
    	xTaskCreate(vDistanceTask, "DistanceTask", configMINIMAL_STACK_SIZE + 128, NULL, tskIDLE_PRIORITY + 1, NULL);
    	vTaskStartScheduler();
    } else {
    	PRINTF("Failed to create RTOS primitives.\r\n");
    }

    /* Force the counter to be placed into memory. */
    volatile static int i = 0 ;
    /* Enter an infinite loop, just incrementing a counter. */
    while(1) {
        i++ ;
        /* 'Dummy' NOP to allow source level single stepping of
            tight while() loop */
        __asm volatile ("nop");
    }
    return 0 ;
}


