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
    D11      Blue button (INPUT_PULLUP)
    D12      Vibration motor (via transistor; active HIGH)
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
#include "LockoutModule.h"

// ── Debug / bypass flags ───────────────────────────────────────────────────
// Set to true to skip a module during development/testing.

const bool BYPASS_FREQUENCY_MODULE = false;
const bool BYPASS_MAZE_MODULE = false;
const bool BYPASS_CORE_MODULE = false;
const bool BYPASS_BUTTON_COMBO = false;
const bool BYPASS_LOCKOUT_MODULE = false;

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

const int TIMER_START_MINUTES  = 6;
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
  if (!BYPASS_LOCKOUT_MODULE && !lockoutOverrideSolved) return false;
  return true;
}

int getTotalModuleCount() {
  int n = 0;
  if (!BYPASS_FREQUENCY_MODULE) n++;
  if (!BYPASS_MAZE_MODULE) n++;
  if (!BYPASS_CORE_MODULE) n++;
  if (!BYPASS_BUTTON_COMBO) n++;
  if (!BYPASS_LOCKOUT_MODULE) n++;
  return n;
}

int getSolvedModuleCount() {
  int n = 0;
  if (!BYPASS_FREQUENCY_MODULE && frequencyModuleSolved && frequencyWirePulled) n++;
  if (!BYPASS_MAZE_MODULE && mazeSolved && mazeWirePulled) n++;
  if (!BYPASS_CORE_MODULE && coreSolved) n++;
  if (!BYPASS_BUTTON_COMBO && buttonComboSolved && buttonComboWirePulled) n++;
  if (!BYPASS_LOCKOUT_MODULE && lockoutOverrideSolved) n++;
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
  
  // Block vibrations during core overheating to avoid microphone picking up stray vibrations
  if (currentGameState == STATE_RUNNING && coreMessageShown && !coreSolved) {
    return; 
  }
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
    
    // Determine if the buzzer should be actively making sound right now
    bool shouldBeOn = (cycleTime < 200 || (cycleTime >= 250 && cycleTime < 450) || (cycleTime >= 500 && cycleTime < 700));
    
    static bool lastCoreSoundActive = false;
    
    if (shouldBeOn) {
      if (!lastCoreSoundActive) {
        buzzerTone(1500, 200); // Fires ONCE at the exact start of a beep window
        lastCoreSoundActive = true;
      }
    } else {
      if (lastCoreSoundActive) {
        stopBuzzer(); // Fires ONCE at the exact start of a silent window
        lastCoreSoundActive = false;
      }
    }
    return;
  }

  if (!timerRunning || timerFinished || remainingSeconds <= 0) {
    stopBuzzer();
    return;
  }

  unsigned long now = millis();

  if (remainingSeconds <= 15) {
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
      
      if (currentGameState == STATE_RUNNING || currentGameState == STATE_WON) {
        currentGameState = STATE_FAILED;
        
        // FIX: Trigger the custom expanding explosion sequence upon running out of time
        if (!endSequencePlayed) {
          playDetonatedSequence(true); // Passes timedOut = true for the correct ending text
          endSequencePlayed = true;
        }
      }
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

  // Smoothly charge up the LED bar level
  for (int i = 1; i <= 10; i++) { 
    setLedLevel(i); 
    delay(60); 
  }

  // Cinematic Green Ripple Wave expanding outward from the center
  // dx and dy calculate the distance squared from the center 4 pixels using integer math
  for (int wave = 0; wave < 5; wave++) {
    // Satisfying ascending laser arpeggio paired with each wave step
    tone(BUZZER_PIN, 400 + (wave * 250), 60);
    
    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 8; x++) {
        int dx = 2 * x - 7;
        int dy = 2 * y - 7;
        int distSq = dx * dx + dy * dy;
        int pixelIdx = y * 8 + x;

        if (wave == 0 && distSq <= 2)   matrix.setPixelColor(pixelIdx, 0x00FF00); // Ring 0 (Center)
        if (wave == 1 && distSq == 18)  matrix.setPixelColor(pixelIdx, 0x00FF00); // Ring 1
        if (wave == 2 && distSq == 50)  matrix.setPixelColor(pixelIdx, 0x00FF00); // Ring 2
        if (wave == 3 && distSq >= 98)  matrix.setPixelColor(pixelIdx, 0x00FF00); // Ring 3 (Corners)
      }
    }
    matrix.show();
    delay(100);
    
    // Dim the trailing edge of the wave for a smooth ripple effect
    for(int i = 0; i < 64; i++) {
      uint32_t c = matrix.getPixelColor(i);
      if (c == 0x00FF00) matrix.setPixelColor(i, 0x003300); 
    }
  }
  noTone(BUZZER_PIN);

  // Bring the whole matrix up to a warm, triumphant solid green pulse
  for (int pulse = 0; pulse < 3; pulse++) {
    for (int i = 0; i < 64; i++) matrix.setPixelColor(i, 0x00FF00);
    matrix.show();
    delay(250);
    for (int i = 0; i < 64; i++) matrix.setPixelColor(i, 0x003300);
    matrix.show();
    delay(200);
  }

  // Clear display down to a pristine off state
  for (int i = 0; i < 64; i++) matrix.setPixelColor(i, 0x000000);
  matrix.show();

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("BOMB DISARMED");
  updateModuleCountDisplay();
}

