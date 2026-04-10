#pragma once
#include "config.h"

void sensorsInit();
void sensorsRead(DeskState &state);
uint8_t breachedCount(const DeskState &state);
uint8_t evaluateState(const DeskState &state);
void printDistance(float distance);
void printHumidity(float humidity);
void printTemperature(float temperature);
