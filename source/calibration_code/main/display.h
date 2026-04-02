#pragma once
#include "config.h"

// Initialise the I2C LCD (call once in setup())
void displayInit();

// Show the normal operating screen
void displayUpdate(const DeskState &state, uint8_t finalState);

// Show the system-inactive screen
void displayInactive();
