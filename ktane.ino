/*
  Combined Bomb Modules Sketch

  Modules included:
  - Exponential buzzer countdown on D6
  - Frequency scrambler with LED bar and two encoders
  - 8x8 RGB LED matrix maze using the same two encoders
  - TM1637 countdown timer with Grove LED Button
  - Core overheating humidity/LCD module
  - Distance control with ultrasonic sensor on D30

  Pins
  - Enc left (d2-3)
  - Enc Right (d4-5)
  - Buzzer (d6-7)
  - 4 digit (d8-9)
  - red LED button (d10-11)
  - LED bar (d12-13)
  - Ultrasonic (d30)

  Important pin note:
  The original Encoder_Tuning.ino used DHT on D4, but D4/D5 are already used by the right encoder.
  This combined version moves the DHT sensor to D7.

  Libraries required:
  - Grove LED Bar
  - Encoder by Paul Stoffregen
  - DHT sensor library by Adafruit
  - rgb_lcd (Grove Serial RGB Backlight LCD)
  - TM1637
  - Grove two RGB LED Matrix
  - Ultrasonic
*/

#include <Wire.h>
#include <math.h>
#include <Grove_LED_Bar.h>
#include <Encoder.h>
#include <DHT.h>
#include <Ultrasonic.h>
#include "rgb_lcd.h"
#include "TM1637.h"
#include "grove_two_rgb_led_matrix.h"

// =====================================================
// MODULE SELECTION
// =====================================================
// Because the frequency module and maze both use the same two encoders,
// only one encoder-controlled module should be active at a time.

enum ActiveEncoderModule {
  FREQUENCY_MODULE,
  MAZE_MODULE
};

enum GameState {
  STATE_IDLE,           // Waiting for key to be turned on
  STATE_RUNNING,        // Game is running, key is on
  STATE_WON,            // All modules solved, waiting for key to be turned off
  STATE_FAILED,         // Game failed (key turned off before all modules solved)
  STATE_DISARMED        // Bomb successfully disarmed (key turned off after all modules solved)
};

// Include module headers (after enum definitions)
#include "FrequencyModule.h"
#include "CoreModule.h"
#include "MazeModule.h"
#include "DistanceModule.h"
#include "ButtonComboModule.h"

// =====================================================
// SHARED PIN MAP
// =====================================================

// Buzzer
const int BUZZER_PIN = 6;

// Encoders
const int ENC_LEFT_A = 2;
const int ENC_LEFT_B = 3;
const int ENC_RIGHT_A = 4;
const int ENC_RIGHT_B = 5;

// Temp & humid sensor
const int DHT_PIN = 7;     // moved from D4 to avoid right encoder conflict
const int DHT_TYPE = DHT11;

// 4 digit display
const int DISP_CLK = 8;
const int DISP_DIO = 9;

// red LED button
const int LED_PIN = 10;
const int BUTTON_PIN = 11;

// LED bar
const int LED_BAR_CLK = 13;
const int LED_BAR_DATA = 12;

// Keyed switch
const int KEY_SWITCH_PIN = 31;

Grove_LED_Bar ledBar(LED_BAR_CLK, LED_BAR_DATA, 0);
Encoder encLeft(ENC_LEFT_A, ENC_LEFT_B);
Encoder encRight(ENC_RIGHT_A, ENC_RIGHT_B);
DHT dht(DHT_PIN, DHT_TYPE);
rgb_lcd lcd;
TM1637 tm1637(DISP_CLK, DISP_DIO);
GroveTwoRGBLedMatrixClass matrix;

// =====================================================
// GAME STATE & CONFIGURATION
// =====================================================

ActiveEncoderModule activeEncoderModule = FREQUENCY_MODULE;
GameState currentGameState = STATE_IDLE;

// Key switch state tracking
bool keyLastState = false;
bool keyCurrentState = false;
unsigned long keyLastChangeTime = 0;
const unsigned long keyDebounceDelay = 50;

// =====================================================
// SHARED GAME TIMER STATE
// =====================================================

const int START_MINUTES = 3;
const int START_SECONDS = 0;
const int TIMER_TOTAL_SECONDS = START_MINUTES * 60 + START_SECONDS;
const unsigned long TIMER_TOTAL_DURATION = (unsigned long)TIMER_TOTAL_SECONDS * 1000UL;

int remainingSeconds = TIMER_TOTAL_SECONDS;
bool timerRunning = true;
bool timerFinished = false;
unsigned long timerLastTick = 0;

// =====================================================
// BUZZER MODULE
// =====================================================

unsigned long buzzerLastBeepTime = 0;

const unsigned long buzzerStartInterval = 5000;
const unsigned long buzzerEndInterval = 90;
const unsigned long buzzerBeepDuration = 80;

const int buzzerStartFreq = 800;
const int buzzerEndFreq = 2000;
const float buzzerIntervalCurve = 3.5;
const float buzzerPitchCurve = 2.8;

void setupBuzzerModule() {
  pinMode(BUZZER_PIN, OUTPUT);
}

