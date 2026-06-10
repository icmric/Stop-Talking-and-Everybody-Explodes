/*
  ktane.ino — KTANE Bomb Defusal Game
  Arduino Mega 2560

  Players solve multiple hardware puzzle modules under a countdown timer.
  A "Defuser" operates the hardware; a "Manual Operator" reads the solution
  guide derived from the randomly-generated serial number displayed on the LCD.

  MODULES:
    Frequency Scrambler  — rotary encoder sequence
    Maze Navigator       — LED matrix pathfinding (shares encoders with Frequency)
    Core Overheating     — blow-to-cool microphone event (random trigger)
    Button Combo         — timed button sequence

  PIN MAP:
    D2, D3   Left encoder  (CLK / DT)
    D4, D5   Right encoder (CLK / DT)
    D6       Buzzer
    D7       Microphone sensor (Core module)
    D8, D9   4-digit TM1637 display (DIO / CLK)
    D10      Red button   (INPUT_PULLUP)
    D11      Green button (INPUT_PULLUP)
    D20, D21 I2C bus — LCD 16×2 (0x27)
    D22      8×8 WS2812B matrix (NeoPixel data)
    D31      Key switch
    D35,37,39,41,43,45,47,49,51,53  LED bar (10 LEDs)
*/

#include <Wire.h>
#include <Encoder.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>
#include <DIYables_4Digit7Segment_TM1637.h>

#include "SerialNumber.h"

// ── Module enable enum — defined before module headers ─────────────────────

enum ActiveEncoderModule { FREQUENCY_MODULE, MAZE_MODULE };

enum GameState {
  STATE_IDLE,      // Waiting for key turn
  STATE_RUNNING,   // Game in progress
  STATE_WON,       // All modules solved; waiting for key-off to confirm disarm
  STATE_FAILED,    // Timer expired or key turned off early
  STATE_DISARMED   // Successfully disarmed
};

// ── Module headers (after enum definitions) ────────────────────────────────

#include "FrequencyModule.h"
#include "CoreModule.h"
#include "MazeModule.h"
#include "ButtonComboModule.h"

// ── Debug / bypass flags ───────────────────────────────────────────────────
// Set to true to skip a module during development/testing.

const bool BYPASS_FREQUENCY_MODULE  = true;
const bool BYPASS_MAZE_MODULE       = false;
const bool BYPASS_CORE_MODULE       = true;
const bool BYPASS_BUTTON_COMBO      = false;

// When true, skips the key-switch requirement and starts immediately on boot.
const bool AUTO_START_GAME = true;

// ── Pin definitions ────────────────────────────────────────────────────────

const int BUZZER_PIN      = 6;
const int KEY_SWITCH_PIN  = 31;

const int ENC_LEFT_A  = 2, ENC_LEFT_B  = 3;
const int ENC_RIGHT_A = 4, ENC_RIGHT_B = 5;

const int DISP_DIO = 8, DISP_CLK = 9;
const int NEOPIXEL_PIN = 22;
const int NUM_PIXELS   = 64;

const int LED_BAR_PINS[10] = {35, 37, 39, 41, 43, 45, 47, 49, 51, 53};

// ── Hardware objects ───────────────────────────────────────────────────────

