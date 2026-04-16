// #include "uart_parser.h"
// #include <string.h>


// char g_suggestionBuf[SUGGESTION_MAX_LEN] = {0};
// bool g_suggestionReady = false;

// void MCXC_ParseIncomingUART(char incomingChar) {
//     static char rxBuffer[100];
//     static uint8_t rxIndex = 0;

//     if (incomingChar == '\r') {
//         return;
//     }

//     // End of frame detected
//     if (incomingChar == '\n') {
//         rxBuffer[rxIndex] = '\0'; // Null terminate the string

//         if (strncmp(rxBuffer, "$SUG,", 5) == 0) {
//             // Copy the payload into our global display buffer
//             strncpy(g_suggestionBuf, rxBuffer + 5, SUGGESTION_MAX_LEN - 1);
//             g_suggestionBuf[SUGGESTION_MAX_LEN - 1] = '\0'; // Ensure termination

//             // Flag that a new message is ready to be displayed
//             g_suggestionReady = true;
//         }

//         rxIndex = 0;
//         return;
//     }

//     // Buffer the incoming character, preventing overflow
//     if (rxIndex < (sizeof(rxBuffer) - 1)) {
//         rxBuffer[rxIndex++] = incomingChar;
//     } else {
//         rxIndex = 0; 
//     }
// }
