/*
  Button Combo Module
  
  Uses two buttons to input a sequence. Player must press RED → RED → GREEN within time limit.
  Red button has an LED indicator (D10) that lights up when the module is active.
*/

#ifndef BUTTON_COMBO_MODULE_H
#define BUTTON_COMBO_MODULE_H

// Forward declarations
extern const int BUZZER_PIN;
extern const int LED_PIN;            // D10 - Red button LED from main file
extern void playSuccessTone();

// =====================================================
// BUTTON COMBO MODULE
// =====================================================

const int RED_BUTTON_PIN = 11;        // Red button signal pin (D11)
const int GREEN_BUTTON_PIN = 33;      // Green button signal pin (D33)
const unsigned long BUTTON_TIME_LIMIT = 5000;

int buttonSequence[3] = {0, 0, 1};  // 0=RED, 1=GREEN
int buttonStepIndex = 0;
unsigned long buttonStartTime = 0;
bool buttonStarted = false;
bool buttonComboSolved = false;
bool buttonComboActive = false;        // Track if module is active

void setupButtonComboModule() {
  pinMode(RED_BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_BUTTON_PIN, INPUT_PULLUP);
  // LED_PIN is controlled by the timer module, not this module
}

void handleButtonPress(int button) {
  if (button == 0) {
    tone(BUZZER_PIN, 900, 100);
  } else {
    tone(BUZZER_PIN, 900, 100);
  }

  if (button == buttonSequence[buttonStepIndex]) {
    buttonStepIndex++;

    if (buttonStepIndex == 3) {
      buttonComboSolved = true;
      playSuccessTone();
    }
  } else {
    tone(BUZZER_PIN, 200, 300);
    buttonStepIndex = 0;
    buttonStartTime = millis();  // Reset timer
  }
}

void updateButtonComboModule() {
  if (buttonComboSolved) {
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

  // Check button states
  if (digitalRead(RED_BUTTON_PIN) == LOW) {
    handleButtonPress(0);  // RED
    delay(250);
  }

  if (digitalRead(GREEN_BUTTON_PIN) == LOW) {
    handleButtonPress(1);  // GREEN
    delay(250);
  }
}

#endif