Encoder encLeft (ENC_LEFT_A,  ENC_LEFT_B);
Encoder encRight(ENC_RIGHT_A, ENC_RIGHT_B);
LiquidCrystal_I2C lcd(0x27, 16, 2);
DIYables_4Digit7Segment_TM1637 tm1637(DISP_CLK, DISP_DIO);
Adafruit_NeoPixel matrix(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ── Game state ─────────────────────────────────────────────────────────────

ActiveEncoderModule activeEncoderModule = FREQUENCY_MODULE;
GameState currentGameState = STATE_IDLE;
String bombSerialNumber    = "";

// ── Timer ──────────────────────────────────────────────────────────────────

const int           TIMER_START_MINUTES  = 5;
const unsigned long TIMER_TOTAL_DURATION = (unsigned long)TIMER_START_MINUTES * 60UL * 1000UL;

int  remainingSeconds = TIMER_START_MINUTES * 60;
bool timerRunning     = false;
bool timerFinished    = false;
unsigned long timerLastTick = 0;

// currentDigits mirrors the live display (MM:SS as four single digits)
int currentDigits[4] = {0, 5, 0, 0};

// ── Display state ──────────────────────────────────────────────────────────

bool serialNumberDisplayed  = false;
int  lastDisplayedGameState = -1;

// ── Buzzer ─────────────────────────────────────────────────────────────────

unsigned long buzzerLastBeepTime = 0;

const unsigned long BUZZER_INTERVAL_START = 5000;
const unsigned long BUZZER_INTERVAL_END   = 90;
const unsigned long BUZZER_BEEP_DURATION  = 80;
const int           BUZZER_FREQ_START     = 800;
const int           BUZZER_FREQ_END       = 2000;
const float         BUZZER_INTERVAL_CURVE = 3.5f;
const float         BUZZER_PITCH_CURVE    = 2.8f;

// ── Key switch debounce ────────────────────────────────────────────────────

bool keyCurrentState    = false;
bool keyLastState       = false;
unsigned long keyLastChangeTime = 0;
const unsigned long KEY_DEBOUNCE_DELAY = 50;

// ── Module transition tracking (for probabilistic Core trigger) ────────────

bool lastFrequencyModuleSolved = false;
bool lastMazeSolved            = false;
bool lastButtonComboSolved     = false;

// ── End-sequence guard ─────────────────────────────────────────────────────

bool endSequencePlayed = false;

// =============================================================================
// LED BAR
// =============================================================================

void initializeLedBar() {
  for (int i = 0; i < 10; i++) {
    pinMode(LED_BAR_PINS[i], OUTPUT);
    digitalWrite(LED_BAR_PINS[i], LOW);
  }
}

void setLedLevel(int level) {
  level = constrain(level, 0, 10);
  for (int i = 0; i < 10; i++)
    digitalWrite(LED_BAR_PINS[i], i < level ? HIGH : LOW);
}

// =============================================================================
// MODULE QUERIES
// =============================================================================

bool allModulesSolved() {
  if (!BYPASS_FREQUENCY_MODULE && !frequencyModuleSolved) return false;
  if (!BYPASS_MAZE_MODULE       && !mazeSolved)           return false;
  if (!BYPASS_CORE_MODULE       && !coreSolved)           return false;
  if (!BYPASS_BUTTON_COMBO      && !buttonComboSolved)    return false;
  return true;
}

int getTotalModuleCount() {
  int n = 0;
  if (!BYPASS_FREQUENCY_MODULE) n++;
  if (!BYPASS_MAZE_MODULE)      n++;
  if (!BYPASS_CORE_MODULE)      n++;
  if (!BYPASS_BUTTON_COMBO)     n++;
  return n;
}

int getSolvedModuleCount() {
  int n = 0;
  if (!BYPASS_FREQUENCY_MODULE && frequencyModuleSolved) n++;
  if (!BYPASS_MAZE_MODULE       && mazeSolved)           n++;
  if (!BYPASS_CORE_MODULE       && coreSolved)           n++;
  if (!BYPASS_BUTTON_COMBO      && buttonComboSolved)    n++;
  return n;
}

// =============================================================================
// PENALTY
// =============================================================================

void applyPenalty(const char* /*reason*/) {
  tone(BUZZER_PIN, 1000, 200);
  if (remainingSeconds > 3) {
    remainingSeconds -= 3;
  } else {
    remainingSeconds = 0;
    timerRunning   = false;
    timerFinished  = true;
    noTone(BUZZER_PIN);
  }
}

// =============================================================================
// AUDIO
// =============================================================================

void stopBuzzer() { noTone(BUZZER_PIN); }

void playSuccessTone() {
  const int notes[] = {523, 659, 784, 1047, 784, 659, 523};
  for (int i = 0; i < 7; i++) {
    tone(BUZZER_PIN, notes[i], 150);
    delay(200);
  }
  noTone(BUZZER_PIN);
}

float getTimerProgress() {
  long msLeft = (long)remainingSeconds * 1000L - (long)(millis() - timerLastTick);
  msLeft = constrain(msLeft, 0L, (long)TIMER_TOTAL_DURATION);
  return 1.0f - ((float)msLeft / (float)TIMER_TOTAL_DURATION);
}

void updateBuzzer() {
  // Core alert overrides normal beeping
  if (coreMessageShown && !coreSolved) {
    unsigned long cycleTime = millis() % 1000;
    if (cycleTime < 200 || (cycleTime >= 250 && cycleTime < 450) || (cycleTime >= 500 && cycleTime < 700))
      tone(BUZZER_PIN, 1500, 200);
    else
      noTone(BUZZER_PIN);
    return;
  }

  if (!timerRunning || timerFinished || remainingSeconds <= 0) {
    noTone(BUZZER_PIN);
    return;
  }

  unsigned long now = millis();

  if (remainingSeconds <= 15) {
    // Rapid panic beep in final 15 seconds
    unsigned long interval  = 100;
    int           panicFreq = 1000 + (15 - remainingSeconds) * 50;
    if (now - buzzerLastBeepTime >= interval) {
      tone(BUZZER_PIN, panicFreq, 50);
      buzzerLastBeepTime = now;
    }
  } else {
    float t = getTimerProgress();
    unsigned long interval = BUZZER_INTERVAL_START -
      (unsigned long)((BUZZER_INTERVAL_START - BUZZER_INTERVAL_END) * pow(t, BUZZER_INTERVAL_CURVE));
    int freq = BUZZER_FREQ_START +
      (int)((BUZZER_FREQ_END - BUZZER_FREQ_START) * pow(t, BUZZER_PITCH_CURVE));

    if (now - buzzerLastBeepTime >= interval) {
      tone(BUZZER_PIN, freq, BUZZER_BEEP_DURATION);
      buzzerLastBeepTime = now;
    }
  }
}

// =============================================================================
// TIMER DISPLAY
// =============================================================================

void updateTimerDisplay() {
  int mins = remainingSeconds / 60;
  int secs = remainingSeconds % 60;
  currentDigits[0] = mins / 10;
  currentDigits[1] = mins % 10;
  currentDigits[2] = secs / 10;
  currentDigits[3] = secs % 10;
  tm1637.printTime(mins, secs, true);
}

void updateCountdown() {
  // Keep display current regardless of state
  if (currentGameState == STATE_RUNNING ||
      currentGameState == STATE_WON     ||
      currentGameState == STATE_FAILED) {
    updateTimerDisplay();
  }

  if (!timerRunning || timerFinished) return;

  if (millis() - timerLastTick >= 1000) {
    timerLastTick += 1000;
    if (remainingSeconds > 0) remainingSeconds--;

    if (remainingSeconds <= 0) {
      remainingSeconds = 0;
      timerRunning     = false;
      timerFinished    = true;
      stopBuzzer();
      if (currentGameState == STATE_RUNNING || currentGameState == STATE_WON)
        currentGameState = STATE_FAILED;
    }
  }
}

// =============================================================================
// LCD DISPLAY MANAGEMENT
// =============================================================================

void updateModuleCountDisplay() {
  String s = String(getSolvedModuleCount()) + "/" + String(getTotalModuleCount());
  lcd.setCursor(16 - s.length(), 1);
  lcd.print(s);
}

void displaySerialNumber() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SN:"); lcd.print(bombSerialNumber);
  updateModuleCountDisplay();
}

