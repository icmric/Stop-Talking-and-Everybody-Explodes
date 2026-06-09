#ifndef DISTANCE_MODULE_H
#define DISTANCE_MODULE_H

/*
  DistanceModule.h
  Player holds an object at a target distance from the ultrasonic sensor
  for a required hold time to solve the module.

  Both values are derived from the serial number:
    Target distance : digit 3 × 10 cm  (clamped 10–50 cm)
    Hold time       : digit 4 × 0.5 s  (minimum 0.5 s)

  Example: serial "ABCD-1240" → digit3=4 → 40cm, digit4=0 → 0.5s (min)

  The LED bar shows proximity feedback while the frequency module LED is free
  (i.e. after frequencyModuleSolved is true).

  Hardware:
    - Generic HC-SR04-style ultrasonic sensor via Ultrasonic library
    - Trigger/Echo pin defined as ULTRASONIC_PIN in ktane.ino
*/

#include <Ultrasonic.h>
#include "SerialNumber.h"

extern void playSuccessTone();
extern void setLedLevel(int level);
extern const int ULTRASONIC_PIN;
extern bool frequencyModuleSolved;

// ── Configuration (populated in setup) ────────────────────────────────────

int TARGET_MIN = 10;
int TARGET_MAX = 15;
unsigned long DISTANCE_HOLD_TIME = 3000;

const int MAX_DISTANCE = 100;
const unsigned long DISTANCE_MEASURE_INTERVAL = 50;

// ── State ──────────────────────────────────────────────────────────────────

bool distanceSolved   = false;
bool distanceInRange  = false;
unsigned long distanceHoldStart   = 0;
unsigned long distanceLastMeasure = 0;

Ultrasonic ultrasonic(ULTRASONIC_PIN);

// ── Setup ──────────────────────────────────────────────────────────────────

void setupDistanceModule() {
  int d3 = getSecondToLastDigit();
  int d4 = getLastDigit();

  int requiredDistance = constrain(d3 * 10, 10, 50);
  TARGET_MIN = requiredDistance - 5;
  TARGET_MAX = requiredDistance + 5;

  int holdMs = max(d4 * 500, 500);
  DISTANCE_HOLD_TIME = (unsigned long)holdMs;

  Serial.print("Distance module: target ");
  Serial.print(requiredDistance);
  Serial.print("cm (");
  Serial.print(TARGET_MIN); Serial.print("–"); Serial.print(TARGET_MAX);
  Serial.print("cm), hold ");
  Serial.print(DISTANCE_HOLD_TIME / 1000.0);
  Serial.println("s");
}

// ── LED feedback ───────────────────────────────────────────────────────────

void updateDistanceDisplay(int distance) {
  int level;
  if (distance >= TARGET_MIN && distance <= TARGET_MAX) {
    level = 10;  // In range — lock bar at full to avoid jitter
  } else {
    int center    = (TARGET_MIN + TARGET_MAX) / 2;
    int tolerance = (TARGET_MAX - TARGET_MIN) / 2;
    int deviation = abs(distance - center);

    if (deviation <= tolerance) {
      level = map(deviation, 0, tolerance, 10, 5);
    } else {
      level = map(deviation, tolerance, tolerance + 2*tolerance, 5, 1);
      level = constrain(level, 1, 5);
    }
  }
  setLedLevel(level);
}

// ── Hold check & solve ─────────────────────────────────────────────────────

void checkDistanceHold(int distance) {
  bool inRange = (distance >= TARGET_MIN && distance <= TARGET_MAX);

  if (inRange) {
    if (!distanceInRange) {
      distanceInRange  = true;
      distanceHoldStart = millis();
    }
    if (millis() - distanceHoldStart >= DISTANCE_HOLD_TIME) {
      distanceSolved = true;
      playSuccessTone();
      for (int i = 1; i <= 10; i++) { setLedLevel(i); delay(50); }
    }
  } else {
    distanceInRange = false;
  }
}

// ── Update ─────────────────────────────────────────────────────────────────

void updateDistanceModule() {
  if (distanceSolved) return;

  unsigned long now = millis();
  if (now - distanceLastMeasure < DISTANCE_MEASURE_INTERVAL) return;
  distanceLastMeasure = now;

  int distance = ultrasonic.MeasureInCentimeters();
  if (distance <= 0 || distance > MAX_DISTANCE) distance = MAX_DISTANCE;

  if (frequencyModuleSolved) updateDistanceDisplay(distance);
  checkDistanceHold(distance);
}

#endif