#pragma once

#include "config.h"

/*
 * uart_receive.h — ESP32 UART link to MCXC
 *
 * Three functions, called from main.ino loop():
 *
 *  uartReceiveInit()      — call once in setup()
 *  uartReceiveLoop()      — drain RX buffer, parse $MCXC frames
 *  uartSendEspSensors()   — evaluate all thresholds, send $ESP,<count>
 */

void uartReceiveInit();
void uartReceiveLoop(DeskState &state);
void uartSendEspSensors(const DeskState &state);
