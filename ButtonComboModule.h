#ifndef BUTTON_COMBO_MODULE_H
#define BUTTON_COMBO_MODULE_H

/*
  ButtonComboModule.h
  Player must press RED/GREEN buttons in a 4-step sequence derived from the
  serial number's digits: even digit → RED (0), odd digit → GREEN (1).

  Example: serial "ABCD-2468" → digits 2,4,6,8 (all even) → RED RED RED RED

  Penalty: wrong button press = -3s and sequence resets.
  The module can be attempted at any time.

  Hardware:
    - Red button:   D10 (INPUT_PULLUP, active LOW)
    - Green button: D11 (INPUT_PULLUP, active LOW)
*/

#include "SerialNumber.h"

extern void applyPenalty(const char* reason);

// ── Pins ───────────────────────────────────────────────────────────────────

const int RED_BUTTON_PIN   = 10;
const int GREEN_BUTTON_PIN = 11;

// ── State ──────────────────────────────────────────────────────────────────

// 0 = RED, 1 = GREEN
int buttonSequence[4];
int buttonStepIndex    = 0;
bool buttonComboSolved = false;

unsigned long buttonStartTime = 0;
const unsigned long BUTTON_TIME_LIMIT = 5000;  // Reset sequence if no press within 5s

// ── Setup ──────────────────────────────────────────────────────────────────

void setupButtonComboModule() {
  pinMode(RED_BUTTON_PIN,   INPUT_PULLUP);
  pinMode(GREEN_BUTTON_PIN, INPUT_PULLUP);

  for (int i = 0; i < 4; i++) {
    int digit = getSerialDigit(i + 1);
    if (digit < 0) digit = 0;
    buttonSequence[i] = digit % 2;  // Even → 0 (RED), odd → 1 (GREEN)
  }
}

// ── Input handler ──────────────────────────────────────────────────────────

void handleButtonPress(int button) {
  if (button == buttonSequence[buttonStepIndex]) {
    buttonStepIndex++;
    if (buttonStepIndex >= 4) buttonComboSolved = true;
  } else {
    applyPenalty("ButtonComboWrong");
    buttonStepIndex = 0;
    buttonStartTime = millis();
  }
}

// ── Update ─────────────────────────────────────────────────────────────────

void updateButtonComboModule() {
  if (buttonComboSolved) return;

  unsigned long now = millis();

  if (buttonStartTime == 0) buttonStartTime = millis();

  // Reset sequence if player takes too long between presses
  if (now - buttonStartTime > BUTTON_TIME_LIMIT) {
    buttonStepIndex = 0;
    buttonStartTime = now;
  }

  static int  lastRedState   = HIGH;
  static int  lastGreenState = HIGH;
  static unsigned long lastRedPress   = 0;
  static unsigned long lastGreenPress = 0;
  const unsigned long debounce = 50;

  int redState = digitalRead(RED_BUTTON_PIN);
  if (redState == LOW && lastRedState == HIGH && now - lastRedPress > debounce) {
    handleButtonPress(0);
    lastRedPress = now;
  }
  lastRedState = redState;

  int greenState = digitalRead(GREEN_BUTTON_PIN);
  if (greenState == LOW && lastGreenState == HIGH && now - lastGreenPress > debounce) {
    handleButtonPress(1);
    lastGreenPress = now;
  }
  lastGreenState = greenState;
}

#endif