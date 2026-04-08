#pragma once
#include "config.h"

void commsInit();
void receiveFromMCXC(DeskState &state);
void sendEspSensors(const DeskState &state);