float getTimerProgress() {
  unsigned long now = millis();
  long msSinceLastTick = (long)(now - timerLastTick);

  if (msSinceLastTick < 0) {
    msSinceLastTick = 0;
  }

  long remainingMillis = ((long)remainingSeconds * 1000L) - msSinceLastTick;

  if (remainingMillis < 0) {
    remainingMillis = 0;
  }

  if (remainingMillis > (long)TIMER_TOTAL_DURATION) {
    remainingMillis = TIMER_TOTAL_DURATION;
  }

  float progress = 1.0 - ((float)remainingMillis / (float)TIMER_TOTAL_DURATION);

  if (progress < 0.0) {
    progress = 0.0;
  }

  if (progress > 1.0) {
    progress = 1.0;
  }

  return progress;
}

void updateBuzzerModule() {
  if (!timerRunning || timerFinished || remainingSeconds <= 0) {
    noTone(BUZZER_PIN);
    return;
  }

  unsigned long now = millis();
  float t = getTimerProgress();

  float intervalProgress = pow(t, buzzerIntervalCurve);
  float pitchProgress = pow(t, buzzerPitchCurve);

  unsigned long currentInterval =
    buzzerStartInterval - (unsigned long)((buzzerStartInterval - buzzerEndInterval) * intervalProgress);

  int currentFreq =
    buzzerStartFreq + (int)((buzzerEndFreq - buzzerStartFreq) * pitchProgress);

  if (now - buzzerLastBeepTime >= currentInterval) {
    tone(BUZZER_PIN, currentFreq, buzzerBeepDuration);
    buzzerLastBeepTime = now;
  }
}

void stopBuzzer() {
  noTone(BUZZER_PIN);
}

void playSuccessTone() {
  // Play a success fanfare: ascending tones
  int fanfarePattern[] = {523, 659, 784, 1047, 784, 659, 523};
  
  for (int i = 0; i < 7; i++) {
    tone(BUZZER_PIN, fanfarePattern[i], 150);
    delay(200);
  }
  noTone(BUZZER_PIN);
}

// =====================================================
// TIMER BUTTON MODULE
// =====================================================

bool lastButtonReading = HIGH;
bool debouncedButtonState = HIGH;
unsigned long timerLastDebounceTime = 0;
const unsigned long timerDebounceDelay = 30;

int currentDigits[4] = {0, 2, 0, 0};

// =====================================================
// KEY SWITCH MODULE
// =====================================================

bool readKeySwitchDebounced() {
  bool reading = digitalRead(KEY_SWITCH_PIN);
  unsigned long now = millis();

  if (reading != keyLastState) {
    keyLastChangeTime = now;
  }

  if ((now - keyLastChangeTime) >= keyDebounceDelay) {
    if (reading != keyCurrentState) {
      keyCurrentState = reading;
      return true; // State changed
    }
  }

  keyLastState = reading;
  return false; // State unchanged
}

bool allModulesSolved() {
  return frequencyModuleSolved && mazeSolved && coreSolved && distanceSolved && buttonComboSolved;
}

void displayInitialScreen() {
  // Display "8888" on 7-segment display
  int8_t displayDigits[4] = {8, 8, 8, 8};
  tm1637.point(POINT_ON);
  tm1637.display(displayDigits);
}

void displayBombDisarmed() {
  // Clear the timer display and show "Bomb Disarmed" on the LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BOMB DISARMED");
  lcd.setCursor(0, 1);
  lcd.print("SUCCESS!");
  
  // Also try to clear the 7-segment display
  int8_t blank[4] = {-1, -1, -1, -1};
  tm1637.display(blank);
}

void displayGameOverFailed() {
  // Display game over message on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("GAME OVER!");
  lcd.setCursor(0, 1);
  lcd.print("KEY TURNED OFF");
}

bool lastDisplayedGameState = -1;

void updateDisplayState() {
  // Update display based on current game state
  // Only update when state changes to avoid flickering
  if (currentGameState != lastDisplayedGameState) {
    if (currentGameState == STATE_FAILED) {
      displayGameOverFailed();
    } else if (currentGameState == STATE_DISARMED) {
      displayBombDisarmed();
    }
    lastDisplayedGameState = currentGameState;
  }
}

void handleKeySwitch() {
  if (!readKeySwitchDebounced()) {
    return; // No state change
  }

  // Key state has changed
  if (keyCurrentState == HIGH) {
    // Key is ON
    if (currentGameState == STATE_IDLE) {
      Serial.println("Key turned ON - Starting game");
      currentGameState = STATE_RUNNING;
      timerRunning = true;
      timerFinished = false;
      timerLastTick = millis();
      remainingSeconds = TIMER_TOTAL_SECONDS;
      updateTimerDisplay();
    }
  } else {
    // Key is OFF
    if (currentGameState == STATE_RUNNING) {
      Serial.println("Key turned OFF while running - Game FAILED");
      currentGameState = STATE_FAILED;
      timerRunning = false;
      stopBuzzer();
      // LCD display will be updated by updateDisplayState()
    } else if (currentGameState == STATE_WON) {
      Serial.println("Key turned OFF after modules solved - Bomb DISARMED");
      currentGameState = STATE_DISARMED;
      timerRunning = false;
      stopBuzzer();
      // LCD display will be updated by updateDisplayState()
    }
  }
}

