/*
  KTANE Bomb Game - Main Controller
  
  A collaborative bomb-defusal game for Arduino Mega 2560.
  Players must solve multiple electronic puzzles under time pressure.
  
  ACTIVE MODULES:
  - Frequency Scrambler: Encoder sequences (2 rotary encoders)
  - Maze Navigator: LED matrix pathfinding (8x8 WS2812B display)
  - Button Combo: Timed button sequences (Red/Green buttons)
  - Core Overheating: Microphone-triggered event (random activation)
  - Timer: 5-minute countdown with exponential beeping
  
  HARDWARE PINS:
  
  Input/Control:
    D2-D3   - Left encoder (CLK/DT phases)
    D4-D5   - Right encoder (CLK/DT phases)
    D10     - Red button
    D11     - Green button
    D31     - Key switch (game on/off)
    D7      - Microphone sensor (Core module)
  
  Output/Display:
    D6      - Buzzer (game audio)
    D8-D9   - 4-Digit display (TM1637 DIO/CLK)
    D22     - 8x8 LED matrix (NeoPixel data)
    D35,37,39,41,43,45,47,49,51,53 - LED bar (10 LEDs)
  
  Communication:
    D20-D21 - I2C bus (LCD 16x2 display)
  
  LIBRARIES REQUIRED:
  - Encoder by Paul Stoffregen
  - LiquidCrystal_I2C
  - DIYables_4Digit7Segment_TM1637
  - Adafruit_NeoPixel
*/

#include <Wire.h>
#include <math.h>
#include <Encoder.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>
#include <DIYables_4Digit7Segment_TM1637.h>

// Utility modules
#include "SerialNumberGenerator.h"
#include "SerialNumberParser.h"

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
#include "ButtonComboModule.h"

// Game modules
#include "signalAlignment.h"

// =====================================================
// DEBUG / TESTING - BYPASS FLAGS
// =====================================================
// Set these to true to bypass modules for testing
const bool BYPASS_FREQUENCY_MODULE = true;
const bool BYPASS_MAZE_MODULE = false;
const bool BYPASS_CORE_MODULE = true;
const bool BYPASS_BUTTON_COMBO_MODULE = false;
const bool BYPASS_TIMER_MODULE = false;
const bool BYPASS_SIGNAL_ALIGNMENT = true;

// When true, auto-starts game on boot without requiring key turn
const bool AUTO_START_GAME = true;

// =====================================================
// SHARED PIN MAP - Arduino Mega
// =====================================================
// Core Control Pins
const int BUZZER_PIN = 6;                // D6 - Game audio feedback

// Encoder Inputs (for Frequency/Maze modules)
// IMPORTANT: Rotary encoders need BOTH CLK and DT phases connected!
// Left encoder: CLK→D2, DT→D3, SW→GND (SW is optional pushbutton)
// Right encoder: CLK→D4, DT→D5, SW→GND (SW is optional pushbutton)
const int ENC_LEFT_A = 2;                // D2 - Left encoder CLK (Phase A)
const int ENC_LEFT_B = 3;                // D3 - Left encoder DT (Phase B)
const int ENC_RIGHT_A = 4;               // D4 - Right encoder CLK (Phase A)
const int ENC_RIGHT_B = 5;               // D5 - Right encoder DT (Phase B)

// 4-Digit 7-Segment Display (DIYables TM1637)
const int DISP_CLK = 9;                  // D9 - Display clock
const int DISP_DIO = 8;                  // D8 - Display data

// Key Switch (Game On/Off)
const int KEY_SWITCH_PIN = 31;           // D31 - Ignition key switch

// 8x8 LED Matrix (WS2812B NeoPixel)
const int NEOPIXEL_PIN = 22;             // D22 - NeoPixel data
const int NUM_PIXELS = 64;               // 8x8 matrix = 64 pixels
const int NEOPIXEL_BRIGHTNESS = 10;      // 10% brightness (25/255) to protect power supply and add buffer for other components

// Generic LED Bar (10 individual LEDs on odd pins)
const int LED_BAR_PINS[10] = {35, 37, 39, 41, 43, 45, 47, 49, 51, 53};

