
#ifndef DISTANCE_TASK_H
#define DISTANCE_TASK_H

#include "FreeRTOS.h"
#include "task.h"

/* --- Hardware Definitions for the ultrasonic sensor --- */

#define HCSR04_TRIG_PORT    PORTD
#define HCSR04_TRIG_GPIO    GPIOD
#define HCSR04_TRIG_PIN     2U

#define HCSR04_ECHO_PORT    PORTD
#define HCSR04_ECHO_GPIO    GPIOD
#define HCSR04_ECHO_PIN     4U

/* --- Task Prototype --- */
void vDistanceTask(void *pvParameters);
#endif
