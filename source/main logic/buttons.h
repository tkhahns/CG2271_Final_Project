#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "MCXC444.h"


extern SemaphoreHandle_t g_buttonSema;


extern volatile bool g_sw2StartPending;
extern volatile bool g_sw3AckPending;

void Buttons_Init(SemaphoreHandle_t sema);

#endif
