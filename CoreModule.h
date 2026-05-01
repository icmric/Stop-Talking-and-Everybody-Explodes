/*
  Core Overheating Module
  
  Uses DHT humidity sensor and LCD display.
  Player must blow on the humidity sensor for 10 seconds to cool the core.
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

bool coreSolved = false;
bool coreBlowing = false;
bool coreMessageShown = false;
unsigned long coreBlowStart = 0;
unsigned long coreLastDHTRead = 0;

// Forward declarations - these objects are declared in main file
extern DHT dht;
extern rgb_lcd lcd;

void setupCoreModule() {
  dht.begin();
  lcd.begin(16, 2);
  lcd.setRGB(255, 0, 0);  // Red backlight
  lcd.clear();
}

void updateCoreModule() {
  if (coreSolved) {
    return;
  }

  if (!coreMessageShown) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CORE OVERHEAT!");
    lcd.setCursor(0, 1);
    lcd.print("BLOW TO COOL");
    coreMessageShown = true;
  }

  unsigned long now = millis();

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
        lcd.setCursor(0, 1);
        lcd.print("BLOW TO COOL    ");
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
    }
  }
}

#endif
