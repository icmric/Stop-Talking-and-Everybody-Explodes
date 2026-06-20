#ifndef FREQUENCY_MODULE_H
#define FREQUENCY_MODULE_H

/*
  FrequencyModule.h
  Encoder Sequence module — player must turn two rotary encoders
  in the correct sequence derived from the serial number's digit sum.

  Penalty: wrong encoder or wrong direction = -3s.
  On completion: activates the Maze module and triggers the Core event.
*/

#include <Encoder.h>
#include "FrequencyPresets.h"
#include "SerialNumber.h"

extern ActiveEncoderModule activeEncoderModule;
extern void setupMazeModule();
extern void triggerCoreEvent();
extern void applyPenalty(const char* reason);
extern void setLedLevel(int level);
extern Encoder encLeft;
extern Encoder encRight;

// ── State ─────────────────────────────────────────────────────────────────────

const EncoderSequence* frequencySequence = nullptr;

int  freqStep             = 0;
int  freqClicks           = 0;
long freqLastLeftRead     = 0;
long freqLastRightRead    = 0;
unsigned long freqLastClickTime = 0;
bool frequencyModuleSolved = false;

const unsigned long FREQ_CONFIRM_TIME       = 500; // ms of stillness before step advances
const int           ENCODER_DEBOUNCE_TICKS  = 3;   // ignore changes smaller than this

// ── Setup ─────────────────────────────────────────────────────────────────────

void setupFrequencyModule() {
  freqStep              = 0;
  freqClicks            = 0;
  frequencyModuleSolved = false;
  frequencySequence     = getFrequencySequence(getDigitSum());
  freqLastLeftRead      = encLeft.read();
  freqLastRightRead     = encRight.read();
  setLedLevel(10);
}

// ── Update ────────────────────────────────────────────────────────────────────

void updateFrequencyModule() {
  if (frequencyModuleSolved || frequencySequence == nullptr) return;

  long leftRaw  = encLeft.read();
  long rightRaw = encRight.read();

  char needEncoder  = frequencySequence->encoder[freqStep];
  int  needDir      = frequencySequence->direction[freqStep];
  int  needClicks   = frequencySequence->clicks[freqStep];

  // Helper lambda-style logic via inline blocks for the active encoder
  if (needEncoder == 'L') {
    long change = leftRaw - freqLastLeftRead;
    if (abs(change) >= ENCODER_DEBOUNCE_TICKS) {
      int dir = (change > 0) ? -1 : 1;
      freqLastLeftRead = leftRaw;
      if (dir == needDir) {
        freqClicks += abs(change);
        freqLastClickTime = millis();
      } else {
        applyPenalty("FreqWrongDirection");
        freqClicks = 0;
        freqLastLeftRead  = encLeft.read();
        freqLastRightRead = encRight.read();
      }
    }
    // Penalise right encoder if touched while left is expected
    long wrongChange = rightRaw - freqLastRightRead;
    if (abs(wrongChange) >= ENCODER_DEBOUNCE_TICKS) {
      for (int i = 0; i < abs(wrongChange); i++) applyPenalty("FreqWrongEncoder");
      freqLastRightRead = rightRaw;
    }

  } else if (needEncoder == 'R') {
    long change = rightRaw - freqLastRightRead;
    if (abs(change) >= ENCODER_DEBOUNCE_TICKS) {
      int dir = (change > 0) ? -1 : 1;
      freqLastRightRead = rightRaw;
      if (dir == needDir) {
        freqClicks += abs(change);
        freqLastClickTime = millis();
      } else {
        applyPenalty("FreqWrongDirection");
        freqClicks = 0;
        freqLastLeftRead  = encLeft.read();
        freqLastRightRead = encRight.read();
      }
    }
    // Penalise left encoder if touched while right is expected
    long wrongChange = leftRaw - freqLastLeftRead;
    if (abs(wrongChange) >= ENCODER_DEBOUNCE_TICKS) {
      for (int i = 0; i < abs(wrongChange); i++) applyPenalty("FreqWrongEncoder");
      freqLastLeftRead = leftRaw;
    }
  }

  // Advance step once enough clicks have accumulated and encoder has been still
  if (freqClicks >= needClicks && (millis() - freqLastClickTime) >= FREQ_CONFIRM_TIME) {
    freqStep++;
    freqClicks = 0;
    freqLastLeftRead  = encLeft.read();
    freqLastRightRead = encRight.read();

    // Map remaining steps to LED level (starts full, drains toward 1)
    int level = map(freqStep, 0, frequencySequence->stepCount, 10, 1);
    setLedLevel(constrain(level, 1, 10));

    if (freqStep >= frequencySequence->stepCount) {
      frequencyModuleSolved = true;

      // Flash LED bar to signal completion
      for (int i = 0; i < 4; i++) {
        setLedLevel(10); delay(150);
        setLedLevel(0);  delay(150);
      }
      setLedLevel(10);

      activeEncoderModule = (ActiveEncoderModule)1;  // Switch to MAZE_MODULE
      setupMazeModule();
      triggerCoreEvent();
    }
  }
}

#endif