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
#include "FrequencyPresets.h"
#include "SerialNumberParser.h"

// Forward declare the enum and extern variables
enum ActiveEncoderModule;
extern ActiveEncoderModule activeEncoderModule;
extern void setupMazeModule();
extern void triggerCoreEvent();
extern void applyPenalty(const char* reason);

// Forward declarations
extern void setLedLevel(int level);  // Generic LED bar control
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

// Encoder position tracking - work with RAW values
// Each full encoder click = 4 raw ticks from the Encoder library
const int ENCODER_CLICKS_PER_DETENT = 4;

// Debouncing: ignore movements smaller than this (protects against contact bounce)
const int ENCODER_DEBOUNCE_THRESHOLD = 3;  // Ignore changes < 3 ticks

void setupFrequencyModule() {
  freqStep = 0;
  freqClicks = 0;
  frequencyModuleSolved = false;
  
  // Select sequence based on serial number digit sum
  int digitSum = getDigitSum();
  frequencySequence = getFrequencySequence(digitSum);
  
  // Store RAW encoder position
  freqLastLeftRead = encLeft.read();
  freqLastRightRead = encRight.read();
  
  setLedLevel(10);
}

void updateFrequencyModule() {
  if (frequencyModuleSolved || frequencySequence == nullptr) {
    return;
  }
  
  // Always read both encoders - work with RAW values
  long leftRaw = encLeft.read();
  long rightRaw = encRight.read();
  
  // Get current step requirements
  char needEncoder = frequencySequence->encoder[freqStep];
  int needDirection = frequencySequence->direction[freqStep];
  int needClicks = frequencySequence->clicks[freqStep];
  
  // Process only the encoder we need
  if (needEncoder == 'L') {
    long change = leftRaw - freqLastLeftRead;
    
    // Only process changes larger than debounce threshold (ignore contact bounce)
    if (abs(change) >= ENCODER_DEBOUNCE_THRESHOLD) {
      int dir = (change > 0) ? 1 : -1;
      freqLastLeftRead = leftRaw;
      
      if (dir == needDirection) {
        // Correct direction - count the RAW ticks
        freqClicks += abs(change);
        freqLastClickTime = millis();
      } else {
        // Wrong direction on left encoder - PENALTY
        applyPenalty("FreqWrongDirection");
        freqClicks = 0;
        // Reset baselines
        freqLastLeftRead = encLeft.read();
        freqLastRightRead = encRight.read();
      }
    }

    // Penalize wrong encoder input without affecting correct input logic
    long wrongChange = rightRaw - freqLastRightRead;
    if (abs(wrongChange) >= ENCODER_DEBOUNCE_THRESHOLD) {
      for (int i = 0; i < abs(wrongChange); i++) {
        applyPenalty("FreqWrongEncoder");
      }
      freqLastRightRead = rightRaw;
    }
  }
  else if (needEncoder == 'R') {
    long change = rightRaw - freqLastRightRead;

    // Only process changes larger than debounce threshold (ignore contact bounce)
    if (abs(change) >= ENCODER_DEBOUNCE_THRESHOLD) {
      int dir = (change > 0) ? 1 : -1;
      freqLastRightRead = rightRaw;
      
      if (dir == needDirection) {
        // Correct direction - count the RAW ticks
        freqClicks += abs(change);
        freqLastClickTime = millis();
      } else {
        // Wrong direction on right encoder - PENALTY
        applyPenalty("FreqWrongDirection");
        freqClicks = 0;
        // Reset baselines
        freqLastLeftRead = encLeft.read();
        freqLastRightRead = encRight.read();
      }
    }

    // Penalize wrong encoder input without affecting correct input logic
    long wrongChange = leftRaw - freqLastLeftRead;
    if (abs(wrongChange) >= ENCODER_DEBOUNCE_THRESHOLD) {
      for (int i = 0; i < abs(wrongChange); i++) {
        applyPenalty("FreqWrongEncoder");
      }
      freqLastLeftRead = leftRaw;
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
    
    // Capture fresh baseline for next step (work with RAW values)
    freqLastLeftRead = encLeft.read();
    freqLastRightRead = encRight.read();
    
    // Update display - map current step to LED bar level
    int level = map(freqStep, 0, frequencySequence->stepCount, 10, 1);
    if (level < 1) level = 1;
    if (level > 10) level = 10;
    setLedLevel(level);
    
    if (freqStep >= frequencySequence->stepCount) {
      frequencyModuleSolved = true;
      
      // Flash LED bar to indicate completion
      for (int i = 0; i < 4; i++) {
        setLedLevel(10);
        delay(150);
        setLedLevel(0);
        delay(150);
      }
      setLedLevel(10);
      
      // Setup maze and trigger core
      activeEncoderModule = (ActiveEncoderModule)1;
      setupMazeModule();
      triggerCoreEvent();
    }
  }
}

#endif
