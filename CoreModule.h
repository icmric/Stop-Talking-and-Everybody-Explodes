/*
  Core Overheating Module
  
  Uses DHT humidity sensor and LCD display.
  This is a triggered event that activates after the frequency module is completed.
  Player must blow on the humidity sensor for 10 seconds to cool the core.
  While active, all other modules are paused.
*/

#ifndef CORE_MODULE_H
#define CORE_MODULE_H

#include <DHT.h>
#include "rgb_lcd.h"

// =====================================================
// CORE OVERHEATING MODULE
// =====================================================

const float HUMIDITY_THRESHOLD = 90.0;
const unsigned long BLOW_DURATION = 10000;
const unsigned long FLASH_DURATION = 3000;  // Flash for 3 seconds
const int FLASH_INTERVAL = 200;              // Flash every 200ms

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

void updateCoreModule() {
  // Only run if core event has been triggered
  if (!coreTriggered || coreSolved) {
    return;
  }

  unsigned long now = millis();

  // Handle flashing alert phase - flash the backlight, not the text
  if (coreFlashing) {
    if (now - coreFlashStart < FLASH_DURATION) {
      // Flash the LCD backlight - toggle every FLASH_INTERVAL milliseconds
      bool flashState = ((now - coreFlashStart) / FLASH_INTERVAL) % 2 == 0;
      
      if (flashState) {
        lcd.setRGB(255, 0, 0);  // Red backlight on
      } else {
        lcd.setRGB(0, 0, 0);    // Backlight off
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
      coreFlashing = false;
      lcd.setRGB(255, 0, 0);  // Keep red backlight after flashing
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
    }
  }
}

#endif
