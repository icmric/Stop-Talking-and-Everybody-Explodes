/*
  Combined Bomb Modules Sketch

  Modules included:
  - Exponential buzzer countdown on D6
  - Frequency scrambler with LED bar and two encoders
  - 8x8 RGB LED matrix maze using the same two encoders
  - TM1637 countdown timer with Grove LED Button
  - Core overheating humidity/LCD module

  Important pin note:
  The original Encoder_Tuning.ino used DHT on D4, but D4/D5 are already used by the right encoder.
  This combined version moves the DHT sensor to D7.

  Libraries required:
  - Grove LED Bar
  - Encoder by Paul Stoffregen
  - DHT sensor library by Adafruit
  - LiquidCrystal_I2C
  - TM1637
  - Grove two RGB LED Matrix
*/

#include <Wire.h>
#include <math.h>
#include <Grove_LED_Bar.h>
#include <Encoder.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include "TM1637.h"
#include "grove_two_rgb_led_matrix.h"

// =====================================================
// SHARED PIN MAP
// =====================================================

const int BUZZER_PIN = 6;

const int ENC_LEFT_A = 2;
const int ENC_LEFT_B = 3;
const int ENC_RIGHT_A = 4;
const int ENC_RIGHT_B = 5;

const int DHT_PIN = 7;     // moved from D4 to avoid right encoder conflict
const int DHT_TYPE = DHT11;

const int DISP_CLK = 8;
const int DISP_DIO = 9;

const int LED_PIN = 10;
const int BUTTON_PIN = 11;

const int GAME_KEY_PIN = 22;  // Key switch pin (HIGH = game on, LOW = game off)

const int LED_BAR_CLK = 13;
const int LED_BAR_DATA = 12;

// =====================================================
// MODULE SELECTION
// =====================================================
// Because the frequency module and maze both use the same two encoders,
// only one encoder-controlled module should be active at a time.

enum ActiveEncoderModule {
  FREQUENCY_MODULE,
  MAZE_MODULE
};

ActiveEncoderModule activeEncoderModule = FREQUENCY_MODULE;

// =====================================================
// HARDWARE OBJECTS
// =====================================================

Grove_LED_Bar ledBar(LED_BAR_CLK, LED_BAR_DATA, 0);
Encoder encLeft(ENC_LEFT_A, ENC_LEFT_B);
Encoder encRight(ENC_RIGHT_A, ENC_RIGHT_B);
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
TM1637 tm1637(DISP_CLK, DISP_DIO);
GroveTwoRGBLedMatrixClass matrix;

// =====================================================
// SHARED GAME TIMER STATE
// =====================================================

const int START_MINUTES = 2;
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

// =====================================================
// FREQUENCY SCRAMBLER MODULE
// =====================================================

struct ComboStep {
  char encoder;
  int direction; // 1 = CW, -1 = CCW
  int clicks;
};

ComboStep combination[] = {
  {'L', 1, 3},
  {'R', -1, 2},
  {'L', -1, 2}
};

int frequencyCurrentStep = 0;
const int frequencyTotalSteps = 3;
int frequencyClicksInCurrentStep = 0;
long frequencyLastLeftPos = 0;
long frequencyLastRightPos = 0;

unsigned long frequencyLastMoveTime = 0;
const int frequencyConfirmDelay = 500;
bool frequencyModuleSolved = false;

void updateFrequencyDisplay() {
  int displayLevel = map(frequencyCurrentStep, 0, frequencyTotalSteps, 10, 1);
  ledBar.setLevel(displayLevel);
}

void flashFrequencySuccess() {
  Serial.println("Frequency module solved!");
  for (int i = 0; i < 4; i++) {
    ledBar.setLevel(10); delay(150);
    ledBar.setLevel(0);  delay(150);
  }
  ledBar.setLevel(10);
}

void setupFrequencyModule() {
  ledBar.begin();
  frequencyLastLeftPos = encLeft.read() / 4;
  frequencyLastRightPos = encRight.read() / 4;
  updateFrequencyDisplay();
}

void updateFrequencyModule() {
  if (frequencyModuleSolved) {
    return;
  }

  long currLeft = encLeft.read() / 4;
  long currRight = encRight.read() / 4;
  ComboStep target = combination[frequencyCurrentStep];

  bool moved = false;
  int moveDir = 0;

  if (target.encoder == 'L' && currLeft != frequencyLastLeftPos) {
    moveDir = (currLeft > frequencyLastLeftPos) ? 1 : -1;
    moved = true;
    frequencyLastLeftPos = currLeft;
  } else if (target.encoder == 'R' && currRight != frequencyLastRightPos) {
    moveDir = (currRight > frequencyLastRightPos) ? 1 : -1;
    moved = true;
    frequencyLastRightPos = currRight;
  }

  if (moved) {
    frequencyLastMoveTime = millis();

    if (moveDir == target.direction) {
      frequencyClicksInCurrentStep++;
      Serial.print("Frequency click: ");
      Serial.println(frequencyClicksInCurrentStep);
    } else {
      frequencyClicksInCurrentStep = 0;
      Serial.println("Wrong direction, frequency step reset");
    }
  }

  if (frequencyClicksInCurrentStep >= target.clicks && millis() - frequencyLastMoveTime > frequencyConfirmDelay) {
    frequencyCurrentStep++;
    frequencyClicksInCurrentStep = 0;

    Serial.print("Frequency step confirmed: ");
    Serial.println(frequencyCurrentStep);
    updateFrequencyDisplay();

    if (frequencyCurrentStep >= frequencyTotalSteps) {
      frequencyModuleSolved = true;
      flashFrequencySuccess();
      activeEncoderModule = MAZE_MODULE;
      setupMazeModule();
    }
  }
}

