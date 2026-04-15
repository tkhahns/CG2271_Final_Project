#include "esp_uart.h"

#include <stdio.h>
#include <string.h>

#include "MCXC444.h"
#include "fsl_clock.h"
#include "fsl_debug_console.h"

#define ESP_UART_BAUD_RATE 9600
#define ESP_UART_TX_PIN    22
#define ESP_UART_RX_PIN    23
#define ESP_UART_PIN_MUX   4
#define UART2_INT_PRIO     128
#define SUG_BUF_LEN        80U

static char     s_suggestionBuf[SUG_BUF_LEN];
static bool     s_suggestionReady  = false;
static char     s_rxBuffer[64];
static volatile char    s_isrBuffer[64];
static volatile uint8_t s_isrIndex  = 0U;
static volatile bool    s_frameReady = false;
static bool     s_remoteValid           = false;
static uint8_t  s_lastRemoteActiveCount = 0U;
static int16_t  s_lastRemoteTempDeci    = -1;
static int16_t  s_lastRemoteDistDeci    = -1;
static bool     s_uartInitialized       = false;

static void ESP_UART_WriteByte(uint8_t byte)
{
    while ((UART2->S1 & UART_S1_TDRE_MASK) == 0U) { }
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

static void ESP_UART_HandleFrame(const char *frame)
{
    unsigned int remoteActiveCount = 0U;
    int remoteTempDeci = -1;
    int remoteDistDeci = -1;

    /* Gemini suggestion sentence */
    if (strncmp(frame, "$SUG,", 5U) == 0)
    {
        strncpy(s_suggestionBuf, frame + 5U, SUG_BUF_LEN - 1U);
        s_suggestionBuf[SUG_BUF_LEN - 1U] = '\0';
        s_suggestionReady = true;
        PRINTF("SUG: %s\r\n", s_suggestionBuf);
        return;
    }

    if (sscanf(frame, "$ESP,%u,%d,%d",
               &remoteActiveCount, &remoteTempDeci, &remoteDistDeci) == 3)
    {
        s_lastRemoteActiveCount = (uint8_t)remoteActiveCount;
        s_lastRemoteTempDeci    = (int16_t)remoteTempDeci;
        s_lastRemoteDistDeci    = (int16_t)remoteDistDeci;
        s_remoteValid = true;
        return;
    }

    if (sscanf(frame, "$ESP,%u", &remoteActiveCount) == 1)
    {
        s_lastRemoteActiveCount = (uint8_t)remoteActiveCount;
        s_lastRemoteTempDeci    = -1;
        s_lastRemoteDistDeci    = -1;
        s_remoteValid = true;
    }
}

void UART2_FLEXIO_IRQHandler(void)
{
    const uint8_t s1 = UART2->S1;

    if ((s1 & UART_S1_RDRF_MASK) != 0U)
    {
        const char rx = (char)UART2->D;

        if (rx == '\r') { return; }

        if (rx == '\n')
        {
            if ((s_isrIndex > 0U) && !s_frameReady)
            {
                s_isrBuffer[s_isrIndex] = '\0';
                memcpy(s_rxBuffer, (const void *)s_isrBuffer,
                       (size_t)s_isrIndex + 1U);
                s_frameReady = true;
            }
            s_isrIndex = 0U;
            return;
        }

        if (s_isrIndex < (sizeof(s_isrBuffer) - 1U))
        {
            s_isrBuffer[s_isrIndex++] = rx;
        }
        else
        {
            s_isrIndex = 0U;
        }
    }
    else if ((s1 & (UART_S1_OR_MASK | UART_S1_NF_MASK |
                    UART_S1_FE_MASK | UART_S1_PF_MASK)) != 0U)
    {
        (void)UART2->D;
        s_isrIndex = 0U;
    }
}

void ESP_UART_Init(void)
{
    uint32_t busClkHz;
    uint32_t sbr;

    if (s_uartInitialized) { return; }

    NVIC_DisableIRQ(UART2_FLEXIO_IRQn);

    SIM->SCGC4 |= SIM_SCGC4_UART2_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;

    UART2->C2 &= ~(UART_C2_TE_MASK | UART_C2_RE_MASK);

    PORTE->PCR[ESP_UART_TX_PIN] &= ~PORT_PCR_MUX_MASK;
    PORTE->PCR[ESP_UART_TX_PIN] |=  PORT_PCR_MUX(ESP_UART_PIN_MUX);
    PORTE->PCR[ESP_UART_RX_PIN] &= ~PORT_PCR_MUX_MASK;
    PORTE->PCR[ESP_UART_RX_PIN] |=  PORT_PCR_MUX(ESP_UART_PIN_MUX);

    busClkHz = CLOCK_GetBusClkFreq();
    sbr = (busClkHz + (ESP_UART_BAUD_RATE * 8U)) / (ESP_UART_BAUD_RATE * 16U);

    UART2->BDH &= (uint8_t)~UART_BDH_SBR_MASK;
    UART2->BDH |= (uint8_t)((sbr >> 8U) & UART_BDH_SBR_MASK);
    UART2->BDL  = (uint8_t)(sbr & 0xFFU);
    UART2->C1   = 0U;
    UART2->C3   = 0U;
    UART2->C4  &= (uint8_t)~UART_C4_BRFA_MASK;
    UART2->S2   = 0U;
    UART2->C2   = UART_C2_RIE_MASK | UART_C2_RE_MASK | UART_C2_TE_MASK;

    NVIC_SetPriority(UART2_FLEXIO_IRQn, UART2_INT_PRIO);
    NVIC_ClearPendingIRQ(UART2_FLEXIO_IRQn);
    NVIC_EnableIRQ(UART2_FLEXIO_IRQn);

    s_isrIndex              = 0U;
    s_frameReady            = false;
    s_remoteValid           = false;
    s_lastRemoteActiveCount = 0U;
    s_lastRemoteTempDeci    = -1;
    s_lastRemoteDistDeci    = -1;
    s_uartInitialized       = true;
}

void ESP_UART_ServiceRx(void)
{
    if (!s_uartInitialized) { return; }

    if (s_frameReady)
    {
        s_frameReady = false;
        ESP_UART_HandleFrame(s_rxBuffer);
    }
}

void ESP_UART_GetRemoteCount(bool *remoteValid, uint8_t *remoteActiveCount)
{
    *remoteValid       = s_remoteValid;
    *remoteActiveCount = s_lastRemoteActiveCount;
}

void ESP_UART_GetRemoteData(bool *remoteValid,
                            uint8_t *remoteActiveCount,
                            float *remoteTemperatureC,
                            bool *remoteTemperatureValid,
                            float *remoteDistanceCm,
                            bool *remoteDistanceValid)
{
    *remoteValid           = s_remoteValid;
    *remoteActiveCount     = s_lastRemoteActiveCount;
    *remoteTemperatureValid = (s_lastRemoteTempDeci >= 0);
    *remoteDistanceValid    = (s_lastRemoteDistDeci >= 0);

    *remoteTemperatureC = (*remoteTemperatureValid)
        ? ((float)s_lastRemoteTempDeci / 10.0f) : 0.0f;

    *remoteDistanceCm = (*remoteDistanceValid)
        ? ((float)s_lastRemoteDistDeci / 10.0f) : 0.0f;
}

void ESP_UART_GetSuggestion(bool *ready, char *buf, uint8_t bufLen)
{
    *ready = s_suggestionReady;
    if (s_suggestionReady && buf != NULL && bufLen > 0U)
    {
        strncpy(buf, s_suggestionBuf, bufLen - 1U);
        buf[bufLen - 1U] = '\0';
        s_suggestionReady = false;
    }
}

void ESP_UART_SendTelemetry(const SensorPacket *packet,
                            bool systemStarted,
                            bool alertSuppressed)
{
    char frame[64];

    if (!s_uartInitialized) { return; }

    (void)snprintf(frame, sizeof(frame), "$MCXC,%u,%u,%u,%u\r\n",
                   (unsigned int)packet->lightRaw,
                   (unsigned int)packet->micP2P,
                   systemStarted    ? 1U : 0U,
                   alertSuppressed  ? 1U : 0U);

    ESP_UART_WriteString(frame);
}
