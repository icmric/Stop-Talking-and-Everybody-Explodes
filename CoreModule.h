#ifndef CORE_MODULE_H
#define CORE_MODULE_H

/*
  CoreModule.h
  Core Overheating event — triggered randomly after another module completes.

  The player must blow continuously into the microphone for a total of
  5 seconds (accumulated) to cool the core. Detection uses a rolling
  window majority-vote to reject brief/stray signals while still
  tolerating natural breath pauses.

  Hardware:
    - Sound sensor: digital pin (active-LOW — LOW = sound detected, HIGH = quiet)
    - 16×2 I2C LCD: address 0x27 (SDA/SCL)
*/

#include <LiquidCrystal_I2C.h>

extern DIYables_4Digit7Segment_TM1637 tm1637;
extern void setLedLevel(int level);

// ── Timing constants ──────────────────────────────────────────────────────

const int           MIC_SENSOR_PIN      = 7;
const unsigned long MIC_SAMPLE_INTERVAL = 40;   // ms between raw samples
const int           MIC_WINDOW_SIZE     = 30;   // samples in rolling window (30 * 10ms = 300ms window)
const int           MIC_HIGH_THRESHOLD  = 10;   // need at least this many "detected" samples in the window to count as blowing (tune this)

const unsigned long MIC_BREATH_TIMEOUT  = 700;   // Allow up to 600ms silence between breaths
const unsigned long BLOW_DURATION       = 5000;  // Total accumulation needed to solve
const unsigned long FLASH_DURATION      = 3000;  // Alert flash phase length
const unsigned long FLASH_CYCLE         = 500;   // Half-period (2Hz = 500ms on/off)

// ── State ───────────────────────────────────────────────────────────────────

bool coreSolved        = false;
bool coreTriggered     = false;
bool coreFlashing      = false;
bool coreBlowing       = false;
bool coreMessageShown  = false;
bool coreDisplayCleared = false;

unsigned long coreLastMicRead        = 0;
unsigned long micLastDetectionTime   = 0;
unsigned long coreFlashStart         = 0;
unsigned long coreSolvedTime         = 0;
unsigned long coreAccumulatedBlowTime = 0;

// ── Rolling window state ─────────────────────────────────────────────────────

bool micWindow[MIC_WINDOW_SIZE] = {false};
int  micWindowIndex      = 0;
int  micWindowHighCount  = 0;
bool micWindowFilled     = false;

extern LiquidCrystal_I2C lcd;
extern String bombSerialNumber;
extern int currentDigits[4];
extern bool serialNumberDisplayed;

// ── Helpers ───────────────────────────────────────────────────────────────────

bool isCoreFlashing() {
  if (!coreFlashing) return false;
  if (millis() - coreFlashStart >= FLASH_DURATION) {
    coreFlashing = false;
    return false;
  }
  return true;
}

bool getCoreFlashState() {
  return (((millis() - coreFlashStart) / FLASH_CYCLE) % 2) == 0;
}

// Resets the rolling window to all-empty. Call whenever a fresh Core
// event starts, so leftover samples from a previous round can't bleed in.
void resetMicWindow() {
  for (int i = 0; i < MIC_WINDOW_SIZE; i++) micWindow[i] = false;
  micWindowIndex     = 0;
  micWindowHighCount = 0;
  micWindowFilled    = false;
}

// Takes one raw sample, updates the rolling window, and returns whether
// the window currently has enough "detected" samples to count as blowing.
// rawDetected should already be polarity-corrected (true = sound detected).
bool updateMicWindow(bool rawDetected) {
  if (micWindow[micWindowIndex]) micWindowHighCount--;  // remove outgoing sample
  micWindow[micWindowIndex] = !rawDetected;
  if (!rawDetected) micWindowHighCount++;                // add incoming sample

  micWindowIndex++;
  if (micWindowIndex >= MIC_WINDOW_SIZE) {
    micWindowIndex  = 0;
    micWindowFilled = true;
  }

  // Until the window has filled once, judge against samples taken so far
  // rather than waiting the full window duration before responding at all.
  return micWindowHighCount >= MIC_HIGH_THRESHOLD;
}

