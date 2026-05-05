/*
  Distance Control Module
  
  Uses an ultrasonic sensor to measure distance from an object.
  Required distance and hold time are derived from serial number:
  - Distance: 2nd-to-last digit × 10 cm (0-90cm range, minimum 10cm enforced)
  - Hold time: Last digit × 0.5 seconds (0-4.5 seconds)
  
  Player must hold the object at the required distance for the required hold time to solve.
  
  Note: LED bar is shared with the frequency module, so distance module pauses LED updates
  while frequency module is active.
*/

#ifndef DISTANCE_MODULE_H
#define DISTANCE_MODULE_H

#include <Ultrasonic.h>
#include "SerialNumberParser.h"

// Forward declarations
extern Grove_LED_Bar ledBar;
extern void playSuccessTone();
extern const int ULTRASONIC_PIN;
extern bool frequencyModuleSolved;  // Check if frequency module is done with LED bar

// =====================================================
// DISTANCE CONTROL MODULE
// =====================================================

// These will be calculated from serial number
int TARGET_MIN = 10;
int TARGET_MAX = 15;
unsigned long DISTANCE_HOLD_TIME = 3000;

const int MAX_DISTANCE = 100;  // Increased to accommodate all possible target distances (up to 90cm)

bool distanceSolved = false;
bool distanceInRange = false;
unsigned long distanceHoldStart = 0;
unsigned long distanceLastMeasure = 0;
const unsigned long DISTANCE_MEASURE_INTERVAL = 50;  // Only measure every 50ms instead of every loop

Ultrasonic ultrasonic(ULTRASONIC_PIN);

void setupDistanceModule() {
  // Derive distance requirements from serial number
  int secondToLastDigit = getSecondToLastDigit();
  int lastDigit = getLastDigit();
  
  // Distance: 2nd-to-last digit × 10
  int requiredDistance = secondToLastDigit * 10;
  
  // Enforce minimum of 10cm (ultrasonic has limited accuracy below this)
  if (requiredDistance < 10) {
    requiredDistance = 10;
  }
  
  // Cap at 90cm (reasonable maximum)
  if (requiredDistance > 90) {
    requiredDistance = 90;
  }
  
  // Set range: ±5cm tolerance around required distance (less sensitive)
  TARGET_MIN = requiredDistance - 5;
  TARGET_MAX = requiredDistance + 5;
  
  // Hold time: last digit × 0.5 seconds, minimum 0.5 seconds
  int holdTimeMs = lastDigit * 500;  // 0.5 seconds per digit
  if (holdTimeMs < 500) {
    holdTimeMs = 500;  // Minimum 0.5 seconds
  }
  
  DISTANCE_HOLD_TIME = (unsigned long)holdTimeMs;
  
  // Ultrasonic sensor handles its own setup
  Serial.print("Distance module: Target ");
  Serial.print(requiredDistance);
  Serial.print("cm (");
  Serial.print(TARGET_MIN);
  Serial.print("-");
  Serial.print(TARGET_MAX);
  Serial.print("cm), Hold ");
  Serial.print(DISTANCE_HOLD_TIME / 1000.0);
  Serial.println("s");
}

void updateDistanceDisplay(int distance) {
  int level;

  if (distance < TARGET_MIN) {
    level = map(distance, 0, TARGET_MIN, 0, 4);
  }
  else if (distance > TARGET_MAX) {
    level = map(distance, TARGET_MAX, MAX_DISTANCE, 6, 10);
  }
  else {
    level = 5;
  }

  ledBar.setLevel(level);
}

void checkDistanceHold(int distance) {
  bool nowInRange = (distance >= TARGET_MIN && distance <= TARGET_MAX);

  if (nowInRange) {
    if (!distanceInRange) {
      distanceInRange = true;
      distanceHoldStart = millis();
      Serial.println("Distance: Hold started");
    }

    if (millis() - distanceHoldStart >= DISTANCE_HOLD_TIME) {
      distanceSolved = true;
      Serial.println("Distance module solved!");

      // Play success tone and flash LED bar
      playSuccessTone();

      for (int i = 1; i <= 10; i++) {
        ledBar.setLevel(i);
        delay(50);
      }
    }
  } else {
    distanceInRange = false;
  }
}

void updateDistanceModule() {
  if (distanceSolved) {
    return;
  }

  // Only measure distance every 50ms to avoid blocking encoder interrupts
  unsigned long now = millis();
  if (now - distanceLastMeasure < DISTANCE_MEASURE_INTERVAL) {
    return;
  }
  distanceLastMeasure = now;

  // Grove Ultrasonic returns distance in cm directly
  int distance = ultrasonic.MeasureInCentimeters();

  // Handle invalid readings
  if (distance <= 0 || distance > MAX_DISTANCE) {
    distance = MAX_DISTANCE;
  }

  // Debug: Log distance readings to help with calibration
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print("cm (target: ");
  Serial.print(TARGET_MIN);
  Serial.print("-");
  Serial.print(TARGET_MAX);
  Serial.println("cm)");

  // Only update LED bar display if frequency module isn't using it
  if (frequencyModuleSolved) {
    updateDistanceDisplay(distance);
  }
  
  checkDistanceHold(distance);
}

#endif
