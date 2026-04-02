#ifndef OPENAI_CLIENT_H
#define OPENAI_CLIENT_H

#include "uart_handler.h"

void   initOpenAI();
String getAdvisory(const SensorData_t& data);

#endif