// I2C Bus (shared) - D20 (SDA), D21 (SCL) for LCD display

// Ending sequence
bool endSequencePlayed = false;

// Generic LED bar control - no global object needed
Encoder encLeft(ENC_LEFT_A, ENC_LEFT_B);
Encoder encRight(ENC_RIGHT_A, ENC_RIGHT_B);
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address 0x27, 16 columns, 2 rows
DIYables_4Digit7Segment_TM1637 tm1637(DISP_CLK, DISP_DIO);
Adafruit_NeoPixel matrix(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// =====================================================
// GAME STATE & CONFIGURATION
// =====================================================

ActiveEncoderModule activeEncoderModule = FREQUENCY_MODULE;
GameState currentGameState = STATE_IDLE;
bool signalAlignmentModuleActive = false;  // Flag for signal alignment module

// Serial number for the bomb
String bombSerialNumber = "";

// Key switch state tracking
bool keyLastState = false;
bool keyCurrentState = false;
unsigned long keyLastChangeTime = 0;
const unsigned long keyDebounceDelay = 50;

// =====================================================
// SHARED GAME TIMER STATE
// =====================================================

const int START_MINUTES = 5;
const int START_SECONDS = 0;
const int TIMER_TOTAL_SECONDS = START_MINUTES * 60 + START_SECONDS;
const unsigned long TIMER_TOTAL_DURATION = (unsigned long)TIMER_TOTAL_SECONDS * 1000UL;

int remainingSeconds = TIMER_TOTAL_SECONDS;
bool timerRunning = false;
bool timerFinished = false;
bool timerModuleSolved = false;  // Track if timer module has been solved
unsigned long timerLastTick = 0;

// =====================================================
// CORE MODULE RANDOMIZATION
// =====================================================
// Module transition tracking lives inline in this sketch

// =====================================================
// TIMER STATE & COUNTDOWN
// =====================================================

bool lastButtonReading = HIGH;
bool debouncedButtonState = HIGH;
unsigned long timerLastDebounceTime = 0;
const unsigned long timerDebounceDelay = 30;

int currentDigits[4] = {0, 2, 0, 0};
int lastDisplayedGameState = -1;
bool serialNumberDisplayed = false;

unsigned long buzzerLastBeepTime = 0;
const unsigned long buzzerStartInterval = 5000;
const unsigned long buzzerEndInterval = 90;
const unsigned long buzzerBeepDuration = 80;
const int buzzerStartFreq = 800;
const int buzzerEndFreq = 2000;
const float buzzerIntervalCurve = 3.5;
const float buzzerPitchCurve = 2.8;

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
    bool solved = true;
    if (!BYPASS_FREQUENCY_MODULE && !frequencyModuleSolved) solved = false;
    if (!BYPASS_MAZE_MODULE && !mazeSolved) solved = false;
    if (!BYPASS_CORE_MODULE && !coreSolved) solved = false;
    if (!BYPASS_BUTTON_COMBO_MODULE && !buttonComboSolved) solved = false;
    if (!BYPASS_TIMER_MODULE && !timerModuleSolved) solved = false;
    if (!BYPASS_SIGNAL_ALIGNMENT && !signalAlignmentSolved) solved = false;
    return solved;
}

// =====================================================
// MODULE TRANSITION TRACKING
// =====================================================

bool lastFrequencyModuleSolved = false;
bool lastMazeSolved = false;
bool lastButtonComboSolved = false;
bool lastTimerModuleSolved = false;

void checkModuleTransitionsAndTriggerCore() {
  if (coreTriggered || coreSolved) {
    return;
  }

  bool frequencyJustCompleted = !lastFrequencyModuleSolved && frequencyModuleSolved;
  bool mazeJustCompleted = !lastMazeSolved && mazeSolved;
  bool buttonComboJustCompleted = !lastButtonComboSolved && buttonComboSolved;
  bool timerJustCompleted = !lastTimerModuleSolved && timerModuleSolved;

  if (frequencyJustCompleted || mazeJustCompleted || buttonComboJustCompleted || timerJustCompleted) {
    if (random(100) < 50) {
      triggerCoreEvent();
    }
  }

  lastFrequencyModuleSolved = frequencyModuleSolved;
  lastMazeSolved = mazeSolved;
  lastButtonComboSolved = buttonComboSolved;
  lastTimerModuleSolved = timerModuleSolved;
}

