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
    D12      Vibration motor (via transistor; active HIGH)
    D8, D9   4-digit TM1637 display (DIO / CLK)
    D10      Red button   (INPUT_PULLUP)
    D11      Green button (INPUT_PULLUP)
    D20, D21 I2C bus — LCD 16×2 (0x27)
    D22      8×8 WS2812B matrix (NeoPixel data)
    D31      Key switch
    D35,37,39,41,43,45,47,49,51,53  LED bar (10 LEDs)
    
    HARDWARE WIRE CUTTING INTERFACE (Ground-to-Pin Arrays):
    A7       Color 0: RED
    A6       Color 1: BLUE
    A5       Color 2: YELLOW
    A4       Color 3: GREEN
    A3       Color 4: BLACK
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

const bool BYPASS_FREQUENCY_MODULE = false;
const bool BYPASS_MAZE_MODULE = false;
const bool BYPASS_CORE_MODULE = false;
const bool BYPASS_BUTTON_COMBO = false;

// When true, skips the key-switch requirement and starts immediately on boot.
const bool AUTO_START_GAME = false;

// ── Pin definitions ────────────────────────────────────────────────────────

const int BUZZER_PIN = 6;
const int VIBRATION_PIN = 12;
const int KEY_SWITCH_PIN = 31;

const int ENC_LEFT_A = 2, ENC_LEFT_B = 3;
const int ENC_RIGHT_A = 4, ENC_RIGHT_B = 5;

const int DISP_DIO = 8, DISP_CLK = 9;
const int NEOPIXEL_PIN = 22;
const int NUM_PIXELS = 64;

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
String bombSerialNumber = "";

// ── Timer ──────────────────────────────────────────────────────────────────

const int TIMER_START_MINUTES  = 5;
const unsigned long TIMER_TOTAL_DURATION = (unsigned long)TIMER_START_MINUTES * 60UL * 1000UL;

int remainingSeconds = TIMER_START_MINUTES * 60;
bool timerRunning = false;
bool timerFinished = false;
unsigned long timerLastTick = 0;

// currentDigits mirrors the live display (MM:SS as four single digits)
int currentDigits[4] = {0, 5, 0, 0};

// ── Display state ──────────────────────────────────────────────────────────

bool serialNumberDisplayed = false;
int  lastDisplayedGameState = -1;

// ── Buzzer / vibration ─────────────────────────────────────────────────────

unsigned long buzzerLastBeepTime = 0;
unsigned long vibrationEndTime = 0;
bool vibrationContinuous = false;

const unsigned long BUZZER_INTERVAL_START = 5000;
const unsigned long BUZZER_INTERVAL_END = 90;
const unsigned long BUZZER_BEEP_DURATION = 80;
const int BUZZER_FREQ_START = 800;
const int BUZZER_FREQ_END = 2000;
const float BUZZER_INTERVAL_CURVE = 3.5f;
const float BUZZER_PITCH_CURVE = 2.8f;

void startVibration(unsigned long durationMs = 0);
void stopVibration();
void updateVibration();
void buzzerTone(int frequency, unsigned long duration);
void buzzerToneWait(int frequency, unsigned long duration, unsigned long waitMs);
void stopBuzzer();

// ── Key switch debounce ────────────────────────────────────────────────────

bool keyCurrentState = false;
bool keyLastState = false;
unsigned long keyLastChangeTime = 0;
const unsigned long KEY_DEBOUNCE_DELAY = 50;

// ── Wire-Cutting Tracking Hardware Variables ───────────────────────────────

bool frequencyWirePulled = false;
bool mazeWirePulled = false;
bool buttonComboWirePulled = false;

bool lastFrequencyWirePulled = false;
bool lastMazeWirePulled = false;
bool lastButtonComboWirePulled = false;

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
  if (!BYPASS_FREQUENCY_MODULE && !(frequencyModuleSolved && frequencyWirePulled)) return false;
  if (!BYPASS_MAZE_MODULE && !(mazeSolved && mazeWirePulled)) return false;
  if (!BYPASS_CORE_MODULE && !coreSolved) return false;
  if (!BYPASS_BUTTON_COMBO && !(buttonComboSolved && buttonComboWirePulled)) return false;
  return true;
}

int getTotalModuleCount() {
  int n = 0;
  if (!BYPASS_FREQUENCY_MODULE) n++;
  if (!BYPASS_MAZE_MODULE) n++;
  if (!BYPASS_CORE_MODULE) n++;
  if (!BYPASS_BUTTON_COMBO) n++;
  return n;
}

