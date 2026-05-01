/*
  Distance Control Module
  
  Uses an ultrasonic sensor to measure distance from an object.
  Player must hold the object at a specific distance range (10-15cm) for 3 seconds to solve.
  Note: LED bar is shared with the frequency module, so distance module pauses LED updates
  while frequency module is active.
*/

#ifndef DISTANCE_MODULE_H
#define DISTANCE_MODULE_H

#include <Ultrasonic.h>

// Forward declarations
extern Grove_LED_Bar ledBar;
extern void playSuccessTone();
extern const int ULTRASONIC_PIN;
extern bool frequencyModuleSolved;  // Check if frequency module is done with LED bar

// =====================================================
// DISTANCE CONTROL MODULE
// =====================================================

const int TARGET_MIN = 10;
const int TARGET_MAX = 15;
const int MAX_DISTANCE = 30;
const unsigned long HOLD_TIME = 3000;

bool distanceSolved = false;
bool distanceInRange = false;
unsigned long distanceHoldStart = 0;
unsigned long distanceLastMeasure = 0;
const unsigned long DISTANCE_MEASURE_INTERVAL = 50;  // Only measure every 50ms instead of every loop

Ultrasonic ultrasonic(ULTRASONIC_PIN);

void setupDistanceModule() {
  // Ultrasonic sensor handles its own setup
  Serial.println("Distance module initialized");
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

    if (millis() - distanceHoldStart >= HOLD_TIME) {
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

  // Disable distance logging to prevent interference with frequency module
  // Serial.print("Distance: ");
  // Serial.print(distance);
  // Serial.println(" cm");

  // Only update LED bar display if frequency module isn't using it
  if (frequencyModuleSolved) {
    updateDistanceDisplay(distance);
  }
  
  checkDistanceHold(distance);
}

#endif
