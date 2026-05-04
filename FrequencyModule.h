/*
  Frequency Scrambler Module
  
  Uses two encoders (left and right) to input a sequence of turns.
  Sequence is determined by serial number's digit sum (mod 5 gives 5 presets).
  Must complete the entire sequence to solve the module.
  
  Penalty: Wrong encoder or wrong direction = -3 seconds from timer
  
  Sequence derivation for manual operator:
  1. Sum the 4 digits of serial number
  2. Divide sum by 5, take remainder (0-4)
  3. Look up sequence in manual reference table
*/

#ifndef FREQUENCY_MODULE_H
#define FREQUENCY_MODULE_H

#include <Encoder.h>
#include <Grove_LED_Bar.h>
#include "FrequencyPresets.h"
#include "SerialNumberParser.h"

// Forward declare the enum and extern variables
enum ActiveEncoderModule;
extern ActiveEncoderModule activeEncoderModule;
extern void setupMazeModule();
extern void triggerCoreEvent();
extern void applyPenalty(const char* reason);

// Forward declarations
extern Grove_LED_Bar ledBar;
extern Encoder encLeft;
extern Encoder encRight;

// =====================================================
// FREQUENCY SCRAMBLER MODULE - SERIAL-DERIVED SEQUENCES
// =====================================================

// Current active sequence (selected based on serial number)
const EncoderSequence* frequencySequence = nullptr;

int freqStep = 0;
int freqClicks = 0;
long freqLastLeftRead = 0;
long freqLastRightRead = 0;
unsigned long freqLastClickTime = 0;
bool frequencyModuleSolved = false;
const unsigned long FREQ_CONFIRM_TIME = 500;

void setupFrequencyModule() {
  ledBar.begin();
  freqStep = 0;
  freqClicks = 0;
  frequencyModuleSolved = false;
  
  // Select sequence based on serial number digit sum
  int digitSum = getDigitSum();
  frequencySequence = getFrequencySequence(digitSum);
  
  freqLastLeftRead = encLeft.read() / 4;
  freqLastRightRead = encRight.read() / 4;
  
  ledBar.setLevel(10);
}

void updateFrequencyModule() {
  if (frequencyModuleSolved || frequencySequence == nullptr) {
    return;
  }
  
  // Always read both encoders
  long leftNow = encLeft.read() / 4;
  long rightNow = encRight.read() / 4;
  
  // Get current step requirements
  char needEncoder = frequencySequence->encoder[freqStep];
  int needDirection = frequencySequence->direction[freqStep];
  int needClicks = frequencySequence->clicks[freqStep];
  
  // Process only the encoder we need
  if (needEncoder == 'L') {
    long change = leftNow - freqLastLeftRead;
    
    if (change != 0) {
      int dir = (change > 0) ? 1 : -1;
      freqLastLeftRead = leftNow;
      
      if (dir == needDirection) {
        // Correct direction
        freqClicks += abs(change);
        freqLastClickTime = millis();
      } else {
        // Wrong direction on left encoder - PENALTY
        applyPenalty("FreqWrongDirection");
        freqClicks = 0;
        // Reset baselines
        freqLastLeftRead = encLeft.read() / 4;
        freqLastRightRead = encRight.read() / 4;
      }
    }
  } 
  else if (needEncoder == 'R') {
    long change = rightNow - freqLastRightRead;
    
    if (change != 0) {
      int dir = (change > 0) ? 1 : -1;
      freqLastRightRead = rightNow;
      
      if (dir == needDirection) {
        // Correct direction
        freqClicks += abs(change);
        freqLastClickTime = millis();
      } else {
        // Wrong direction on right encoder - PENALTY
        applyPenalty("FreqWrongDirection");
        freqClicks = 0;
        // Reset baselines
        freqLastLeftRead = encLeft.read() / 4;
        freqLastRightRead = encRight.read() / 4;
      }
    }
  }
  else if (needEncoder != ' ') {
    // Invalid encoder character in sequence - shouldn't happen
    frequencyModuleSolved = false;
    return;
  }
  
  // Check if step is complete
  if (freqClicks >= needClicks && (millis() - freqLastClickTime) >= FREQ_CONFIRM_TIME) {
    freqStep++;
    freqClicks = 0;
    
    // Capture fresh baseline for next step
    freqLastLeftRead = encLeft.read() / 4;
    freqLastRightRead = encRight.read() / 4;
    
    // Update display - map current step to LED bar level
    // If this is the last step, map(freqStep, 0, stepCount, 10, 1) would be 1
    // If this is the first step, map(freqStep, 0, stepCount, 10, 1) would be closer to 10
    int level = map(freqStep, 0, frequencySequence->stepCount, 10, 1);
    if (level < 1) level = 1;
    if (level > 10) level = 10;
    ledBar.setLevel(level);
    
    if (freqStep >= frequencySequence->stepCount) {
      frequencyModuleSolved = true;
      
      // Flash LED bar to indicate completion
      for (int i = 0; i < 4; i++) {
        ledBar.setLevel(10);
        delay(150);
        ledBar.setLevel(0);
        delay(150);
      }
      ledBar.setLevel(10);
      
      // Setup maze and trigger core
      activeEncoderModule = (ActiveEncoderModule)1;
      setupMazeModule();
      triggerCoreEvent();
    }
  }
}

#endif