int getSolvedModuleCount() {
  int n = 0;
  if (!BYPASS_FREQUENCY_MODULE && frequencyModuleSolved && frequencyWirePulled) n++;
  if (!BYPASS_MAZE_MODULE && mazeSolved && mazeWirePulled) n++;
  if (!BYPASS_CORE_MODULE && coreSolved) n++;
  if (!BYPASS_BUTTON_COMBO && buttonComboSolved && buttonComboWirePulled) n++;
  return n;
}

// =============================================================================
// PENALTY
// =============================================================================

void applyPenalty(const char* /*reason*/) {
  buzzerTone(1000, 200);
  if (remainingSeconds > 3) {
    remainingSeconds -= 3;
  } else {
    remainingSeconds = 0;
    timerRunning = false;
    timerFinished = true;
    noTone(BUZZER_PIN);
  }
}

// =============================================================================
// AUDIO
// =============================================================================

void startVibration(unsigned long durationMs) {
  digitalWrite(VIBRATION_PIN, HIGH);
  if (durationMs == 0) {
    vibrationContinuous = true;
    vibrationEndTime = 0;
  } else {
    vibrationContinuous = false;
    vibrationEndTime = millis() + durationMs;
  }
}

void stopVibration() {
  digitalWrite(VIBRATION_PIN, LOW);
  vibrationContinuous = false;
  vibrationEndTime = 0;
}

void updateVibration() {
  if (vibrationContinuous || vibrationEndTime == 0) return;
  if (millis() >= vibrationEndTime) {
    digitalWrite(VIBRATION_PIN, LOW);
    vibrationEndTime = 0;
  }
}

void buzzerTone(int frequency, unsigned long duration) {
  tone(BUZZER_PIN, frequency, duration);
  if (!vibrationContinuous) startVibration(duration);
}

void buzzerToneWait(int frequency, unsigned long duration, unsigned long waitMs) {
  buzzerTone(frequency, duration);
  unsigned long end = millis() + waitMs;
  while (millis() < end) updateVibration();
  if (!vibrationContinuous) stopVibration();
}

void stopBuzzer() {
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
  if (!vibrationContinuous) stopVibration();
}