// =====================================================
// CORE OVERHEATING MODULE
// =====================================================

const float HUMIDITY_THRESHOLD = 90.0;
const unsigned long BLOW_DURATION = 10000;

bool coreSolved = false;
bool coreBlowing = false;
bool coreMessageShown = false;
unsigned long coreBlowStart = 0;
unsigned long coreLastDHTRead = 0;

void setupCoreModule() {
  dht.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
}

void updateCoreModule() {
  if (coreSolved) {
    return;
  }

  if (!coreMessageShown) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CORE OVERHEAT!");
    lcd.setCursor(0, 1);
    lcd.print("BLOW TO COOL");
    coreMessageShown = true;
  }

  unsigned long now = millis();

  if (now - coreLastDHTRead >= 1200) {
    coreLastDHTRead = now;
    float humidity = dht.readHumidity();

    if (!isnan(humidity)) {
      if (humidity >= HUMIDITY_THRESHOLD) {
        if (!coreBlowing) {
          coreBlowing = true;
          coreBlowStart = now;
        }
      } else {
        coreBlowing = false;
        lcd.setCursor(0, 1);
        lcd.print("BLOW TO COOL    ");
      }
    }
  }

  if (coreBlowing) {
    int secondsLeft = (BLOW_DURATION - (now - coreBlowStart)) / 1000 + 1;
    lcd.setCursor(0, 1);
    lcd.print("KEEP BLOWING: ");
    lcd.print(secondsLeft);
    lcd.print("s ");

    if (now - coreBlowStart >= BLOW_DURATION) {
      coreSolved = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("CORE STABLE");
    }
  }
}

// =====================================================
// MATRIX MAZE MODULE
// =====================================================

const int MAZE_W = 6;
const int MAZE_H = 6;

int playerX = 0;
int playerY = 0;
const int goalX = 5;
const int goalY = 5;

bool mazeSolved = false;
uint8_t frame[64];
long mazeLastLeftPos = 0;
long mazeLastRightPos = 0;
unsigned long mazeLastMoveTime = 0;
const unsigned long mazeMoveDelay = 120;

bool wallRight[MAZE_H][MAZE_W];
bool wallDown[MAZE_H][MAZE_W];
bool wallLeft[MAZE_H][MAZE_W];
bool wallUp[MAZE_H][MAZE_W];

void clearWalls() {
  for (int y = 0; y < MAZE_H; y++) {
    for (int x = 0; x < MAZE_W; x++) {
      wallRight[y][x] = false;
      wallDown[y][x] = false;
      wallLeft[y][x] = false;
      wallUp[y][x] = false;
    }
  }
}

void buildOuterWalls() {
  for (int x = 0; x < MAZE_W; x++) {
    wallUp[0][x] = true;
    wallDown[MAZE_H - 1][x] = true;
  }
  for (int y = 0; y < MAZE_H; y++) {
    wallLeft[y][0] = true;
    wallRight[y][MAZE_W - 1] = true;
  }
}

void addVerticalWall(int x, int y) {
  wallRight[y][x] = true;
  wallLeft[y][x + 1] = true;
}

void addHorizontalWall(int x, int y) {
  wallDown[y][x] = true;
  wallUp[y + 1][x] = true;
}

void setupFirstMaze() {
  clearWalls();
  buildOuterWalls();

  addVerticalWall(0, 1);
  addVerticalWall(0, 2);
  addVerticalWall(0, 3);
  addVerticalWall(1, 5);
  addVerticalWall(2, 0);
  addVerticalWall(2, 1);
  addVerticalWall(2, 2);
  addVerticalWall(2, 4);
  addVerticalWall(3, 3);
  addVerticalWall(3, 5);
  addVerticalWall(4, 4);

  addHorizontalWall(1, 0);
  addHorizontalWall(4, 0);
  addHorizontalWall(5, 0);
  addHorizontalWall(2, 1);
  addHorizontalWall(3, 1);
  addHorizontalWall(4, 1);
  addHorizontalWall(1, 2);
  addHorizontalWall(4, 2);
  addHorizontalWall(1, 3);
  addHorizontalWall(2, 3);
  addHorizontalWall(3, 3);
  addHorizontalWall(4, 3);
  addHorizontalWall(1, 4);
  addHorizontalWall(4, 4);
}

