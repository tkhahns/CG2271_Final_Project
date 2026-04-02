#pragma once
#include "config.h"

// Initialise UART1 for MCXC communication (call once in setup())
void commsInit();

// Read and process any waiting packets from MCXC444
void receiveFromMCXC(DeskState &state);

// Send an RGB state command to MCXC444
void sendRGBCommand(uint8_t state);