// =====================================================
// GENERIC LED BAR CONTROL
// =====================================================

void initializeLedBar() {
  for (int i = 0; i < 10; i++) {
    pinMode(LED_BAR_PINS[i], OUTPUT);
    digitalWrite(LED_BAR_PINS[i], LOW);
  }
}

void setLedLevel(int level) {
  level = constrain(level, 0, 10);

  for (int i = 0; i < 10; i++) {
    if (i < level) {
      digitalWrite(LED_BAR_PINS[i], HIGH);
    } else {
      digitalWrite(LED_BAR_PINS[i], LOW);
    }
  }
}

void playLEDBootSequence() {
  for (int level = 0; level <= 10; level++) {
    setLedLevel(level);
    delay(50);
  }

  for (int level = 10; level >= 0; level--) {
    setLedLevel(level);
    delay(50);
  }
}

// Display and end sequence handling

void handleKeySwitch() {
  if (!readKeySwitchDebounced()) {
    return; // No state change
  }

  // Key state has changed
  if (keyCurrentState == HIGH) {
    // Key is ON
    if (currentGameState == STATE_IDLE) {
      currentGameState = STATE_RUNNING;
      timerRunning = true;
      timerFinished = false;
      timerLastTick = millis();
      remainingSeconds = TIMER_TOTAL_SECONDS;
      updateTimerDisplay();
      
      // Reset maze module state for new game
      extern bool mazeSetupDone;
      extern bool mazeDisplayCleared;
      mazeSetupDone = false;
      mazeDisplayCleared = false;
      
      // Reset actual module solved states for new game
      frequencyModuleSolved = false;
      mazeSolved = false;
      coreTriggered = false;
      coreSolved = false;
      buttonComboSolved = false;
      timerModuleSolved = false;
      signalAlignmentSolved = false;
      endSequencePlayed = false;
    }
  } else {
    // Key is OFF
    if (currentGameState == STATE_RUNNING) {
      currentGameState = STATE_FAILED;
      timerRunning = false;
      stopBuzzer();
      // Play detonated sequence once
      if (!endSequencePlayed) {
        playDetonatedSequence(timerFinished);
        endSequencePlayed = true;
      }
      // LCD display will be updated by updateDisplayState()
    } else if (currentGameState == STATE_WON) {
      // Force trigger core if it hasn't been triggered yet before allowing disarm
      if (!coreTriggered && !coreSolved) {
        triggerCoreEvent();
        // Give core event a moment to activate
        delay(100);
      }
      
      currentGameState = STATE_DISARMED;
      timerRunning = false;
      stopBuzzer();
      // Play disarmed sequence once
      if (!endSequencePlayed) {
        playDisarmedSequence();
        endSequencePlayed = true;
      }
      // LCD display will be updated by updateDisplayState()
    }
  }
}