void updateDisplayState() {
  if (currentGameState != lastDisplayedGameState) {
    if (currentGameState == STATE_IDLE) {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("BOMB ARMED");
      lcd.setCursor(0, 1); lcd.print("TURN KEY");
    }
    lastDisplayedGameState = currentGameState;
    serialNumberDisplayed  = false;
  }

  if (!serialNumberDisplayed &&
      (currentGameState == STATE_RUNNING || currentGameState == STATE_WON)) {
    if (!coreTriggered || coreSolved) {
      displaySerialNumber();
      serialNumberDisplayed = true;
    }
  }

  if (serialNumberDisplayed && (!coreTriggered || coreSolved))
    updateModuleCountDisplay();
}

// =============================================================================
// WIN / LOSE SEQUENCES
// =============================================================================

void playDisarmedSequence() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("** DISARMED **");
  lcd.setCursor(0, 1);
  int m = remainingSeconds / 60, s = remainingSeconds % 60;
  lcd.print(m); lcd.print(":");
  if (s < 10) lcd.print("0");
  lcd.print(s); lcd.print(" remaining");

  delay(100);
  for (int i = 1; i <= 10; i++) { setLedLevel(i); delay(100); }

  const int fanfare[] = {523, 659, 784, 1047, 1319};
  for (int i = 0; i < 5; i++) { tone(BUZZER_PIN, fanfare[i], 120); delay(160); }
  noTone(BUZZER_PIN);

  // Green flash on matrix
  for (int i = 0; i < 64; i++) matrix.setPixelColor(i, 0x00FF00);
  matrix.show(); delay(2000);
  for (int i = 0; i < 64; i++) matrix.setPixelColor(i, 0x000000);
  matrix.show();

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("BOMB DISARMED");
  updateModuleCountDisplay();
}

