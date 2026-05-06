/*
  signal_alignment.h
  Compass (HMC5883L) - I2C 0x1E (SDA/SCL, shared bus)
  Buzzer (Mozzi)     - D9  *** D9 is reserved by Mozzi on Arduino Uno — move TM1637 DIO off D9 ***

  Packages used:
    Mozzi - Tim Barrass

  Usage in main sketch:
    1. #include "signal_alignment.h"  (before setup/loop)
    2. Call startMozzi(CONTROL_RATE) at the end of setup()
    3. Call audioHook() as the first line of loop()
    4. Call setupSignalAlignmentModule() when you want to activate it (e.g. when maze is solved)
    5. Call updateSignalAlignmentModule() every loop() while the module is active
*/

#ifndef SIGNAL_ALIGNMENT_H
#define SIGNAL_ALIGNMENT_H

// Forward declarations
extern void playSuccessTone();
extern String bombSerialNumber;
extern Grove_LED_Bar ledBar;
extern bool signalAlignmentModuleActive;  // Flag to indicate when signal alignment is running

#include <Wire.h>
#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/sin512_int8.h>

#define CONTROL_RATE  64
#define HMC5883L_ADDR 0x1E

const int PITCH_HIGH = 2000;
const int PITCH_LOW  = 500;
const unsigned long HOLD_TIME = 8000;

Oscil<SIN512_NUM_CELLS, AUDIO_RATE> osc(SIN512_DATA);

// Values from compass_calibration.ino
int calOffsetX = -28;
int calOffsetY = 232;
int calScaleX  = 91;
int calScaleY  = 110;

// signalCurrentFreq starts at 0 so the Mozzi oscillator is silent
// until the signal alignment module activates
int  signalCurrentFreq     = 0;
int  signalStage           = 1;
bool signalAlignmentSolved = false;
unsigned long signalHoldStart       = 0;
unsigned long signalLastCompassRead = 0;
bool signalHolding = false;
int  signalHeading = 0;

// Mozzi required callbacks — must be global
void updateControl() {
  osc.setFreq(signalCurrentFreq);
}

int updateAudio() {
  int sample = osc.next();
  return (sample + (sample >> 1)) >> 1;
}

bool inNorthWindow(int h) { return (h >= 340 || h <= 20); }
bool inSouthWindow(int h) { return (h >= 150 && h <= 210); }

// Calculate distance from heading to nearest target direction
// Returns 0-180 degrees (closest direction)
int getDeviationFromTarget(int heading, bool targetNorth) {
  int targetHeading = targetNorth ? 0 : 180;
  int deviation = abs(heading - targetHeading);
  // Wrap around 360 degrees
  if (deviation > 180) {
    deviation = 360 - deviation;
  }
  return deviation;
}

// Update LED bar based on how close we are to the target direction
// Maximum deviation is 180 degrees, so we use this to map to LED levels
void updateSignalAlignmentLED(int heading, bool targetNorth) {
  int deviation = getDeviationFromTarget(heading, targetNorth);
  
  // Map 0-180 degrees to 10-0 LED levels (closer = more LEDs lit)
  // 0-20 degrees (in target window) = full 10 LEDs
  // 180 degrees (opposite direction) = 0 LEDs
  int ledLevel;
  if (deviation <= 20) {
    ledLevel = 10;  // In target window
  } else if (deviation >= 170) {
    ledLevel = 0;   // Opposite direction
  } else {
    // Linear interpolation between 20-170 degrees
    ledLevel = map(deviation, 20, 170, 9, 1);
  }
  
  ledBar.setLevel(ledLevel);
}

int headingToPitch(int h) {
  if (h <= 180) return map(h, 0, 180, PITCH_HIGH, PITCH_LOW);
  else          return map(h, 180, 360, PITCH_LOW, PITCH_HIGH);
}

bool getTargetNorth(int stg) {
  if (stg == 1) {
    int sum = 0;
    for (int i = 4; i < 8; i++) sum += (bombSerialNumber[i] - '0');
    return (sum % 2 == 0);
  } else {
    int letterVal = bombSerialNumber[0] - 'A' + 1;
    int lastDigit = bombSerialNumber[7] - '0';
    return (letterVal == lastDigit);
  }
}

void signalPlayNote(int freq, int durationMs) {
  signalCurrentFreq = freq;
  unsigned long t = millis();
  while (millis() - t < durationMs) audioHook();
}

void signalSilence(int durationMs) {
  signalCurrentFreq = 0;
  unsigned long t = millis();
  while (millis() - t < durationMs) audioHook();
}

void setupSignalAlignmentModule() {
  Wire.beginTransmission(HMC5883L_ADDR);
  Wire.write(0x00);
  Wire.write(0x70);
  Wire.endTransmission();

  Wire.beginTransmission(HMC5883L_ADDR);
  Wire.write(0x01);
  Wire.write(0xA0);
  Wire.endTransmission();

  Wire.beginTransmission(HMC5883L_ADDR);
  Wire.write(0x02);
  Wire.write(0x00);
  Wire.endTransmission();

  Serial.println(F("Signal Alignment active."));
  Serial.print(F("Stage 1 target: "));
  Serial.println(getTargetNorth(1) ? F("NORTH") : F("SOUTH"));
}

void updateSignalAlignmentModule() {
  if (signalAlignmentSolved) {
    return;
  }

  unsigned long now = millis();

  if (now - signalLastCompassRead >= 100) {
    signalLastCompassRead = now;

    Wire.beginTransmission(HMC5883L_ADDR);
    Wire.write(0x03);
    Wire.endTransmission(false);
    Wire.requestFrom(HMC5883L_ADDR, 6);
    int x = (Wire.read() << 8) | Wire.read();
    Wire.read(); Wire.read(); // discard Z
    int y = (Wire.read() << 8) | Wire.read();

    long cx = ((long)(x - calOffsetX) * calScaleX) / 100;
    long cy = ((long)(y - calOffsetY) * calScaleY) / 100;

    float headingRad = atan2(cy, cx);
    if (headingRad < 0) headingRad += 2 * PI;
    signalHeading     = (int)(headingRad * 180.0 / PI);
    
    // Update LED bar based on heading (instead of Mozzi pitch)
    bool targetNorth = getTargetNorth(signalStage);
    updateSignalAlignmentLED(signalHeading, targetNorth);
  }

  bool targetNorth = getTargetNorth(signalStage);
  bool inCorrect   = targetNorth ? inNorthWindow(signalHeading) : inSouthWindow(signalHeading);

  if (inCorrect) {
    if (!signalHolding) {
      signalHolding   = true;
      signalHoldStart = now;
      Serial.println(F("Holding correct direction..."));
    }

    if (now - signalHoldStart >= HOLD_TIME) {
      Serial.print(F("Stage ")); Serial.print(signalStage); Serial.println(F(" complete!"));
      playSuccessTone();

      if (signalStage == 2) {
        signalAlignmentSolved = true;
        ledBar.setLevel(0);  // Turn off LED bar when complete
        Serial.println(F("SIGNAL LOCKED - MODULE COMPLETE"));
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
