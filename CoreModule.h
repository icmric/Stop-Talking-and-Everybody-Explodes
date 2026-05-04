/*
  Core Overheating Module
  
  Uses DHT humidity sensor and LCD display.
  This is a triggered event that activates after specified time.
  Player must blow on the humidity sensor for 10 seconds to cool the core.
  While active, all other modules are paused.
  
  Synchronized Flashing:
  During the alert phase, simultaneously flashes:
  - LCD backlight (red)
  - 4-digit timer display
  - LED bar
  - Frequency: ~2Hz (500ms on, 500ms off)
*/

#ifndef CORE_MODULE_H
#define CORE_MODULE_H

#include <DHT.h>
#include "rgb_lcd.h"

// Forward declarations
extern Grove_LED_Bar ledBar;
extern TM1637 tm1637;

// =====================================================
// CORE OVERHEATING MODULE
// =====================================================

const float HUMIDITY_THRESHOLD = 90.0;
const unsigned long BLOW_DURATION = 10000;
const unsigned long FLASH_DURATION = 3000;  // Flash for 3 seconds
const unsigned long FLASH_CYCLE = 500;      // 500ms per half-cycle (2Hz total)

bool coreSolved = false;
bool coreTriggered = false;  // Tracks if core event has been activated
bool coreBlowing = false;
bool coreMessageShown = false;
unsigned long coreBlowStart = 0;
unsigned long coreLastDHTRead = 0;
unsigned long coreFlashStart = 0;
bool coreFlashing = false;

// Forward declarations - these objects are declared in main file
extern DHT dht;
extern rgb_lcd lcd;
extern String bombSerialNumber;
extern int currentDigits[4];  // Timer display digits

void setupCoreModule() {
  dht.begin();
  lcd.begin(16, 2);
  lcd.setRGB(255, 0, 0);  // Red backlight
  lcd.clear();
}

// Trigger the core overheating event (call this when you want to activate it)
void triggerCoreEvent() {
  if (!coreTriggered && !coreSolved) {
    coreTriggered = true;
    coreFlashing = true;
    coreFlashStart = millis();
    coreMessageShown = false;  // Reset message flag to show the message
    Serial.println("Core module triggered with alert flash!");
  }
}

// Check if core is currently flashing (used to pause other modules)
bool isCoreFlashing() {
  if (!coreFlashing) {
    return false;
  }
  
  if (millis() - coreFlashStart >= FLASH_DURATION) {
    coreFlashing = false;
    return false;
  }
  
  return true;
}

// Get current flash state (true = ON, false = OFF)
// Frequency: 2Hz = 500ms cycle
bool getCoreFlashState() {
  unsigned long elapsed = millis() - coreFlashStart;
  // Divide by FLASH_CYCLE to get half-cycle, mod 2 to determine state
  return ((elapsed / FLASH_CYCLE) % 2) == 0;
}

void updateCoreModule() {
  // Only run if core event has been triggered
  if (!coreTriggered || coreSolved) {
    return;
  }

  unsigned long now = millis();

  // Handle flashing alert phase - synchronize LCD, timer display, and LED bar
  if (coreFlashing) {
    if (now - coreFlashStart < FLASH_DURATION) {
      bool flashState = getCoreFlashState();
      
      if (flashState) {
        // FLASH ON
        lcd.setRGB(255, 0, 0);  // Red backlight on
        ledBar.setLevel(10);    // LED bar full brightness
        
        // 4-digit display shows current time
        int8_t displayDigits[4];
        displayDigits[0] = currentDigits[0];
        displayDigits[1] = currentDigits[1];
        displayDigits[2] = currentDigits[2];
        displayDigits[3] = currentDigits[3];
        tm1637.display(displayDigits);
      } else {
        // FLASH OFF
        lcd.setRGB(0, 0, 0);    // Backlight off
        ledBar.setLevel(0);     // LED bar off
        
        // 4-digit display off (show blanks)
        int8_t blank[4] = {-1, -1, -1, -1};
        tm1637.display(blank);
      }
      
      // Show the message once during flashing
      if (!coreMessageShown) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("CORE OVERHEAT!");
        lcd.setCursor(0, 1);
        lcd.print("BLOW TO COOL");
        coreMessageShown = true;
      }
    } else {
      // Flashing phase complete
      coreFlashing = false;
      lcd.setRGB(255, 0, 0);  // Keep red backlight after flashing
      ledBar.setLevel(0);      // LED bar off
    }
  }

  if (now - coreLastDHTRead >= 1200) {
    coreLastDHTRead = now;
    float humidity = dht.readHumidity();

    if (!isnan(humidity)) {
      if (humidity >= HUMIDITY_THRESHOLD) {
        if (!coreBlowing) {
          coreBlowing = true;
          coreBlowStart = now;
        }
      } else {
        coreBlowing = false;
        if (coreMessageShown && !coreFlashing) {
          lcd.setCursor(0, 1);
          lcd.print("BLOW TO COOL    ");
        }
      }
    }
  }

  if (coreBlowing) {
    int secondsLeft = (BLOW_DURATION - (now - coreBlowStart)) / 1000 + 1;
    lcd.setCursor(0, 1);
    lcd.print("KEEP BLOWING: ");
    lcd.print(secondsLeft);
    lcd.print("s ");

    if (now - coreBlowStart >= BLOW_DURATION) {
      coreSolved = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("CORE STABLE");
      lcd.setRGB(0, 255, 0);  // Green backlight when solved
      ledBar.setLevel(0);      // LED bar off
    }
  }
}

#endif