void playDetonatedSequence(bool timedOut) {
  for (int f = 2000; f >= 200; f -= 90) { tone(BUZZER_PIN, f, 20); delay(10); }
  noTone(BUZZER_PIN);

  for (int i = 0; i < 64; i++) matrix.setPixelColor(i, 0xFF0000);
  matrix.show(); delay(2500);
  for (int i = 0; i < 64; i++) matrix.setPixelColor(i, 0x000000);
  matrix.show();

  setLedLevel(10); delay(600);

  for (int i = 0; i < 15; i++) {
    tone(BUZZER_PIN, (i % 2 == 0) ? 80 : 120, 40); delay(40);
  }
  noTone(BUZZER_PIN);

  for (int i = 0; i < 5; i++) {
    setLedLevel(10);
    for (int j = 0; j < 64; j++) matrix.setPixelColor(j, 0xFF0000);
    matrix.show(); delay(60);
    setLedLevel(0);
    for (int j = 0; j < 64; j++) matrix.setPixelColor(j, 0x000000);
    matrix.show(); delay(60);
  }

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(timedOut ? "DETONATED"    : "GAME FAILED");
  lcd.setCursor(0, 1); lcd.print(timedOut ? "TIMER EXPIRED": "KEY TURNED OFF");
}

// =============================================================================
// KEY SWITCH
// =============================================================================

bool readKeySwitchDebounced() {
  bool reading = digitalRead(KEY_SWITCH_PIN);
  unsigned long now = millis();
  if (reading != keyLastState) keyLastChangeTime = now;
  if ((now - keyLastChangeTime) >= KEY_DEBOUNCE_DELAY && reading != keyCurrentState) {
    keyCurrentState = reading;
    keyLastState    = reading;
    return true;
  }
  keyLastState = reading;
  return false;
}

void resetGameModules() {
  frequencyModuleSolved = false;
  mazeSolved            = false;
  mazeSetupDone         = false;
  mazeDisplayCleared    = false;
  coreTriggered         = false;
  coreSolved            = false;
  buttonComboSolved     = false;
  endSequencePlayed     = false;

  lastFrequencyModuleSolved = false;
  lastMazeSolved            = false;
  lastButtonComboSolved     = false;
}

void startGame() {
  resetGameModules();
  currentGameState = STATE_RUNNING;
  timerRunning     = true;
  timerFinished    = false;
  timerLastTick    = millis();
  remainingSeconds = TIMER_START_MINUTES * 60;
  activeEncoderModule = BYPASS_FREQUENCY_MODULE ? MAZE_MODULE : FREQUENCY_MODULE;
  updateTimerDisplay();
}

void handleKeySwitch() {
  if (!readKeySwitchDebounced()) return;

  if (keyCurrentState == HIGH) {
    // Key turned ON
    if (currentGameState == STATE_IDLE) startGame();

  } else {
    // Key turned OFF
    if (currentGameState == STATE_RUNNING) {
      currentGameState = STATE_FAILED;
      timerRunning     = false;
      stopBuzzer();
      if (!endSequencePlayed) {
        playDetonatedSequence(false);
        endSequencePlayed = true;
      }
    } else if (currentGameState == STATE_WON) {
      if (!coreTriggered && !coreSolved) { triggerCoreEvent(); delay(100); }
      currentGameState = STATE_DISARMED;
      timerRunning     = false;
      stopBuzzer();
      if (!endSequencePlayed) {
        playDisarmedSequence();
        endSequencePlayed = true;
      }
    }
  }
}