void updateGameState() {
  // Check if we should transition from RUNNING to WON
  if (currentGameState == STATE_RUNNING && allModulesSolved()) {
    // Force trigger core if it hasn't been triggered yet
    if (!coreTriggered && !coreSolved) {
      triggerCoreEvent();
    }
    
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

  // Display MM:SS format using DIYables printTime API
  // printTime takes: hours (displayed as tens digit), minutes (displayed as ones digit), colonOn
  // We repurpose this to show MM:SS by passing minutes and seconds
  tm1637.printTime(minutes, seconds, true);  // Displays as MM:SS with colon
}

void setupTimerModule() {
  // Initialize 4-digit 7-segment display (DIYables TM1637)
  tm1637.begin();

  // Display initial "88:88" screen
  displayInitialScreen();
  
  timerLastTick = millis();
}

int getTotalModuleCount() {
  int count = 0;
  if (!BYPASS_FREQUENCY_MODULE) count++;
  if (!BYPASS_MAZE_MODULE) count++;
  if (!BYPASS_CORE_MODULE) count++;
  if (!BYPASS_BUTTON_COMBO_MODULE) count++;
  if (!BYPASS_TIMER_MODULE) count++;
  if (!BYPASS_SIGNAL_ALIGNMENT) count++;
  return count;
}

int getSolvedModuleCount() {
  int count = 0;
  if (!BYPASS_FREQUENCY_MODULE && frequencyModuleSolved) count++;
  if (!BYPASS_MAZE_MODULE && mazeSolved) count++;
  if (!BYPASS_CORE_MODULE && coreSolved) count++;
  if (!BYPASS_BUTTON_COMBO_MODULE && buttonComboSolved) count++;
  if (!BYPASS_TIMER_MODULE && timerModuleSolved) count++;
  if (!BYPASS_SIGNAL_ALIGNMENT && signalAlignmentSolved) count++;
  return count;
}

void displayInitialScreen() {
  tm1637.printTime(88, 88, true);
}

void displayIdleScreen() {
  // Display "Bomb Armed" on LCD while waiting for key to be turned on
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BOMB ARMED");
  lcd.setCursor(0, 1);
  lcd.print("TURN KEY");
}

void updateCountdown() {
  // Always keep the timer display updated while in gameplay states
  if (currentGameState == STATE_RUNNING || currentGameState == STATE_WON || currentGameState == STATE_FAILED) {
    updateTimerDisplay();
  }
  
  // Only run the countdown if timer is actively running
  if (!timerRunning || timerFinished) {
    return;
  }

  unsigned long now = millis();

  if (now - timerLastTick >= 1000) {
    timerLastTick += 1000;

    if (remainingSeconds > 0) {
      remainingSeconds--;
    }

    if (remainingSeconds <= 0) {
      remainingSeconds = 0;
      timerRunning = false;
      timerFinished = true;
      stopBuzzer();
      
      // Change game state to FAILED regardless of current state
      if (currentGameState == STATE_RUNNING || currentGameState == STATE_WON) {
        currentGameState = STATE_FAILED;
      }
    }
  }
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

void updateTimerModule() {
  updateCountdown();
}

void updateModuleCountDisplay() {
  int solvedCount = getSolvedModuleCount();
  int totalCount = getTotalModuleCount();

  String countStr = String(solvedCount) + "/" + String(totalCount);
  int startCol = 16 - countStr.length();
  lcd.setCursor(startCol, 1);
  lcd.print(countStr);
}

void displaySerialNumber() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SN:");
  lcd.print(bombSerialNumber);

  updateModuleCountDisplay();
}

void updateDisplayState() {
  if (currentGameState != lastDisplayedGameState) {
    if (currentGameState == STATE_IDLE) {
      displayIdleScreen();
    }
    lastDisplayedGameState = currentGameState;
    serialNumberDisplayed = false;
  }

  if (!serialNumberDisplayed && (currentGameState == STATE_RUNNING || currentGameState == STATE_WON)) {
    if (!coreTriggered || coreSolved) {
      displaySerialNumber();
      serialNumberDisplayed = true;
    }
  }

  if ((currentGameState == STATE_RUNNING || currentGameState == STATE_WON) && serialNumberDisplayed) {
    if (!coreTriggered || coreSolved) {
      updateModuleCountDisplay();
    }
  }
}

void setupBuzzerModule() {
  pinMode(BUZZER_PIN, OUTPUT);
}

void stopBuzzer() {
  noTone(BUZZER_PIN);
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

void updateCoreEmergencyBeeping() {
  if (!coreMessageShown || coreSolved) {
    return;
  }

  unsigned long now = millis();
  unsigned long cycleTime = now % 1000;

  if (cycleTime < 200) {
    tone(BUZZER_PIN, 1500, 200);
  } else if (cycleTime >= 250 && cycleTime < 450) {
    tone(BUZZER_PIN, 1500, 200);
  } else if (cycleTime >= 500 && cycleTime < 700) {
    tone(BUZZER_PIN, 1500, 200);
  } else {
    noTone(BUZZER_PIN);
  }
}

void updateBuzzerModule() {
  if (coreMessageShown && !coreSolved) {
    updateCoreEmergencyBeeping();
    return;
  }

  if (!timerRunning || timerFinished || remainingSeconds <= 0) {
    noTone(BUZZER_PIN);
    return;
  }

  unsigned long now = millis();

  if (remainingSeconds <= 15) {
    unsigned long panicInterval = 100;
    int panicFreq = 1000 + (15 - remainingSeconds) * 50;

    if (now - buzzerLastBeepTime >= panicInterval) {
      tone(BUZZER_PIN, panicFreq, 50);
      buzzerLastBeepTime = now;
    }
  } else {
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
}

void applyPenalty(const char* reason) {
  tone(BUZZER_PIN, 1000, 200);

  if (remainingSeconds > 3) {
    remainingSeconds -= 3;
  } else {
    remainingSeconds = 0;
    timerRunning = false;
    timerFinished = true;
    stopBuzzer();
  }
}

void playSuccessTone() {
  int fanfarePattern[] = {523, 659, 784, 1047, 784, 659, 523};

  for (int i = 0; i < 7; i++) {
    tone(BUZZER_PIN, fanfarePattern[i], 150);
    delay(200);
  }
  noTone(BUZZER_PIN);
}

// =====================================================
// MATRIX ROTATION HELPER
// =====================================================
// Rotates an 8-frame uint64_t pattern 90 degrees CCW.
// Each uint64_t encodes one row: byte 7 = col 0, byte 0 = col 7.

void rotateFrames90CCW(uint64_t* src, uint64_t* dst) {
  uint8_t grid[8][8];
  uint8_t rotated[8][8];

  // Unpack src into grid[row][col]
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      grid[row][col] = (src[row] >> ((7 - col) * 8)) & 0xFF;
    }
  }

  // 90° CCW: rotated[row][col] = grid[col][7 - row]
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      rotated[row][col] = grid[col][7 - row];
    }
  }

  // Repack into dst
  for (int row = 0; row < 8; row++) {
    dst[row] = 0;
    for (int col = 0; col < 8; col++) {
      dst[row] |= ((uint64_t)rotated[row][col]) << ((7 - col) * 8);
    }
  }
}

