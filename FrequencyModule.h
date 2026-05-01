/*
  Frequency Scrambler Module
  
  Uses two encoders (left and right) to input a sequence of turns.
  Must complete the combination sequence to solve the module.
*/

#ifndef FREQUENCY_MODULE_H
#define FREQUENCY_MODULE_H

#include <Encoder.h>
#include <Grove_LED_Bar.h>

// Forward declare the enum and extern variables
enum ActiveEncoderModule;
extern ActiveEncoderModule activeEncoderModule;
extern void setupMazeModule();

// Forward declarations - these objects are declared in main file
extern Grove_LED_Bar ledBar;
extern Encoder encLeft;
extern Encoder encRight;

// =====================================================
// FREQUENCY SCRAMBLER MODULE
// =====================================================

struct ComboStep {
  char encoder;
  int direction; // 1 = CW, -1 = CCW
  int clicks;
};

ComboStep combination[] = {
  {'L', 1, 3},
  {'R', -1, 2},
  {'L', -1, 2}
};

int frequencyCurrentStep = 0;
const int frequencyTotalSteps = 3;
int frequencyClicksInCurrentStep = 0;
long frequencyLastLeftPos = 0;
long frequencyLastRightPos = 0;

unsigned long frequencyLastMoveTime = 0;
const int frequencyConfirmDelay = 500;
bool frequencyModuleSolved = false;

void updateFrequencyDisplay() {
  int displayLevel = map(frequencyCurrentStep, 0, frequencyTotalSteps, 10, 1);
  ledBar.setLevel(displayLevel);
}

void flashFrequencySuccess() {
  Serial.println("Frequency module solved!");
  for (int i = 0; i < 4; i++) {
    ledBar.setLevel(10); delay(150);
    ledBar.setLevel(0);  delay(150);
  }
  ledBar.setLevel(10);
}

void setupFrequencyModule() {
  ledBar.begin();
  frequencyLastLeftPos = encLeft.read() / 4;
  frequencyLastRightPos = encRight.read() / 4;
  updateFrequencyDisplay();
}

void updateFrequencyModule() {
  if (frequencyModuleSolved) {
    return;
  }

  long currLeft = encLeft.read() / 4;
  long currRight = encRight.read() / 4;
  ComboStep target = combination[frequencyCurrentStep];

  bool moved = false;
  int moveDir = 0;

  if (target.encoder == 'L' && currLeft != frequencyLastLeftPos) {
    moveDir = (currLeft > frequencyLastLeftPos) ? 1 : -1;
    moved = true;
    frequencyLastLeftPos = currLeft;
  } else if (target.encoder == 'R' && currRight != frequencyLastRightPos) {
    moveDir = (currRight > frequencyLastRightPos) ? 1 : -1;
    moved = true;
    frequencyLastRightPos = currRight;
  }

  if (moved) {
    frequencyLastMoveTime = millis();

    if (moveDir == target.direction) {
      frequencyClicksInCurrentStep++;
      Serial.print("Frequency click: ");
      Serial.println(frequencyClicksInCurrentStep);
    } else {
      frequencyClicksInCurrentStep = 0;
      Serial.println("Wrong direction, frequency step reset");
    }
  }

  if (frequencyClicksInCurrentStep >= target.clicks && millis() - frequencyLastMoveTime > frequencyConfirmDelay) {
    frequencyCurrentStep++;
    frequencyClicksInCurrentStep = 0;

    Serial.print("Frequency step confirmed: ");
    Serial.println(frequencyCurrentStep);
    updateFrequencyDisplay();

    if (frequencyCurrentStep >= frequencyTotalSteps) {
      frequencyModuleSolved = true;
      flashFrequencySuccess();
      // Note: Transition to maze module is handled in main file
      activeEncoderModule = (ActiveEncoderModule)1;  // MAZE_MODULE
      setupMazeModule();
    }
  }
}

#endif