void drawScene() {
  for (int i = 0; i < 64; i++) {
    frame[i] = black;
  }

  for (int x = 0; x < 8; x++) {
    frame[x] = white;
    frame[7 * 8 + x] = white;
  }

  for (int y = 0; y < 8; y++) {
    frame[y * 8] = white;
    frame[y * 8 + 7] = white;
  }

  int px = playerX + 1;
  int py = playerY + 1;
  int gx = goalX + 1;
  int gy = goalY + 1;

  frame[gy * 8 + gx] = mazeSolved ? green : blue;

  if (!mazeSolved) {
    frame[py * 8 + px] = red;
  }

  matrix.displayFrames(frame, 0, true, 1);
}

void checkMazeSolved() {
  if (playerX == goalX && playerY == goalY) {
    mazeSolved = true;
    drawScene();
    Serial.println("Maze solved!");
  }
}

bool moveRight() {
  if (!wallRight[playerY][playerX]) {
    playerX++;
    return true;
  }
  return false;
}

bool moveLeft() {
  if (!wallLeft[playerY][playerX]) {
    playerX--;
    return true;
  }
  return false;
}

bool moveDown() {
  if (!wallDown[playerY][playerX]) {
    playerY++;
    return true;
  }
  return false;
}

bool moveUp() {
  if (!wallUp[playerY][playerX]) {
    playerY--;
    return true;
  }
  return false;
}

void setupMazeModule() {
  if (matrix.getDeviceVID() != 0x2886) {
    Serial.println("Matrix not detected");
    return;
  }

  playerX = 0;
  playerY = 0;
  mazeSolved = false;
  mazeLastLeftPos = encLeft.read() / 4;
  mazeLastRightPos = encRight.read() / 4;
  setupFirstMaze();
  drawScene();
}

void updateMazeModule() {
  if (mazeSolved) {
    return;
  }

  bool changed = false;
  unsigned long now = millis();

  long currLeft = encLeft.read() / 4;
  if (currLeft != mazeLastLeftPos && now - mazeLastMoveTime > mazeMoveDelay) {
    if (currLeft > mazeLastLeftPos) {
      changed = moveRight();
    } else {
      changed = moveLeft();
    }
    mazeLastLeftPos = currLeft;
    if (changed) {
      mazeLastMoveTime = now;
    }
  }

  long currRight = encRight.read() / 4;
  if (currRight != mazeLastRightPos && now - mazeLastMoveTime > mazeMoveDelay) {
    if (currRight > mazeLastRightPos) {
      changed = moveDown();
    } else {
      changed = moveUp();
    }
    mazeLastRightPos = currRight;
    if (changed) {
      mazeLastMoveTime = now;
    }
  }

  if (changed) {
    checkMazeSolved();
    drawScene();
  }
}

// =====================================================
// TIMER BUTTON MODULE
// =====================================================

bool lastButtonReading = HIGH;
bool debouncedButtonState = HIGH;
unsigned long timerLastDebounceTime = 0;
const unsigned long timerDebounceDelay = 30;

// =====================================================
// GAME KEY SWITCH MODULE
// =====================================================

bool lastKeyReading = LOW;
bool keyState = false;  // false = off, true = on
unsigned long keyLastDebounceTime = 0;
const unsigned long keyDebounceDelay = 50;

int currentDigits[4] = {0, 2, 0, 0};

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
  pinMode(GAME_KEY_PIN, INPUT_PULLDOWN);  // Key switch with internal pull-down
  digitalWrite(LED_PIN, LOW);

  updateTimerDisplay();
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
      updateTimerDisplay();
    }

    if (remainingSeconds <= 0) {
      remainingSeconds = 0;
      timerRunning = false;
      timerFinished = true;
      noTone(BUZZER_PIN);
      updateTimerDisplay();
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

void updateKeyState() {
  bool reading = digitalRead(GAME_KEY_PIN);

  if (reading != lastKeyReading) {
    keyLastDebounceTime = millis();
  }

  if ((millis() - keyLastDebounceTime) > keyDebounceDelay) {
    bool newKeyState = reading == HIGH;
    
    if (newKeyState != keyState) {
      keyState = newKeyState;
      
      if (keyState) {
        // Key turned ON - restart the game
        Serial.println("GAME KEY ON - Restarting game");
        remainingSeconds = TIMER_TOTAL_SECONDS;
        timerRunning = true;
        timerFinished = false;
        buzzerLastBeepTime = 0;
        updateTimerDisplay();
      } else {
        // Key turned OFF - pause/reset the game
        Serial.println("GAME KEY OFF - Pausing game");
        timerRunning = false;
        noTone(BUZZER_PIN);
        updateTimerDisplay();
      }
    }
  }

  lastKeyReading = reading;
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

  setupBuzzerModule();
  setupFrequencyModule();
  setupCoreModule();
  setupTimerModule();

  if (activeEncoderModule == MAZE_MODULE) {
    setupMazeModule();
  }

  Serial.println("Combined system ready");
}

void loop() {
  updateKeyState();  // Always check key state first
  
  if (keyState) {
    // Only run game when key is ON
    updateTimerModule();
    updateBuzzerModule();
    updateCoreModule();

    if (activeEncoderModule == FREQUENCY_MODULE) {
      updateFrequencyModule();
    } else if (activeEncoderModule == MAZE_MODULE) {
      updateMazeModule();
    }
  }
}