// =====================================================
// MATRIX GRAPHICS (original emoji pixel data)
// =====================================================

// Smile emoji (index 0) raw pixel data, copied from Grove library source
uint64_t smileRaw[] = {
  0xffff5e5e5e5effff,
  0xff5effffffff5eff,
  0x5eff5effff5eff5e,
  0x5effffffffffff5e,
  0x5eff5effff5eff5e,
  0x5effff5e5effff5e,
  0xff5effffffff5eff,
  0xffff5e5e5e5effff
};

// Flame emoji (index 14) raw pixel data
uint64_t flameRaw[] = {
  0xffffff29ffffffff,
  0xffff2929ffffffff,
  0xff292929ff29ffff,
  0xff292929292929ff,
  0x2929292929292929,
  0x2929292929292929,
  0xff29292929292929,  // corrected from library
  0xffff292929ffffff
};

// =====================================================
// WIN / LOSE SEQUENCES
// =====================================================

// --- WIN: Disarmed sequence ---
// Uses: LCD, LED bar, buzzer, 8x8 matrix
// Total duration: ~3 seconds, then settles

void playDisarmedSequence() {
  // Step 1 (0ms): LCD message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("** DISARMED **");
  lcd.setCursor(0, 1);
  // Show time remaining
  int mins = remainingSeconds / 60;
  int secs = remainingSeconds % 60;
  lcd.print(mins);
  lcd.print(":");
  if (secs < 10) lcd.print("0");
  lcd.print(secs);
  lcd.print(" remaining");

  // Step 2 (100ms): Fill LED bar left to right
  delay(100);
  for (int i = 1; i <= 10; i++) {
    setLedLevel(i);
    delay(100);
  }

  // Step 3 (1.1s): Ascending fanfare
  // C5, E5, G5, C6, E6
  int fanfare[] = {523, 659, 784, 1047, 1319};
  for (int i = 0; i < 5; i++) {
    tone(BUZZER_PIN, fanfare[i], 120);
    delay(160);
  }
  noTone(BUZZER_PIN);

  // Matrix: display success pattern (simple animation)
  // Clear matrix and show green for 2 seconds
  for (int i = 0; i < 64; i++) {
    matrix.setPixelColor(i, 0x00FF00);  // Green
  }
  matrix.show();
  delay(2000);
  
  // Clear matrix
  for (int i = 0; i < 64; i++) {
    matrix.setPixelColor(i, 0x000000);  // Black
  }
  matrix.show();

  // Settle: final LCD state
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BOMB DISARMED");
  updateModuleCountDisplay();
}

