/*
  Button Combo Module
  
  Player must press buttons in a sequence derived from the serial number.
  Sequence: Based on the 4 digits - if digit is EVEN press RED, if ODD press GREEN.
  Always 4 button presses total (one for each digit).
  Wrong button press triggers -3 second penalty.
  
  Serial Example (ABC1-2345):
  - Digit 2 (EVEN) → RED
  - Digit 3 (ODD) → GREEN  
  - Digit 4 (EVEN) → RED
  - Digit 5 (ODD) → GREEN
  Sequence: RED → GREEN → RED → GREEN
*/

#ifndef BUTTON_COMBO_MODULE_H
#define BUTTON_COMBO_MODULE_H

#include "SerialNumberParser.h"

// Forward declarations
extern const int BUZZER_PIN;
extern const int LED_PIN;            // D10 - Red button LED from main file
extern void playSuccessTone();
extern void applyPenalty(const char* reason);
extern bool frequencyModuleSolved;   // Module dependencies
extern bool mazeSolved;
extern bool coreSolved;

// =====================================================
// BUTTON COMBO MODULE
// =====================================================

const int RED_BUTTON_PIN = 11;        // Red button signal pin (D11)
const int GREEN_BUTTON_PIN = 33;      // Green button signal pin (D33)
const unsigned long BUTTON_TIME_LIMIT = 5000;

// Sequence derived from serial number (0=RED, 1=GREEN)
int buttonSequence[4];           // Will be filled with 0s and 1s
int sequenceLength = 4;          // Always 4 presses (one per digit)
int buttonStepIndex = 0;
unsigned long buttonStartTime = 0;
bool buttonStarted = false;
bool buttonComboSolved = false;
bool buttonComboActive = false;

void setupButtonComboModule() {
  pinMode(RED_BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_BUTTON_PIN, INPUT_PULLUP);
  
  // Generate sequence from serial number
  for (int i = 0; i < 4; i++) {
    int digit = getSerialDigit(i + 1);  // Digit 1-4
    if (digit < 0) digit = 0;
    
    // EVEN = RED (0), ODD = GREEN (1)
    buttonSequence[i] = (digit % 2);
  }
}

void handleButtonPress(int button) {
  // Play button tone
  tone(BUZZER_PIN, 900, 100);
  
  // Check if this is the correct button
  if (button == buttonSequence[buttonStepIndex]) {
    // Correct button
    buttonStepIndex++;
    
    if (buttonStepIndex >= sequenceLength) {
      buttonComboSolved = true;
      playSuccessTone();
    }
  } else {
    // Wrong button - PENALTY
    applyPenalty("ButtonComboWrong");
    
    // Play error tone
    tone(BUZZER_PIN, 200, 300);
    
    // Reset sequence
    buttonStepIndex = 0;
    buttonStartTime = millis();  // Reset timer
  }
}

void updateButtonComboModule() {
  if (buttonComboSolved) {
    return;
  }
  
  // Check module dependencies - only active if other modules have started
  // Typically: only active after frequency module is complete
  bool moduleReady = frequencyModuleSolved;
  if (!moduleReady) {
    return;
  }
  
  if (!buttonStarted) {
    buttonStartTime = millis();
    buttonStarted = true;
    buttonComboActive = true;  // Module is now active
  }

  // Check time limit
  if (millis() - buttonStartTime > BUTTON_TIME_LIMIT) {
    buttonStepIndex = 0;
    buttonStartTime = millis();
  }

  // Check button states with proper debouncing
  static unsigned long lastRedPress = 0;
  static unsigned long lastGreenPress = 0;
  const unsigned long debounceDelay = 250;
  
  unsigned long now = millis();
  
  if (digitalRead(RED_BUTTON_PIN) == LOW) {
    if (now - lastRedPress > debounceDelay) {
      handleButtonPress(0);  // RED
      lastRedPress = now;
    }
  }

  if (digitalRead(GREEN_BUTTON_PIN) == LOW) {
    if (now - lastGreenPress > debounceDelay) {
      handleButtonPress(1);  // GREEN
      lastGreenPress = now;
    }
  }
}

#endif