// ── Public API ────────────────────────────────────────────────────────────────

void setupCoreModule() {
  pinMode(MIC_SENSOR_PIN, INPUT);
  lcd.init();
  lcd.backlight();
  lcd.clear();
}

void triggerCoreEvent() {
  if (coreTriggered || coreSolved) return;

  coreTriggered           = true;
  coreFlashing            = true;
  coreFlashStart          = millis();
  coreMessageShown        = false;
  coreAccumulatedBlowTime = 0;
  coreBlowing             = false;
  coreLastMicRead         = millis();
  micLastDetectionTime    = millis();

  resetMicWindow();

  extern bool mazeSetupDone;
  extern void clearMazeDisplay();
  if (mazeSetupDone) clearMazeDisplay();
}

void updateCoreModule() {
  if (!coreTriggered) return;

  unsigned long now = millis();

  // ── Alert flash phase ──────────────────────────────────────────────────────
  if (coreFlashing) {
    if (now - coreFlashStart < FLASH_DURATION) {
      bool on = getCoreFlashState();
      if (on) {
        lcd.backlight();
        setLedLevel(10);
        int v = currentDigits[0]*1000 + currentDigits[1]*100 + currentDigits[2]*10 + currentDigits[3];
        tm1637.print(v);
      } else {
        lcd.noBacklight();
        setLedLevel(0);
        tm1637.print(0);
      }

      if (!coreMessageShown) {
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("CORE OVERHEAT!");
        lcd.setCursor(0, 1); lcd.print("BLOW TO COOL");
        coreMessageShown = true;
      }
    } else {
      coreFlashing = false;
      lcd.backlight();
      setLedLevel(0);
    }
  }

  // ── Post-solve display clear ───────────────────────────────────────────────
  if (coreSolved) {
    if (!coreDisplayCleared && now - coreSolvedTime >= 2000) {
      lcd.clear();
      coreDisplayCleared    = true;
      serialNumberDisplayed = false;
    }
    return;
  }

  // ── Microphone sampling (fast raw sample, fed into rolling window) ─────────
  if (now - coreLastMicRead < MIC_SAMPLE_INTERVAL) return;
  unsigned long deltaTime = now - coreLastMicRead;
  coreLastMicRead = now;

  // Sensor is active-LOW: LOW = sound detected, HIGH = quiet.
  bool rawDetected = (digitalRead(MIC_SENSOR_PIN) == LOW);
  bool windowDetected = updateMicWindow(rawDetected);

  Serial.println(micWindowHighCount);

  if (windowDetected) {
    micLastDetectionTime = now;
    coreBlowing = true;
    coreAccumulatedBlowTime += deltaTime;  // only grows while the window says "blowing"
  } else if (coreBlowing) {
    if (now - micLastDetectionTime >= MIC_BREATH_TIMEOUT) {
      // Silence too long — give up progress
      coreBlowing             = false;
      coreAccumulatedBlowTime = 0;
      if (coreMessageShown && !coreFlashing) {
        lcd.setCursor(0, 1);
        lcd.print("BLOW TO COOL    ");
      }
    }
    // else: just a short breathing gap — accumulated time stays frozen, not reset
  }

  // ── Progress display & solve check ───────────────────────────────────────
  if (coreBlowing) {
    int secLeft = (BLOW_DURATION - coreAccumulatedBlowTime) / 1000 + 1;

    lcd.setCursor(0, 1);
    lcd.print("KEEP BLOWING: ");
    lcd.print(secLeft);
    lcd.print("s ");

    if (coreAccumulatedBlowTime >= BLOW_DURATION) {
      coreSolved              = true;
      coreSolvedTime          = now;
      coreDisplayCleared      = false;
      coreAccumulatedBlowTime = 0;
      coreBlowing             = false;

      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("CORE STABLE");
      lcd.backlight();
      setLedLevel(0);
    }
  }
}

#endif