void playDetonatedSequence(bool timedOut) {
  startVibration(); // Initiate continuous haptic rumble framework

  // Multi-Stage Expanding Plasma Fireball Animation
  // Steps iterate outward through concentric distance thresholds
  for (int step = 0; step < 8; step++) {
    // Generate a violent, chaotic explosion crackle sound pitch down
    tone(BUZZER_PIN, random(1800 - (step * 200), 2400 - (step * 200)), 40);

    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 8; x++) {
        int dx = 2 * x - 7;
        int dy = 2 * y - 7;
        int distSq = dx * dx + dy * dy;
        int pixelIdx = y * 8 + x;

        // Map colors dynamically relative to the expanding shockwave front
        if (distSq <= 2) { // Ring 0 (Center core)
          if (step < 2) matrix.setPixelColor(pixelIdx, 0xFFFFFF);      // Blinding white flash
          else if (step < 4) matrix.setPixelColor(pixelIdx, 0xFFCC00); // Hot yellow
          else if (step < 6) matrix.setPixelColor(pixelIdx, 0xFF4500); // Fire orange
          else matrix.setPixelColor(pixelIdx, 0x440000);               // Cooling ash
        } 
        else if (distSq > 2 && distSq <= 18) { // Ring 1
          if (step == 1) matrix.setPixelColor(pixelIdx, 0xFFFFFF);
          else if (step < 4) matrix.setPixelColor(pixelIdx, 0xFFCC00);
          else if (step < 6) matrix.setPixelColor(pixelIdx, 0xFF4500);
          else matrix.setPixelColor(pixelIdx, 0x330000);
        } 
        else if (distSq > 18 && distSq <= 50) { // Ring 2
          if (step == 2) matrix.setPixelColor(pixelIdx, 0xFFFFFF);
          else if (step == 3) matrix.setPixelColor(pixelIdx, 0xFFCC00);
          else if (step < 6) matrix.setPixelColor(pixelIdx, 0xFF4500);
          else matrix.setPixelColor(pixelIdx, 0x110000);
        } 
        else { // Ring 3 (Outer edge bounds)
          if (step == 3) matrix.setPixelColor(pixelIdx, 0xFFFFFF);
          else if (step == 4) matrix.setPixelColor(pixelIdx, 0xFFCC00);
          else if (step == 5) matrix.setPixelColor(pixelIdx, 0xFF4500);
          else if (step == 6) matrix.setPixelColor(pixelIdx, 0xFF0000);
          else matrix.setPixelColor(pixelIdx, 0x000000);
        }
      }
    }
    matrix.show();
    delay(60);
  }
  noTone(BUZZER_PIN);

  // Dissipation phase: Fiery residual sparks twinkling and cooling down to empty
  for (int fade = 0; fade < 12; fade++) {
    for (int i = 0; i < 64; i++) {
      uint32_t c = matrix.getPixelColor(i);
      if (c > 0 && random(100) < 35) {
        matrix.setPixelColor(i, 0x000000); // Pixel burns completely out
      } else if (c == 0xFF4500 || c == 0xFFCC00) {
        matrix.setPixelColor(i, 0x330000); // Cool down to dim deep red
      }
    }
    matrix.show();
    delay(70);
  }

  // Clear total matrix array layout completely before warning strobes
  for (int i = 0; i < 64; i++) matrix.setPixelColor(i, 0x000000);
  matrix.show();

  // Violent Post-Explosion Emergency Status Strobes
  for (int i = 0; i < 6; i++) {
    setLedLevel(10);
    for (int j = 0; j < 64; j++) matrix.setPixelColor(j, 0xFF0000); // Pure emergency red warning
    matrix.show();
    
    // Screaming low-frequency building layout alert tone
    buzzerTone(i % 2 == 0 ? 120 : 90, 50);
    
    unsigned long flashOn = millis() + 60;
    while (millis() < flashOn) updateVibration();
    
    setLedLevel(0);
    for (int j = 0; j < 64; j++) matrix.setPixelColor(j, 0x000000);
    matrix.show();
    noTone(BUZZER_PIN);
    
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
  // Enforce bypass configuration states directly into runtime tracking
  frequencyModuleSolved   = BYPASS_FREQUENCY_MODULE;
  frequencyWirePulled     = BYPASS_FREQUENCY_MODULE;
  lastFrequencyWirePulled = BYPASS_FREQUENCY_MODULE;

  mazeSolved              = BYPASS_MAZE_MODULE;
  mazeWirePulled          = BYPASS_MAZE_MODULE;
  lastMazeWirePulled      = BYPASS_MAZE_MODULE;
  mazeSetupDone           = BYPASS_MAZE_MODULE;
  mazeDisplayCleared      = false;

  coreTriggered           = BYPASS_CORE_MODULE;
  coreSolved              = BYPASS_CORE_MODULE;
  buttonComboSolved       = BYPASS_BUTTON_COMBO;
  buttonComboSolved       = BYPASS_BUTTON_COMBO;
  buttonComboWirePulled   = BYPASS_BUTTON_COMBO;
  lastButtonComboWirePulled = BYPASS_BUTTON_COMBO;

  endSequencePlayed       = false;
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
      if (!BYPASS_CORE_MODULE && !coreTriggered && !coreSolved) { triggerCoreEvent(); delay(100); }
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
  if (BYPASS_CORE_MODULE || coreTriggered || coreSolved) return;

  // Thermal core spikes trigger strictly after hardware termination verification
  bool anyJustCompleted =
    (!lastFrequencyWirePulled && frequencyWirePulled && !BYPASS_FREQUENCY_MODULE) ||
    (!lastMazeWirePulled && mazeWirePulled && !BYPASS_MAZE_MODULE) ||
    (!lastButtonComboWirePulled && buttonComboWirePulled && !BYPASS_BUTTON_COMBO);

  if (anyJustCompleted) {
    // 1. Calculate how many puzzle modules are actually enabled in this game setup
    int totalPriorModules = 0;
    if (!BYPASS_FREQUENCY_MODULE) totalPriorModules++;
    if (!BYPASS_MAZE_MODULE) totalPriorModules++;
    if (!BYPASS_BUTTON_COMBO) totalPriorModules++;

    // 2. Count how many wires have been successfully severed so far
    int currentPulled = 0;
    if (!BYPASS_FREQUENCY_MODULE && frequencyWirePulled) currentPulled++;
    if (!BYPASS_MAZE_MODULE && mazeWirePulled) currentPulled++;
    if (!BYPASS_BUTTON_COMBO && buttonComboWirePulled) currentPulled++;

    // 3. Fallback Check: If we are at (or past) the second-to-last module, force it.
    // Otherwise, fallback to the standard 75% probability roll.
    if (currentPulled >= (totalPriorModules - 1) || random(100) < 75) {
      triggerCoreEvent();
    }
  }

  lastFrequencyWirePulled = frequencyWirePulled;
  lastMazeWirePulled = mazeWirePulled;
  lastButtonComboWirePulled = buttonComboWirePulled;
}

void updateGameState() {
  if (currentGameState == STATE_RUNNING && allModulesSolved()) {
    if (!BYPASS_CORE_MODULE && !coreTriggered && !coreSolved) triggerCoreEvent();
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
  Serial.print("Wires: ");
  Serial.print(digitalRead(A3));
  Serial.print(digitalRead(A4));
  Serial.print(digitalRead(A5));
  Serial.print(digitalRead(A6));
  Serial.println(digitalRead(A7));

  keyCurrentState = digitalRead(KEY_SWITCH_PIN);
  keyLastState    = keyCurrentState;

  Serial.println("Read Key State");

  seedRandom();
  bombSerialNumber = generateSerialNumber();
  Serial.println("Generated Serial Number");

  tm1637.begin();
  tm1637.printTime(88, 88, true);  // "88:88" splash

  setupCoreModule();
  setupFrequencyModule();
  setupButtonComboModule();
  clearMazeDisplay();

  // ── Apply bypass flags ─────────────────────────────────────────────────

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
  updateLockoutModule();

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