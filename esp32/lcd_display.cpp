#include "lcd_display.h"
#include "config.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);

void initLCD() {
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Study Monitor");
    lcd.setCursor(0, 1);
    lcd.print("Starting...");
}

void displayText(const String& line1, const String& line2) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1.substring(0, LCD_COLS));
    lcd.setCursor(0, 1);
    lcd.print(line2.substring(0, LCD_COLS));
}

// Scroll long advisory text across line 2
void scrollText(const String& text) {
    lcd.setCursor(0, 0);
    lcd.print("Advisory:       ");

    if ((int)text.length() <= LCD_COLS) {
        lcd.setCursor(0, 1);
        lcd.print(text);
        return;
    }

    String padded = text + "   ";
    for (int i = 0; i <= (int)padded.length() - LCD_COLS; i++) {
        lcd.setCursor(0, 1);
        lcd.print(padded.substring(i, i + LCD_COLS));
        delay(300);
    }
}
