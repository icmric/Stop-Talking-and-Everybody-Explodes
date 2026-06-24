#ifndef CORE_MODULE_H
#define CORE_MODULE_H

/*
  CoreModule.h
  Core Overheating event — triggered randomly after another module completes.

  The player must blow continuously into the microphone (MH sound sensor,
  digital HIGH when sound detected) for a total of 5 seconds to cool the core.
  Brief breathing pauses of up to 600ms are tolerated without losing progress.

  While the alert phase is active, all other modules are paused and the LCD,
  4-digit display, and LED bar flash together at ~2Hz.

  Hardware:
    - MH sound sensor: digital pin 7 (HIGH = sound detected)
    - 16×2 I2C LCD: address 0x27 (SDA/SCL)
*/

#include <LiquidCrystal_I2C.h>

extern DIYables_4Digit7Segment_TM1637 tm1637;
extern void setLedLevel(int level);

// ── Timing constants ──────────────────────────────────────────────────────────

const int           MIC_SENSOR_PIN      = 7;
const unsigned long MIC_READ_INTERVAL   = 20;    // Poll rapidly (every 20ms) to catch buzzer silence gaps
const unsigned long MIC_BREATH_TIMEOUT  = 600;   // Allow up to 600ms silence between breaths
const unsigned long BLOW_DURATION       = 5000;  // Total accumulation needed to solve
const unsigned long FLASH_DURATION      = 3000;  // Alert flash phase length
const unsigned long FLASH_CYCLE         = 500;   // Half-period (2Hz = 500ms on/off)

// ── State ─────────────────────────────────────────────────────────────────────

bool coreSolved        = false;
bool coreTriggered     = false;
bool coreFlashing      = false;
bool coreBlowing       = false;
bool coreMessageShown  = false;
bool coreDisplayCleared = false;

unsigned long coreBlowStart          = 0;
unsigned long coreLastMicRead        = 0;
unsigned long micLastDetectionTime   = 0;
unsigned long coreFlashStart         = 0;
unsigned long coreSolvedTime         = 0;
unsigned long coreAccumulatedBlowTime = 0;

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
  return ((( millis() - coreFlashStart) / FLASH_CYCLE) % 2) == 0;
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
      serialNumberDisplayed = false;  // Trigger serial number redisplay
    }
    return;
  }

 // ── Microphone sampling ──────────────────────────────────────────────────
  if (now - coreLastMicRead < MIC_READ_INTERVAL) return;
  unsigned long deltaTime = now - coreLastMicRead;
  coreLastMicRead = now;

  bool micDetected = (digitalRead(MIC_SENSOR_PIN) == HIGH);

  if (micDetected) {
  Serial.print(now);
  Serial.println(" MIC HIGH");
}

  if (micDetected) {
    micLastDetectionTime = now;
    coreBlowing = true;
    coreAccumulatedBlowTime += deltaTime;   // only grows while actually HIGH
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