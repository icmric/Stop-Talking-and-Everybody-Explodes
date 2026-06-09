#ifndef SIGNAL_ALIGNMENT_H
#define SIGNAL_ALIGNMENT_H

/*
  signalAlignment.h
  Compass Alignment module — two-stage directional puzzle using HMC5883L.

  Stage 1: if sum of serial digits is even → point North; else South. Hold 8s.
  Stage 2: if letterToNumber(letter1) == lastDigit → point North; else South. Hold 8s.

  The LED bar shows proximity to the target direction (10 = on target, 0 = opposite).
  Penalty: leaving the target window before the hold completes resets that stage's timer.
  No strike penalty here — the player simply has to re-hold.

  Hardware:
    - HMC5883L compass: I2C address 0x1E (SDA/SCL shared bus)
  
  Calibration:
    Update calOffset/calScale values using CompassCalibration.h if readings drift.
*/

#include <Wire.h>
#include "SerialNumber.h"

extern void playSuccessTone();
extern void setLedLevel(int level);

#define HMC5883L_ADDR 0x1E

const unsigned long SIGNAL_HOLD_TIME = 8000;  // ms to hold correct direction

// Calibration values — run CompassCalibration.h to regenerate if needed
int calOffsetX =  -28;
int calOffsetY =  232;
int calScaleX  =   91;
int calScaleY  =  110;

// ── State ──────────────────────────────────────────────────────────────────

int  signalStage           = 1;
bool signalAlignmentSolved = false;
bool signalHolding         = false;
int  signalHeading         = 0;
unsigned long signalHoldStart        = 0;
unsigned long signalLastCompassRead  = 0;

// ── Heading windows ────────────────────────────────────────────────────────

bool inNorthWindow(int h) { return (h >= 340 || h <= 20);  }
bool inSouthWindow(int h) { return (h >= 150 && h <= 210); }

// ── Target derivation ──────────────────────────────────────────────────────

bool getTargetNorth(int stage) {
  if (stage == 1) {
    return (getDigitSum() % 2 == 0);
  } else {
    return (letterToNumber(getSerialLetter1()) == getLastDigit());
  }
}

// ── LED proximity feedback ─────────────────────────────────────────────────

void updateSignalAlignmentLED(int heading, bool targetNorth) {
  int target    = targetNorth ? 0 : 180;
  int deviation = abs(heading - target);
  if (deviation > 180) deviation = 360 - deviation;

  int level;
  if      (deviation <= 20)  level = 10;
  else if (deviation >= 170) level = 0;
  else                       level = map(deviation, 20, 170, 9, 1);

  setLedLevel(level);
}

// ── Setup ──────────────────────────────────────────────────────────────────

void setupSignalAlignmentModule() {
  Wire.beginTransmission(HMC5883L_ADDR);
  Wire.write(0x00); Wire.write(0x70); Wire.endTransmission();  // 8-sample avg, 15Hz
  Wire.beginTransmission(HMC5883L_ADDR);
  Wire.write(0x01); Wire.write(0xA0); Wire.endTransmission();  // Gain 5
  Wire.beginTransmission(HMC5883L_ADDR);
  Wire.write(0x02); Wire.write(0x00); Wire.endTransmission();  // Continuous mode

  Serial.print(F("Signal Alignment active. Stage 1 target: "));
  Serial.println(getTargetNorth(1) ? F("NORTH") : F("SOUTH"));
}

// ── Update ─────────────────────────────────────────────────────────────────

void updateSignalAlignmentModule() {
  if (signalAlignmentSolved) return;

  unsigned long now = millis();

  // Read compass at 10Hz
  if (now - signalLastCompassRead >= 100) {
    signalLastCompassRead = now;

    Wire.beginTransmission(HMC5883L_ADDR);
    Wire.write(0x03); Wire.endTransmission(false);
    Wire.requestFrom(HMC5883L_ADDR, 6);

    int x = (Wire.read() << 8) | Wire.read();
    Wire.read(); Wire.read();  // discard Z axis
    int y = (Wire.read() << 8) | Wire.read();

    long cx = ((long)(x - calOffsetX) * calScaleX) / 100;
    long cy = ((long)(y - calOffsetY) * calScaleY) / 100;

    float rad = atan2(cy, cx);
    if (rad < 0) rad += 2 * PI;
    signalHeading = (int)(rad * 180.0 / PI);

    updateSignalAlignmentLED(signalHeading, getTargetNorth(signalStage));
  }

  bool targetNorth = getTargetNorth(signalStage);
  bool inCorrect   = targetNorth ? inNorthWindow(signalHeading) : inSouthWindow(signalHeading);

  if (inCorrect) {
    if (!signalHolding) {
      signalHolding   = true;
      signalHoldStart = now;
      Serial.println(F("Holding correct direction..."));
    }

    if (now - signalHoldStart >= SIGNAL_HOLD_TIME) {
      Serial.print(F("Stage ")); Serial.print(signalStage); Serial.println(F(" complete!"));
      playSuccessTone();

      if (signalStage == 2) {
        signalAlignmentSolved = true;
        setLedLevel(0);
        Serial.println(F("SIGNAL LOCKED — module complete"));
      } else {
        signalStage++;
        signalHolding   = false;
        signalHoldStart = 0;
        Serial.print(F("Stage 2 target: "));
        Serial.println(getTargetNorth(2) ? F("NORTH") : F("SOUTH"));
      }
    }
  } else {
    signalHolding   = false;
    signalHoldStart = 0;
  }
}

#endif