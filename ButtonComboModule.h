/*
  Button Combo Module
  
  Uses two buttons to input a sequence. Player must press RED → RED → GREEN within time limit.
*/

#ifndef BUTTON_COMBO_MODULE_H
#define BUTTON_COMBO_MODULE_H

// Forward declarations
extern void playSuccessTone();

// =====================================================
// BUTTON COMBO MODULE
// =====================================================

const int RED_BUTTON_PIN = 28;      // New pin, not conflicting
const int GREEN_BUTTON_PIN = 33;    // Available
const unsigned long BUTTON_TIME_LIMIT = 5000;

int buttonSequence[3] = {0, 0, 1};  // 0=RED, 1=GREEN
int buttonStepIndex = 0;
unsigned long buttonStartTime = 0;
bool buttonStarted = false;
bool buttonComboSolved = false;

void setupButtonComboModule() {
  pinMode(RED_BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Button combo module initialized");
  Serial.println("Press RED → RED → GREEN");
}

void handleButtonPress(int button) {
  if (button == 0) {
    Serial.println("Button: RED");
    tone(BUZZER_PIN, 900, 100);
  } else {
    Serial.println("Button: GREEN");
    tone(BUZZER_PIN, 900, 100);
  }

  if (button == buttonSequence[buttonStepIndex]) {
    buttonStepIndex++;
    Serial.print("Button: Step ");
    Serial.println(buttonStepIndex);

    if (buttonStepIndex == 3) {
      buttonComboSolved = true;
      Serial.println("Button combo module solved!");
      playSuccessTone();
    }
  } else {
    Serial.println("Button: WRONG - RESET");
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
  }

  // Check time limit
  if (millis() - buttonStartTime > BUTTON_TIME_LIMIT) {
    Serial.println("Button: TIME OUT - RESET");
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