void playSuccessTone() {
  const int notes[] = {523, 659, 784, 1047, 784, 659, 523};
  for (int i = 0; i < 7; i++) {
    buzzerToneWait(notes[i], 150, 200);
  }
  stopBuzzer();
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
      buzzerTone(1500, 200);
    else
      stopBuzzer();
    return;
  }

  if (!timerRunning || timerFinished || remainingSeconds <= 0) {
    stopBuzzer();
    return;
  }

  unsigned long now = millis();

  if (remainingSeconds <= 15) {
    // Rapid panic beep in final 15 seconds
    unsigned long interval  = 100;
    int panicFreq = 1000 + (15 - remainingSeconds) * 50;
    if (now - buzzerLastBeepTime >= interval) {
      buzzerTone(panicFreq, 50);
      buzzerLastBeepTime = now;
    }
  } else {
    float t = getTimerProgress();
    unsigned long interval = BUZZER_INTERVAL_START -
      (unsigned long)((BUZZER_INTERVAL_START - BUZZER_INTERVAL_END) * pow(t, BUZZER_INTERVAL_CURVE));
    int freq = BUZZER_FREQ_START +
      (int)((BUZZER_FREQ_END - BUZZER_FREQ_START) * pow(t, BUZZER_PITCH_CURVE));

    if (now - buzzerLastBeepTime >= interval) {
      buzzerTone(freq, BUZZER_BEEP_DURATION);
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
      timerRunning = false;
      timerFinished = true;
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
  startVibration();

  for (int f = 2000; f >= 200; f -= 90) {
    buzzerTone(f, 20);
    unsigned long end = millis() + 10;
    while (millis() < end) updateVibration();
  }
  noTone(BUZZER_PIN);

  for (int i = 0; i < 64; i++) matrix.setPixelColor(i, 0xFF0000);
  matrix.show();
  unsigned long redEnd = millis() + 2500;
  while (millis() < redEnd) updateVibration();
  for (int i = 0; i < 64; i++) matrix.setPixelColor(i, 0x000000);
  matrix.show();

  setLedLevel(10);
  unsigned long ledEnd = millis() + 600;
  while (millis() < ledEnd) updateVibration();

  for (int i = 0; i < 15; i++) {
    buzzerToneWait((i % 2 == 0) ? 80 : 120, 40, 40);
  }
  noTone(BUZZER_PIN);

  for (int i = 0; i < 5; i++) {
    setLedLevel(10);
    for (int j = 0; j < 64; j++) matrix.setPixelColor(j, 0xFF0000);
    matrix.show();
    unsigned long flashOn = millis() + 60;
    while (millis() < flashOn) updateVibration();
    setLedLevel(0);
    for (int j = 0; j < 64; j++) matrix.setPixelColor(j, 0x000000);
    matrix.show();
    unsigned long flashOff = millis() + 60;
    while (millis() < flashOff) updateVibration();
  }

  stopBuzzer();
  stopVibration();

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(timedOut ? "DETONATED"     : "GAME FAILED");
  lcd.setCursor(0, 1); lcd.print(timedOut ? "TIMER EXPIRED": "WIRE FAULT / KEY OFF");
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
  mazeSolved = false;
  mazeSetupDone = false;
  mazeDisplayCleared = false;
  coreTriggered = false;
  coreSolved = false;
  buttonComboSolved = false;
  endSequencePlayed = false;

  frequencyWirePulled = false;
  mazeWirePulled = false;
  buttonComboWirePulled = false;

  lastFrequencyWirePulled = false;
  lastMazeWirePulled = false;
  lastButtonComboWirePulled = false;
}

void startGame() {
  resetGameModules();
  currentGameState = STATE_RUNNING;
  timerRunning = true;
  timerFinished = false;
  timerLastTick = millis();
  remainingSeconds = TIMER_START_MINUTES * 60;
  activeEncoderModule = BYPASS_FREQUENCY_MODULE ? MAZE_MODULE : FREQUENCY_MODULE;
  updateTimerDisplay();
}

void handleKeySwitch() {
  if (!readKeySwitchDebounced()) return;

  if (keyCurrentState == LOW) {
    // Key turned ON
    if (currentGameState == STATE_IDLE) startGame();

  } else {
    // Key turned OFF
    if (currentGameState == STATE_RUNNING) {
      currentGameState = STATE_FAILED;
      timerRunning = false;
      stopBuzzer();
      if (!endSequencePlayed) {
        playDetonatedSequence(false);
        endSequencePlayed = true;
      }
    } else if (currentGameState == STATE_WON) {
      if (!coreTriggered && !coreSolved) { triggerCoreEvent(); delay(100); }
      currentGameState = STATE_DISARMED;
      timerRunning = false;
      stopBuzzer();
      if (!endSequencePlayed) {
        playDisarmedSequence();
        endSequencePlayed = true;
      }
    }
  }
}

// =============================================================================
// HARDWARE WIRE PROCESSING
// =============================================================================

// Helper to determine which physical pin matches the active target calculation
int getModuleWirePin(int moduleNum) {
  int color = -1;
  if (moduleNum == 1) color = getMod1Color(getSerialDigit4());
  else if (moduleNum == 2) color = getMod2Color(getSerialDigit1());
  else if (moduleNum == 3) color = getMod3Color(getSerialLetter4());
  
  // Color-to-pin architecture: 0->A7, 1->A6, 2->A5, 3->A4, 4->A3
  if (color >= 0 && color <= 4) {
    return A7 - color;
  }
  return -1;
}

void updateWireCutting() {
  if (currentGameState != STATE_RUNNING && currentGameState != STATE_WON) return;

  int mod1TargetPin = getModuleWirePin(1);
  int mod2TargetPin = getModuleWirePin(2);
  int mod3TargetPin = getModuleWirePin(3);

  // Audit all 5 pin paths (A3 through A7). HIGH means disconnected/pulled out.
  for (int color = 0; color <= 4; color++) {
    int currentPin = A7 - color;
    
    if (digitalRead(currentPin) == HIGH) {
      bool isActionValid = false;

      // Case A: Correctly terminating Module 1 wire
      if (currentPin == mod1TargetPin && (!BYPASS_FREQUENCY_MODULE && frequencyModuleSolved) && !frequencyWirePulled) {
        frequencyWirePulled = true;
        isActionValid = true;
        buzzerTone(1100, 150);
      }
      // Case B: Correctly terminating Module 2 wire
      else if (currentPin == mod2TargetPin && (!BYPASS_MAZE_MODULE && mazeSolved) && !mazeWirePulled) {
        mazeWirePulled = true;
        isActionValid = true;
        buzzerTone(1100, 150);
      }
      // Case C: Correctly terminating Module 3 wire
      else if (currentPin == mod3TargetPin && (!BYPASS_BUTTON_COMBO && buttonComboSolved) && !buttonComboWirePulled) {
        buttonComboWirePulled = true;
        isActionValid = true;
        buzzerTone(1100, 150);
      }
      // Case D: Wire was already pulled legally in an earlier sweep
      else if (currentPin == mod1TargetPin && frequencyWirePulled) {
        isActionValid = true;
      }
      else if (currentPin == mod2TargetPin && mazeWirePulled) {
        isActionValid = true;
      }
      else if (currentPin == mod3TargetPin && buttonComboWirePulled) {
        isActionValid = true;
      }

      // Detonation Trigger: Unauthorised or out-of-order loop intervention
      if (!isActionValid) {
        currentGameState = STATE_FAILED;
        timerRunning = false;
        stopBuzzer();
        if (!endSequencePlayed) {
          playDetonatedSequence(false);
          endSequencePlayed = true;
        }
        return;
      }
    }
  }
}

// =============================================================================
// GAME STATE MANAGEMENT
// =============================================================================

void checkModuleTransitionsAndTriggerCore() {
  if (coreTriggered || coreSolved) return;

  // Thermal core spikes trigger strictly after hardware termination verification
  bool anyJustCompleted =
    (!lastFrequencyWirePulled && frequencyWirePulled && !BYPASS_FREQUENCY_MODULE) ||
    (!lastMazeWirePulled && mazeWirePulled && !BYPASS_MAZE_MODULE) ||
    (!lastButtonComboWirePulled && buttonComboWirePulled && !BYPASS_BUTTON_COMBO);

  if (anyJustCompleted && random(100) < 50) triggerCoreEvent();

  lastFrequencyWirePulled = frequencyWirePulled;
  lastMazeWirePulled = mazeWirePulled;
  lastButtonComboWirePulled = buttonComboWirePulled;
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

  stopBuzzer();
  stopVibration();

  matrix.begin();
  matrix.setBrightness(25);  // ~10% — protects power supply
  matrix.show();

  lcd.init();
  lcd.backlight();

  initializeLedBar();

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VIBRATION_PIN, OUTPUT);
  pinMode(KEY_SWITCH_PIN, INPUT_PULLUP);
  pinMode(ENC_LEFT_A, INPUT_PULLUP);
  pinMode(ENC_LEFT_B, INPUT_PULLUP);
  pinMode(ENC_RIGHT_A, INPUT_PULLUP);
  pinMode(ENC_RIGHT_B, INPUT_PULLUP);
  
  // Initialize physical wire ports as Pullup loops
  pinMode(A3, INPUT_PULLUP);
  pinMode(A4, INPUT_PULLUP);
  pinMode(A5, INPUT_PULLUP);
  pinMode(A6, INPUT_PULLUP);
  pinMode(A7, INPUT_PULLUP);
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

  // ── Apply bypass flags ─────────────────────────────────────────────────

  if (BYPASS_FREQUENCY_MODULE) {
    frequencyModuleSolved = true;
    frequencyWirePulled = true;
    lastFrequencyWirePulled = true;
    activeEncoderModule = MAZE_MODULE;
    Serial.println(F("BYPASS: Frequency module"));
  }
  if (BYPASS_MAZE_MODULE) {
    mazeSolved = true;
    mazeWirePulled = true;
    lastMazeWirePulled = true;
    mazeSetupDone = true;
    Serial.println(F("BYPASS: Maze module"));
  }
  if (BYPASS_CORE_MODULE) {
    coreTriggered = true;
    coreSolved = true;
    Serial.println(F("BYPASS: Core module"));
  }
  if (BYPASS_FREQUENCY_MODULE && BYPASS_CORE_MODULE && !BYPASS_MAZE_MODULE) {
    setupMazeModule();
    mazeSetupDone = true;
    Serial.println(F("BYPASS: Maze initialised immediately (Freq + Core bypassed)"));
  }
  if (BYPASS_BUTTON_COMBO) {
    buttonComboSolved = true;
    buttonComboWirePulled = true;
    lastButtonComboWirePulled = true;
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
  updateVibration();
  handleKeySwitch();
  updateGameState();
  updateDisplayState();
  updateCountdown();
  updateBuzzer();

  if (currentGameState != STATE_RUNNING && currentGameState != STATE_WON) return;

  // Process live hardware link infrastructure checks
  updateWireCutting();

  checkModuleTransitionsAndTriggerCore();
  updateCoreModule();

  // Button combo is always interactable, even during Core event
  updateButtonComboModule();

  // All other modules pause while Core is active
  if (!coreTriggered || coreSolved) {
    
    // Explicit lockout override protection to prevent encoder bleed through
    if (frequencyModuleSolved && !frequencyWirePulled) {
      activeEncoderModule = FREQUENCY_MODULE;
    } else if (frequencyModuleSolved && frequencyWirePulled && activeEncoderModule == FREQUENCY_MODULE) {
      activeEncoderModule = MAZE_MODULE;
    }

    if (activeEncoderModule == FREQUENCY_MODULE) {
      updateFrequencyModule();
    } else {
      updateMazeModule();
    }
  }
}