void updateGameState() {
  // Check if we should transition from RUNNING to WON
  if (currentGameState == STATE_RUNNING && allModulesSolved()) {
    Serial.println("All modules solved - waiting for key OFF to disarm");
    currentGameState = STATE_WON;
    // Don't stop timer yet - it continues until key is turned off
  }
}

void updateTimerDisplay() {
  int minutes = remainingSeconds / 60;
  int seconds = remainingSeconds % 60;

  currentDigits[0] = minutes / 10;
  currentDigits[1] = minutes % 10;
  currentDigits[2] = seconds / 10;
  currentDigits[3] = seconds % 10;

  int8_t displayDigits[4];
  displayDigits[0] = currentDigits[0];
  displayDigits[1] = currentDigits[1];
  displayDigits[2] = currentDigits[2];
  displayDigits[3] = currentDigits[3];

  tm1637.point(POINT_ON);
  tm1637.display(displayDigits);
}

void setupTimerModule() {
  tm1637.init();
  tm1637.set(BRIGHT_TYPICAL);
  tm1637.point(POINT_ON);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  digitalWrite(LED_PIN, LOW);

  // Display initial "8888" screen
  displayInitialScreen();
  
  timerLastTick = millis();
}

void updateCountdown() {
  if (!timerRunning || timerFinished) {
    return;
  }

  unsigned long now = millis();

  if (now - timerLastTick >= 1000) {
    timerLastTick += 1000;

    if (remainingSeconds > 0) {
      remainingSeconds--;
      if (currentGameState == STATE_RUNNING || currentGameState == STATE_WON) {
        updateTimerDisplay();
      }
    }

    if (remainingSeconds <= 0) {
      remainingSeconds = 0;
      timerRunning = false;
      timerFinished = true;
      stopBuzzer();
      if (currentGameState == STATE_RUNNING || currentGameState == STATE_WON) {
        updateTimerDisplay();
      }
    }
  }
}

bool buttonJustPressed() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    timerLastDebounceTime = millis();
  }

  if ((millis() - timerLastDebounceTime) > timerDebounceDelay) {
    if (reading != debouncedButtonState) {
      debouncedButtonState = reading;

      if (debouncedButtonState == LOW) {
        lastButtonReading = reading;
        return true;
      }
    }
  }

  lastButtonReading = reading;
  return false;
}

bool digitIsVisible(char targetDigit) {
  if (targetDigit < '0' || targetDigit > '9') {
    return false;
  }

  int target = targetDigit - '0';

  for (int i = 0; i < 4; i++) {
    if (currentDigits[i] == target) {
      return true;
    }
  }

  return false;
}

bool activate(const char* target) {
  if (target == nullptr || target[0] == '\0' || target[1] != '\0') {
    return false;
  }

  if (!buttonJustPressed()) {
    return false;
  }

  bool correct = digitIsVisible(target[0]);

  if (correct) {
    timerRunning = false;
    noTone(BUZZER_PIN);
    digitalWrite(LED_PIN, HIGH);
    Serial.println("Timer button solved!");
    return true;
  }

  digitalWrite(LED_PIN, LOW);
  return false;
}

void updateTimerModule() {
  updateCountdown();

  if (activate("1")) {
    // Correct button press while a 1 is visible.
  }
}

// =====================================================
// MAIN SETUP / LOOP
// =====================================================

void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(1000);

  // Initialize key switch
  pinMode(KEY_SWITCH_PIN, INPUT);
  keyCurrentState = digitalRead(KEY_SWITCH_PIN);
  keyLastState = keyCurrentState;

  setupBuzzerModule();
  setupFrequencyModule();
  setupCoreModule();
  setupDistanceModule();
  setupButtonComboModule();
  setupTimerModule();

  if (activeEncoderModule == MAZE_MODULE) {
    setupMazeModule();
  }

  Serial.println("Combined system ready");
  Serial.println("Waiting for key switch to turn ON...");
  lcd.print("Starting!");
}

void loop() {
  // Check key switch state and handle state transitions
  handleKeySwitch();
  updateGameState();
  
  // Always update display state regardless of game state
  updateDisplayState();

  // Only update modules if game is running or won
  if (currentGameState == STATE_RUNNING || currentGameState == STATE_WON) {
    updateTimerModule();
    updateBuzzerModule();
    updateCoreModule();
    updateDistanceModule();
    updateButtonComboModule();

    if (activeEncoderModule == FREQUENCY_MODULE) {
      updateFrequencyModule();
    } else if (activeEncoderModule == MAZE_MODULE) {
      updateMazeModule();
    }
  }
}
