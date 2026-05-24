/*
  Core Overheating Module
  
  Uses MH sound sensor (microphone) and LCD display.
  This is a triggered event that activates after specified time.
  Player must blow on the microphone for 5 seconds to cool the core.
  While active, all other modules are paused.
  
  Microphone Setup:
  - MH-series sound sensor ("sound sensor blue")
  - OUT pin connected to DIGITAL pin 6
  - GND and VCC connected to ground and 5V
  - Potentiometer on module adjusts detection sensitivity threshold
  
  Synchronized Flashing:
  During the alert phase, simultaneously flashes:
  - LCD backlight (red)
  - 4-digit timer display
  - LED bar
  - Frequency: ~2Hz (500ms on, 500ms off)
*/

#ifndef CORE_MODULE_H
#define CORE_MODULE_H

#include "rgb_lcd.h"

// Forward declarations
extern Grove_LED_Bar ledBar;
extern TM1637 tm1637;

// =====================================================
// CORE OVERHEATING MODULE
// =====================================================

// Microphone sensor pin (digital input)
const int MIC_SENSOR_PIN = 7;

// Debounce settings for microphone detection
// Allow up to 800ms of silence (enough for a normal breath) without losing progress
const unsigned long MIC_BREATH_TIMEOUT = 800;  // Allows breathing breaks
const unsigned long MIC_READ_INTERVAL = 100;   // Read sensor every 100ms
const unsigned long BLOW_DURATION = 5000;      // Total continuous accumulation time needed
const unsigned long FLASH_DURATION = 3000;     // Flash for 3 seconds
const unsigned long FLASH_CYCLE = 500;         // 500ms per half-cycle (2Hz total)

bool coreSolved = false;
bool coreTriggered = false;  // Tracks if core event has been activated
bool coreBlowing = false;    // Currently detecting sound
bool coreMessageShown = false;
bool coreDisplayCleared = false;  // Track if we've cleared the "CORE STABLE" message
unsigned long coreBlowStart = 0;  // When the accumulated blowing started (for cumulative time)
unsigned long coreLastMicRead = 0;
unsigned long micLastDetectionTime = 0;  // Track last time sound was detected
unsigned long coreFlashStart = 0;
unsigned long coreSolvedTime = 0;  // Track when core was solved
bool coreFlashing = false;
unsigned long coreAccumulatedBlowTime = 0;  // Cumulative blow time across breaths

// Forward declarations - these objects are declared in main file
extern rgb_lcd lcd;
extern String bombSerialNumber;
extern int currentDigits[4];  // Timer display digits

void setupCoreModule() {
  pinMode(MIC_SENSOR_PIN, INPUT);  // Set microphone pin as digital input
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
    coreAccumulatedBlowTime = 0;  // Reset accumulated blow time
    
    // Clear the maze display when core is triggered
    extern bool mazeSetupDone;
    extern void clearMazeDisplay();
    if (mazeSetupDone) {
      clearMazeDisplay();
    }
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
  if (!coreTriggered) {
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

  if (coreSolved) {
    // Display "CORE STABLE" for 2 seconds, then clear to return to normal display
    if (!coreDisplayCleared) {
      if (now - coreSolvedTime >= 2000) {
        // Clear the display after 2 seconds
        lcd.clear();
        coreDisplayCleared = true;
        // Signal to the main display logic to refresh the serial number
        extern bool serialNumberDisplayed;
        serialNumberDisplayed = false;
      }
    }
    return;
  }

  // Check microphone sensor for blow detection with breath-tolerant debouncing
  // Read once per 100ms to avoid noise
  if (now - coreLastMicRead >= MIC_READ_INTERVAL) {
    coreLastMicRead = now;
    bool micDetected = digitalRead(MIC_SENSOR_PIN) == HIGH;

    if (micDetected) {
      // Microphone detected sound - update the last detection time
      micLastDetectionTime = now;
      
      if (!coreBlowing) {
        // Just started blowing - begin accumulation period
        coreBlowing = true;
        if (coreAccumulatedBlowTime == 0) {
          coreBlowStart = now;  // Only set start time on very first blow
        }
      }
    } else {
      // No sound detected - check if we should keep accumulating or reset
      if (coreBlowing) {
        // Check if silence has exceeded our breath timeout
        if (now - micLastDetectionTime >= MIC_BREATH_TIMEOUT) {
          // Silence too long - player stopped blowing, reset accumulation
          coreBlowing = false;
          coreAccumulatedBlowTime = 0;
          if (coreMessageShown && !coreFlashing) {
            lcd.setCursor(0, 1);
            lcd.print("BLOW TO COOL    ");
          }
        }
        // Otherwise, stay in coreBlowing state during the breath (tolerating silence)
      }
    }
  }

  if (coreBlowing) {
    // Calculate time since start of this session (including all breaths)
    unsigned long totalElapsedTime = now - coreBlowStart;
    int secondsLeft = (BLOW_DURATION - totalElapsedTime) / 1000 + 1;
    
    lcd.setCursor(0, 1);
    lcd.print("KEEP BLOWING: ");
    lcd.print(secondsLeft);
    lcd.print("s ");

    if (totalElapsedTime >= BLOW_DURATION) {
      // Success! Core has been blown on for 5 seconds total (with breath breaks allowed)
      coreSolved = true;
      coreSolvedTime = now;  // Record when solved
      coreDisplayCleared = false;  // Reset flag
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("CORE STABLE");
      lcd.setRGB(0, 255, 0);  // Green backlight when solved
      ledBar.setLevel(0);      // LED bar off
      coreAccumulatedBlowTime = 0;  // Reset for potential future attempts
    }
  }
}

#endif
