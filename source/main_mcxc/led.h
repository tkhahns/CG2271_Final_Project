#ifndef LED_H
#define LED_H

#include <stdbool.h>

typedef enum {
    RED, GREEN, BLUE
} TLED;

void LED_Init(void);
void LED_On(TLED led);
void LED_Off(TLED led);
void LED_OffAll(void);
void LED_SetRGB(bool r, bool g, bool b);

#endif /* LED_H */