// --- LOSE: Detonated sequence ---
// Uses: Buzzer (wail + rumble), matrix (red flash), LED bar (all on then flicker)

void playDetonatedSequence(bool timedOut) {
  // Step 1 (0ms): Descending wail on buzzer
  // Sweep from 2000Hz to 200Hz over ~350ms
  for (int freq = 2000; freq >= 200; freq -= 90) {
    tone(BUZZER_PIN, freq, 20);
    delay(10);
  }
  noTone(BUZZER_PIN);

  // Matrix: display failure pattern (red flashing for 2500ms)
  for (int i = 0; i < 64; i++) {
    matrix.setPixelColor(i, 0xFF0000);  // Red
  }
  matrix.show();
  delay(2500);
  
  // Clear matrix
  for (int i = 0; i < 64; i++) {
    matrix.setPixelColor(i, 0x000000);  // Black
  }
  matrix.show();

  // LED bar all on
  setLedLevel(10);
  delay(600);

  // Step 4 (1.27s): Low rumble — toggle two low frequencies
  for (int i = 0; i < 15; i++) {
    tone(BUZZER_PIN, (i % 2 == 0) ? 80 : 120, 40);
    delay(40);
  }
  noTone(BUZZER_PIN);

  // Step 5 (2.0s): LED bar and matrix flicker then go dark
  for (int flicker = 0; flicker < 5; flicker++) {
    setLedLevel(10);
    // Flash matrix red
    for (int i = 0; i < 64; i++) {
      matrix.setPixelColor(i, 0xFF0000);  // Red
    }
    matrix.show();
    delay(60);
    
    setLedLevel(0);
    // Matrix off
    for (int i = 0; i < 64; i++) {
      matrix.setPixelColor(i, 0x000000);  // Black
    }
    matrix.show();
    delay(60);
  }

  // Settle: LCD failure message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(timedOut ? "DETONATED" : "GAME FAILED");
  lcd.setCursor(0, 1);
  lcd.print(timedOut ? "TIMER EXPIRED" : "KEY TURNED OFF");
}

// =====================================================
// MAIN SETUP / LOOP
// =====================================================

