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
extern void triggerCoreEvent();

// Forward declarations
extern Grove_LED_Bar ledBar;
extern Encoder encLeft;
extern Encoder encRight;

// =====================================================
// FREQUENCY SCRAMBLER MODULE - MINIMAL REBUILD
// =====================================================

// Combination sequence: (encoder: 'L'/'R', direction: 1/-1, clicks needed)
const char COMBO_ENCODER[] = {'L', 'R', 'L'};
const int COMBO_DIRECTION[] = {1, -1, -1};
const int COMBO_CLICKS[] = {3, 2, 2};
const int COMBO_STEPS = 3;

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
  
  freqLastLeftRead = encLeft.read() / 4;
  freqLastRightRead = encRight.read() / 4;
  
  ledBar.setLevel(10);
}

void updateFrequencyModule() {
  if (frequencyModuleSolved) {
    return;
  }
  
  // Always read both encoders
  long leftNow = encLeft.read() / 4;
  long rightNow = encRight.read() / 4;
  
  char needEncoder = COMBO_ENCODER[freqStep];
  int needDirection = COMBO_DIRECTION[freqStep];
  int needClicks = COMBO_CLICKS[freqStep];
  
  // Process only the encoder we need
  if (needEncoder == 'L') {
    long change = leftNow - freqLastLeftRead;
    
    if (change != 0) {
      int dir = (change > 0) ? 1 : -1;
      freqLastLeftRead = leftNow;
      
      if (dir == needDirection) {
        freqClicks += abs(change);
        freqLastClickTime = millis();
      } else {
        freqClicks = 0;
      }
    }
  } 
  else if (needEncoder == 'R') {
    long change = rightNow - freqLastRightRead;
    
    if (change != 0) {
      int dir = (change > 0) ? 1 : -1;
      freqLastRightRead = rightNow;
      
      if (dir == needDirection) {
        freqClicks += abs(change);
        freqLastClickTime = millis();
      } else {
        freqClicks = 0;
      }
    }
  }
  
  // Check if step is complete
  if (freqClicks >= needClicks && (millis() - freqLastClickTime) >= FREQ_CONFIRM_TIME) {
    freqStep++;
    freqClicks = 0;
    
    // Capture fresh baseline for next step
    freqLastLeftRead = encLeft.read() / 4;
    freqLastRightRead = encRight.read() / 4;
    
    // Update display
    int level = map(freqStep, 0, COMBO_STEPS, 10, 1);
    ledBar.setLevel(level);
    
    if (freqStep >= COMBO_STEPS) {
      frequencyModuleSolved = true;
      
      // Flash LED bar
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
