#include <Ultrasonic.h>
#include <Grove_LED_Bar.h>

// -------- PINS --------
#define ULTRASONIC_PIN 9
#define LED_BAR_CLK 2
#define LED_BAR_DATA 3
#define BUZZER 13

// -------- SETTINGS --------
const int TARGET_MIN = 10;
const int TARGET_MAX = 15;
const int MAX_DISTANCE = 30;
const unsigned long HOLD_TIME = 3000;

// -------- OBJECTS --------
Ultrasonic ultrasonic(ULTRASONIC_PIN);
Grove_LED_Bar ledBar(LED_BAR_CLK, LED_BAR_DATA, true);

// -------- VARIABLES --------
bool inRange = false;
unsigned long holdStart = 0;
bool complete = false;

// -------- SETUP --------
void setup() {
  Serial.begin(9600);

  ledBar.begin();
  ledBar.setLevel(0);

  pinMode(BUZZER, OUTPUT);

  Serial.println("Distance Module Started");
}

// -------- LOOP --------
void loop() {
  if (complete) return;

  int distance = ultrasonic.read();

  if (distance <= 0 || distance > MAX_DISTANCE)
    distance = MAX_DISTANCE;

  Serial.println(distance);

  updateLED(distance);
  checkHold(distance);

  delay(100);
}

// -------- LED LOGIC --------
void updateLED(int d) {
  int level;

  if (d < TARGET_MIN) {
    level = map(d, 0, TARGET_MIN, 0, 4);
  }
  else if (d > TARGET_MAX) {
    level = map(d, TARGET_MAX, MAX_DISTANCE, 6, 10);
  }
  else {
    level = 5;
  }

  ledBar.setLevel(level);
}

// -------- HOLD CHECK --------
void checkHold(int d) {

  bool nowInRange = (d >= TARGET_MIN && d <= TARGET_MAX);

  if (nowInRange) {

    if (!inRange) {
      inRange = true;
      holdStart = millis();
      Serial.println("Hold started");
    }

    if (millis() - holdStart >= HOLD_TIME) {
      complete = true;
      Serial.println("DISTANCE COMPLETE");

      successTone();

      for (int i = 1; i <= 10; i++) {
        ledBar.setLevel(i);
        delay(50);
      }
    }

  } else {
    inRange = false;
  }
}

// -------- SOUND --------
void successTone() {
  tone(BUZZER, 800, 120); delay(140);
  tone(BUZZER, 1000, 120); delay(140);
  tone(BUZZER, 1400, 200); delay(220);
}