void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(1000);

  // Initialize 8x8 NeoPixel matrix
  matrix.begin();
  matrix.setBrightness(NEOPIXEL_BRIGHTNESS);  // Limit to 10% brightness (25/255) - protects power supply with buffer for other components
  matrix.show();  // Initialize all pixels to off
  
  // Initialize I2C LCD
  lcd.init();
  lcd.backlight();

  // Initialize generic LED bar (all pins as outputs, start off)
  initializeLedBar();

  // Seed random number generator for truly random serial numbers
  seedRandomNumberGenerator();

  // Generate bomb serial number
  bombSerialNumber = generateSerialNumber();

  // Initialize key switch
  pinMode(KEY_SWITCH_PIN, INPUT);
  keyCurrentState = digitalRead(KEY_SWITCH_PIN);
  keyLastState = keyCurrentState;

  // Initialize encoder pins with internal pull-ups (required for generic encoders)
  // CRITICAL: Both CLK and DT phases must be connected for the Encoder library to work!
  // Left encoder: CLK→D2, DT→D3
  // Right encoder: CLK→D4, DT→D5
  // The Encoder library uses these pins for interrupt detection
  pinMode(ENC_LEFT_A, INPUT_PULLUP);    // D2 - Left encoder CLK
  pinMode(ENC_LEFT_B, INPUT_PULLUP);    // D3 - Left encoder DT
  pinMode(ENC_RIGHT_A, INPUT_PULLUP);   // D4
  pinMode(ENC_RIGHT_B, INPUT_PULLUP);   // D5
  
  // Give the encoder pins a moment to stabilize after pull-up initialization
  delay(50);

  setupBuzzerModule();
  setupFrequencyModule();
  setupCoreModule();
  setupButtonComboModule();
  setupTimerModule();
  clearMazeDisplay();

  // Maze module setup is deferred until after core event to save power
  // (will be called in updateMazeModule when conditions are met)

  // =====================================================
  // APPLY DEBUG BYPASSES
  // =====================================================
  if (BYPASS_FREQUENCY_MODULE) {
    frequencyModuleSolved = true;
    // Switch to maze module when frequency is bypassed
    activeEncoderModule = MAZE_MODULE;
    Serial.println("DEBUG: Frequency module bypassed");
  }
  if (BYPASS_MAZE_MODULE) {
    mazeSolved = true;
    mazeSetupDone = true;
    Serial.println("DEBUG: Maze module bypassed");
  }
  if (BYPASS_CORE_MODULE) {
    coreSolved = true;
    coreTriggered = true;
    Serial.println("DEBUG: Core module bypassed");
  }
  
  // If both frequency AND core are bypassed, setup maze immediately
  if (BYPASS_FREQUENCY_MODULE && BYPASS_CORE_MODULE && !BYPASS_MAZE_MODULE) {
    extern void setupMazeModule();
    setupMazeModule();
    extern bool mazeSetupDone;
    mazeSetupDone = true;
    Serial.println("DEBUG: Maze module initialized (both frequency and core bypassed)");
  }
  if (BYPASS_BUTTON_COMBO_MODULE) {
    buttonComboSolved = true;
    Serial.println("DEBUG: Button combo module bypassed");
  }
  if (BYPASS_TIMER_MODULE) {
    timerModuleSolved = true;
    Serial.println("DEBUG: Timer module bypassed");
  }
  if (BYPASS_SIGNAL_ALIGNMENT) {
    signalAlignmentSolved = true;
    Serial.println("DEBUG: Signal alignment module bypassed");
  }

  // Auto-start game if flag is set (useful when testing without key switch connected)
  if (AUTO_START_GAME) {
    currentGameState = STATE_RUNNING;
    timerRunning = true;
    timerFinished = false;
    timerLastTick = millis();
    remainingSeconds = TIMER_TOTAL_SECONDS;
    updateTimerDisplay();
    Serial.println("DEBUG: Auto-starting game (AUTO_START_GAME = true)");
  }

  lcd.print("Starting!");
  //startMozzi(CONTROL_RATE);
  //COMMENTED OUT BECUASE WE NEED THIS BUZZER TO BE ON D9 FOR IT TO WORK
}

void loop() {
  //audioHook();
  //COMMENTED OUT BECUASE WE NEED THIS BUZZER TO BE ON D9 FOR IT TO WORK
  
  // Check key switch state and handle state transitions
  handleKeySwitch();
  updateGameState();
  
  // Always update display state regardless of game state
  updateDisplayState();

  // Always update countdown to keep timer display showing (even when failed)
  updateCountdown();
  updateBuzzerModule();

  // Only update modules if game is running or won
  if (currentGameState == STATE_RUNNING || currentGameState == STATE_WON) {
    // Check if modules just completed and trigger core event randomly (50% chance)
    checkModuleTransitionsAndTriggerCore();
    
    updateTimerModule();
    updateCoreModule();
    
    // Button combo can always be attempted, even during Core event
    updateButtonComboModule();
    
    // Pause all other modules while core event is active/flashing
    if (!coreTriggered || coreSolved) {
      if (activeEncoderModule == FREQUENCY_MODULE) {
        updateFrequencyModule();
      } else if (activeEncoderModule == MAZE_MODULE) {
        updateMazeModule();
      }
      // Signal alignment disabled for testing
      // if (frequencyModuleSolved && mazeSolved && coreSolved && buttonComboSolved) {
      //   static bool signalSetupDone = false;
      //   if (!signalSetupDone) {
      //     setupSignalAlignmentModule();
      //     signalSetupDone = true;
      //   }
      //   updateSignalAlignmentModule();
      // }
    }
  }
}
