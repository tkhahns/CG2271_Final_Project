#include <main logic/buttons.h>
#include "FreeRTOS.h"
#include "task.h"

#define SW2_START_PIN   3    /* PTC3 */
#define SW3_ACK_PIN     4    /* PTA4 */
#define DEBOUNCE_MS     200

extern SemaphoreHandle_t  g_buttonSema;
volatile bool      g_sw2StartPending  = false;
volatile bool      g_sw3AckPending    = false;

static volatile TickType_t s_lastSw2Tick = 0;
static volatile TickType_t s_lastSw3Tick = 0;

void Buttons_Init(SemaphoreHandle_t sema) {
    g_buttonSema = sema;

    NVIC_DisableIRQ(PORTC_PORTD_IRQn);
    NVIC_DisableIRQ(PORTA_IRQn);

    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTA_MASK;

    /* SW2 = PTC3: GPIO input, pull-up, falling-edge IRQ */
    PORTC->PCR[SW2_START_PIN] = PORT_PCR_MUX(1)
                               | PORT_PCR_PE_MASK
                               | PORT_PCR_PS_MASK
                               | PORT_PCR_IRQC(0b1010);
    GPIOC->PDDR &= ~(1U << SW2_START_PIN);

    /* SW3 = PTA4: GPIO input, pull-up, falling-edge IRQ */
    PORTA->PCR[SW3_ACK_PIN]   = PORT_PCR_MUX(1)
                               | PORT_PCR_PE_MASK
                               | PORT_PCR_PS_MASK
                               | PORT_PCR_IRQC(0b1010);
    GPIOA->PDDR &= ~(1U << SW3_ACK_PIN);

    /* Clear any stale interrupt flags (w1c: write 1 to clear) */
    PORTC->ISFR = (1U << SW2_START_PIN);
    PORTA->ISFR = (1U << SW3_ACK_PIN);

    NVIC_SetPriority(PORTC_PORTD_IRQn, 3);
    NVIC_ClearPendingIRQ(PORTC_PORTD_IRQn);
    NVIC_EnableIRQ(PORTC_PORTD_IRQn);

    NVIC_SetPriority(PORTA_IRQn, 3);
    NVIC_ClearPendingIRQ(PORTA_IRQn);
    NVIC_EnableIRQ(PORTA_IRQn);
}

// SW2 on PTC3 — START/STOP
void PORTC_PORTD_IRQHandler(void) {
    BaseType_t hpw = pdFALSE;
    TickType_t now = xTaskGetTickCountFromISR();

    if (PORTC->ISFR & (1U << SW2_START_PIN)) {
        PORTC->ISFR = (1U << SW2_START_PIN);   /* w1c */

        if ((now - s_lastSw2Tick) >= pdMS_TO_TICKS(DEBOUNCE_MS)) {
            s_lastSw2Tick     = now;
            g_sw2StartPending = true;
            xSemaphoreGiveFromISR(g_buttonSema, &hpw);
        }
    }

    portYIELD_FROM_ISR(hpw);
}


void PORTA_IRQHandler(void) {
    BaseType_t hpw = pdFALSE;
    TickType_t now = xTaskGetTickCountFromISR();

    if (PORTA->ISFR & (1U << SW3_ACK_PIN)) {
        PORTA->ISFR = (1U << SW3_ACK_PIN);

        if ((now - s_lastSw3Tick) >= pdMS_TO_TICKS(DEBOUNCE_MS)) {
            s_lastSw3Tick    = now;
            g_sw3AckPending  = true;
            xSemaphoreGiveFromISR(g_buttonSema, &hpw);
        }
    }

    portYIELD_FROM_ISR(hpw);
}
