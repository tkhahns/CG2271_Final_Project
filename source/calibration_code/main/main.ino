#include <Arduino.h>
#include "config.h"
#include "comms.h"
#include "display.h"
#include "sensors.h"

// Define the global state variable here
DeskState myDesk = {0.0f, 0.0f, 0.0f, 0, false, true, OFF, 0};

void setup() {
  Serial.begin(115200);
  
  // Initialize all our modules
  commsInit();
  sensorsInit();
  displayInit();
  
  Serial.println("System fully initialized...");
  delay(1500); // Give the user time to read the boot screen
}

void loop() {
  // 1. Process incoming data from MCXC
  receiveFromMCXC(myDesk);

  // 2. Handle Inactive State
  if (!myDesk.systemActive) {
    myDesk.noise = false;
    myDesk.rgbState = OFF;
    digitalWrite(BUZZER_PIN, LOW);
    
    displayInactive();
    
    Serial.println("SYSTEM INACTIVE");
    delay(500);
    return;
  }

  // 3. Read Local Sensors
  sensorsRead(myDesk);

  // 4. Evaluate the New State
  uint8_t newState = evaluateState(myDesk);
  uint8_t finalState = newState;

  // 5. Apply the 10-second silence rule for Critical alerts
  if (newState == CRITICAL) {
    if (millis() - myDesk.lastAckTime < 10000) {
      finalState = WARNING; 
    }
  }

  // 6. Act on State Changes (RGB & Buzzer)
  if (finalState != myDesk.rgbState) {
    myDesk.rgbState = finalState;
    sendRGBCommand(finalState);

    if (finalState == CRITICAL) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }

  // 7. Update the LCD
  displayUpdate(myDesk, finalState);

  // 8. Debug Printing
  printDistance(myDesk.distance);
  printHumidity(myDesk.humidity);
  printTemperature(myDesk.temp);
  Serial.println();
  
  delay(500);
}