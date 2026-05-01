/*
  Distance Control Module
  
  Uses an ultrasonic sensor to measure distance from an object.
  Player must hold the object at a specific distance range (10-15cm) for 3 seconds to solve.
*/

#ifndef DISTANCE_MODULE_H
#define DISTANCE_MODULE_H

#include <Ultrasonic.h>

// Forward declarations
extern Grove_LED_Bar ledBar;
extern void playSuccessTone();

// =====================================================
// DISTANCE CONTROL MODULE
// =====================================================

const int ULTRASONIC_PIN = 30;

const int TARGET_MIN = 10;
const int TARGET_MAX = 15;
const int MAX_DISTANCE = 30;
const unsigned long HOLD_TIME = 3000;

bool distanceSolved = false;
bool distanceInRange = false;
unsigned long distanceHoldStart = 0;

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

  int distance = ultrasonic.read();

  // Handle invalid readings
  if (distance <= 0 || distance > MAX_DISTANCE) {
    distance = MAX_DISTANCE;
  }

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  updateDistanceDisplay(distance);
  checkDistanceHold(distance);
}

#endif
