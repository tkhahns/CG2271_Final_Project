#include "esp_uart.h"

#include <stdio.h>

#include "MCXC444.h"
#include "fsl_clock.h"

#define ESP_UART_BAUD_RATE 115200
#define ESP_UART_TX_PIN    22  /* PTE22 -> UART2_TX */
#define ESP_UART_RX_PIN    23  /* PTE23 -> UART2_RX */
#define ESP_UART_PIN_MUX   4   /* ALT4 routes PTE22/PTE23 to UART2 */

static void ESP_UART_WriteByte(uint8_t byte)
{
    while ((UART2->S1 & UART_S1_TDRE_MASK) == 0U)
    {
    }

    UART2->D = byte;
}

static void ESP_UART_WriteString(const char *str)
{
    while (*str != '\0')
    {
        ESP_UART_WriteByte((uint8_t)*str);
        str++;
    }
}

void ESP_UART_Init(void)
{
    uint32_t uartClockHz = CLOCK_GetFreq(UART2_CLK_SRC);
    uint32_t divisorTimes32 = (uint32_t)((((uint64_t)uartClockHz * 2ULL) + (ESP_UART_BAUD_RATE / 2U)) / ESP_UART_BAUD_RATE);
    uint16_t sbr = (uint16_t)(divisorTimes32 / 32U);
    uint8_t brfa = (uint8_t)(divisorTimes32 % 32U);

    SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;
    SIM->SCGC4 |= SIM_SCGC4_UART2_MASK;

    PORTE->PCR[ESP_UART_TX_PIN] = PORT_PCR_MUX(ESP_UART_PIN_MUX);
    PORTE->PCR[ESP_UART_RX_PIN] = PORT_PCR_MUX(ESP_UART_PIN_MUX);

    UART2->C2 = 0U;
    UART2->BDH = (uint8_t)((UART2->BDH & ~UART_BDH_SBR_MASK) | UART_BDH_SBR((sbr >> 8U) & UART_BDH_SBR_MASK));
    UART2->BDL = UART_BDL_SBR(sbr & 0xFFU);
    UART2->C1 = 0U;
    UART2->C3 = 0U;
    UART2->C4 = (uint8_t)((UART2->C4 & ~UART_C4_BRFA_MASK) | UART_C4_BRFA(brfa));
    UART2->C2 = UART_C2_TE_MASK | UART_C2_RE_MASK;
}

void ESP_UART_SendTelemetry(uint16_t lightRaw, uint16_t micP2P, bool systemStarted, bool alertLatched)
{
    char frame[48];
    (void)snprintf(frame, sizeof(frame), "$MCXC,%u,%u,%u,%u\r\n",
                   (unsigned int)lightRaw,
                   (unsigned int)micP2P,
                   systemStarted ? 1U : 0U,
                   alertLatched ? 1U : 0U);

    ESP_UART_WriteString(frame);
}
