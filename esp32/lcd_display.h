#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <Arduino.h>

void initLCD();
void displayText(const String& line1, const String& line2);
void scrollText(const String& text);

#endif