// =============================================================================
// GAME STATE MANAGEMENT
// =============================================================================

void checkModuleTransitionsAndTriggerCore() {
  if (coreTriggered || coreSolved) return;

  bool anyJustCompleted =
    (!lastFrequencyModuleSolved && frequencyModuleSolved) ||
    (!lastMazeSolved            && mazeSolved)            ||
    (!lastButtonComboSolved     && buttonComboSolved);

  if (anyJustCompleted && random(100) < 50) triggerCoreEvent();

  lastFrequencyModuleSolved = frequencyModuleSolved;
  lastMazeSolved            = mazeSolved;
  lastButtonComboSolved     = buttonComboSolved;
}

void updateGameState() {
  if (currentGameState == STATE_RUNNING && allModulesSolved()) {
    if (!coreTriggered && !coreSolved) triggerCoreEvent();
    currentGameState = STATE_WON;
  }
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(500);

  matrix.begin();
  matrix.setBrightness(25);  // ~10% — protects power supply
  matrix.show();

  lcd.init();
  lcd.backlight();

  initializeLedBar();

  pinMode(BUZZER_PIN,     OUTPUT);
  pinMode(KEY_SWITCH_PIN, INPUT);
  pinMode(ENC_LEFT_A,  INPUT_PULLUP);
  pinMode(ENC_LEFT_B,  INPUT_PULLUP);
  pinMode(ENC_RIGHT_A, INPUT_PULLUP);
  pinMode(ENC_RIGHT_B, INPUT_PULLUP);
  delay(50);  // Let pull-ups settle

  keyCurrentState = digitalRead(KEY_SWITCH_PIN);
  keyLastState    = keyCurrentState;

  seedRandom();
  bombSerialNumber = generateSerialNumber();

  tm1637.begin();
  tm1637.printTime(88, 88, true);  // "88:88" splash

  setupCoreModule();
  setupFrequencyModule();
  setupButtonComboModule();
  clearMazeDisplay();
  // Maze full setup is deferred until Frequency + Core are complete

  // ── Apply bypass flags ─────────────────────────────────────────────────

  if (BYPASS_FREQUENCY_MODULE) {
    frequencyModuleSolved = true;
    activeEncoderModule   = MAZE_MODULE;
    Serial.println(F("BYPASS: Frequency module"));
  }
  if (BYPASS_MAZE_MODULE) {
    mazeSolved    = true;
    mazeSetupDone = true;
    Serial.println(F("BYPASS: Maze module"));
  }
  if (BYPASS_CORE_MODULE) {
    coreTriggered = true;
    coreSolved    = true;
    Serial.println(F("BYPASS: Core module"));
  }
  if (BYPASS_FREQUENCY_MODULE && BYPASS_CORE_MODULE && !BYPASS_MAZE_MODULE) {
    setupMazeModule();
    mazeSetupDone = true;
    Serial.println(F("BYPASS: Maze initialised immediately (Freq + Core bypassed)"));
  }
  if (BYPASS_BUTTON_COMBO) {
    buttonComboSolved = true;
    Serial.println(F("BYPASS: Button combo"));
  }

  debugPrintSerial();

  if (AUTO_START_GAME) {
    startGame();
    Serial.println(F("AUTO_START: game running"));
  } else {
    updateDisplayState();  // Show "BOMB ARMED / TURN KEY"
  }
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
  handleKeySwitch();
  updateGameState();
  updateDisplayState();
  updateCountdown();
  updateBuzzer();

  if (currentGameState != STATE_RUNNING && currentGameState != STATE_WON) return;

  checkModuleTransitionsAndTriggerCore();
  updateCoreModule();

  // Button combo is always interactable, even during Core event
  updateButtonComboModule();

  // All other modules pause while Core is active
  if (!coreTriggered || coreSolved) {
    if (activeEncoderModule == FREQUENCY_MODULE) {
      updateFrequencyModule();
    } else {
      updateMazeModule();
    }
  }
}
