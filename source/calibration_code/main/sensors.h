#pragma once
#include "config.h"

// Initialise all sensor peripherals (call once in setup())
void sensorsInit();

// Read all local sensors and write results into `state`
void sensorsRead(DeskState &state);

// Determine the raw system state from current sensor readings
uint8_t evaluateState(const DeskState &state);

// Debug helpers
void printDistance(float distance);
void printHumidity(float humidity);
void printTemperature(float temperature);
