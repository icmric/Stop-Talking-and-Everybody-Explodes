/*
  Core Overheating Module
  
  Uses MH sound sensor (microphone) and generic I2C LCD display.
  This is a triggered event that activates after specified time.
  Player must blow on the microphone for 5 seconds to cool the core.
  While active, all other modules are paused.
  
  Microphone Setup:
  - MH-series sound sensor ("sound sensor blue")
  - OUT pin connected to DIGITAL pin 6
  - GND and VCC connected to ground and 5V
  - Potentiometer on module adjusts detection sensitivity threshold
  
  I2C LCD Setup:
  - Generic 16x2 I2C LCD module
  - SDA → Arduino SDA (pin 20 on Mega)
  - SCL → Arduino SCL (pin 21 on Mega)
  - Address: 0x27 (adjust if different)
  - No RGB backlight support - uses simple on/off
  
  Synchronized Flashing:
  During the alert phase, simultaneously flashes:
  - LCD backlight (on/off only - no RGB on generic I2C)
  - 4-digit timer display
  - LED bar
  - Frequency: ~2Hz (500ms on, 500ms off)
*/

#ifndef CORE_MODULE_H
#define CORE_MODULE_H

#include <LiquidCrystal_I2C.h>

// Forward declarations
extern DIYables_4Digit7Segment_TM1637 tm1637;

// Forward declaration for generic LED bar control
void setLedLevel(int level);

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
extern LiquidCrystal_I2C lcd;
extern String bombSerialNumber;
extern int currentDigits[4];  // Timer display digits

void setupCoreModule() {
  pinMode(MIC_SENSOR_PIN, INPUT);  // Set microphone pin as digital input
  lcd.init();                       // Initialize I2C LCD
  lcd.backlight();                  // Turn on backlight
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
        lcd.backlight();         // Turn backlight on (white only on generic I2C)
        setLedLevel(10);         // LED bar full brightness
        
        // 4-digit display shows current time (construct number from digits)
        // currentDigits[0] = tens of minutes, [1] = ones of minutes, [2] = tens of seconds, [3] = ones of seconds
        int displayValue = (currentDigits[0] * 1000) + (currentDigits[1] * 100) + (currentDigits[2] * 10) + currentDigits[3];
        tm1637.print(displayValue);
      } else {
        // FLASH OFF
        lcd.noBacklight();       // Backlight off
        setLedLevel(0);          // LED bar off
        
        // 4-digit display off (display empty/blank)
        tm1637.print(0);  // Blank display
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
      lcd.backlight();  // Keep backlight on after flashing
      setLedLevel(0);   // LED bar off
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
      lcd.backlight();  // Keep backlight on (no RGB color change on generic I2C)
      setLedLevel(0);   // LED bar off
      coreAccumulatedBlowTime = 0;  // Reset for potential future attempts
    }
  }
}

#endif
