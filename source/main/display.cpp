#include "display.h"
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C lcd(0x27, 16, 2);

void displayInit() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("ESP32 System");
  lcd.setCursor(0, 1);
  lcd.print("Initialized...");
}

void displayUpdate(const DeskState &state, uint8_t finalState) {
  lcd.setCursor(0, 0);
  lcd.print("T:");
  if (isnan(state.temp)) {
    lcd.print("ERR  ");
  } else {
    lcd.print(state.temp, 1);
    lcd.print("C");
  }

  lcd.setCursor(8, 0);
  switch (finalState) {
    case WARNING_STATE_IDLE:         lcd.print(" S:IDLE "); break;
    case WARNING_STATE_ACKNOWLEDGED: lcd.print(" S:ACK  "); break;
    case WARNING_STATE_YELLOW:       lcd.print(" S:YEL  "); break;
    case WARNING_STATE_RED:          lcd.print(" S:RED  "); break;
    case WARNING_STATE_RED_BUZZER:   lcd.print(" S:BUZZ "); break;
    default:                         lcd.print("        "); break;
  }

  lcd.setCursor(0, 1);
  lcd.print("D:");
  if (state.distance < 0) {
    lcd.print("ERR ");
  } else {
    lcd.print((int)state.distance);
    lcd.print("cm ");
  }

  lcd.setCursor(8, 1);
  lcd.print("L:");
  lcd.print(state.light);
  lcd.print("   ");
}

void displayInactive() {
  lcd.setCursor(0, 0);
  lcd.print("SYSTEM INACTIVE ");
  lcd.setCursor(0, 1);
  lcd.print("                ");
}
