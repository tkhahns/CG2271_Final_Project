#include "comms.h"
#include "uart_receive.h"

void commsInit() {
  uartReceiveInit();
}

void receiveFromMCXC(DeskState &state) {
  uartReceiveLoop(state);
}

void sendEspSensors(const DeskState &state) {
  uartSendEspSensors(state);
}
