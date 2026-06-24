#ifndef LOCKOUT_MODULE_H
#define LOCKOUT_MODULE_H

/*
  LockoutModule.h
  Section 4: Lockout Override Module
  
  Intercepts the shared primary red button input once all prior assigned 
  shift tasks are either fully neutralized or explicitly bypassed by management.
  
  Operator must press the button when any digit on the countdown timer displays
  the third digit of the serial number.
  Penalty: Input when that digit is absent = -3s efficiency tax.
*/

#include "SerialNumber.h"

// Reference global assets from the master controller unit
extern int currentDigits[4];
extern const int RED_BUTTON_PIN;
extern void applyPenalty(const char* reason);
extern void buzzerTone(int frequency, unsigned long duration);
extern void buzzerToneWait(int frequency, unsigned long duration, unsigned long waitMs);
extern void playStepConfirmTone();
extern void playModuleCompleteTone();

// Track institutional compliance parameters across module profiles
extern const bool BYPASS_FREQUENCY_MODULE;
extern const bool BYPASS_MAZE_MODULE;
extern const bool BYPASS_CORE_MODULE;
extern const bool BYPASS_BUTTON_COMBO;
extern const bool BYPASS_LOCKOUT_MODULE;

extern bool frequencyModuleSolved;
extern bool frequencyWirePulled;
extern bool mazeSolved;
extern bool mazeWirePulled;
extern bool buttonComboSolved;
extern bool buttonComboWirePulled;
extern bool coreTriggered;
extern bool coreSolved;

// Localized module status flags
bool lockoutOverrideSolved = false;

// Tier A: Audit whether prior tasks are cleared or bypassed before allowing override access
bool priorModulesSolved() {
  if (!BYPASS_FREQUENCY_MODULE && !(frequencyModuleSolved && frequencyWirePulled)) return false;
  if (!BYPASS_MAZE_MODULE && !(mazeSolved && mazeWirePulled)) return false;
  if (!BYPASS_BUTTON_COMBO && !(buttonComboSolved && buttonComboWirePulled)) return false;
  
  // Administrative Gate: Block input if a core thermal runaway event is actively screaming
  if (!BYPASS_CORE_MODULE && (coreTriggered && !coreSolved)) return false; 
  
  return true;
}

void setupLockoutModule() {
  lockoutOverrideSolved = false;
}

void updateLockoutModule() {
  // Exit early if the task is already archived or management skipped it
  if (lockoutOverrideSolved || BYPASS_LOCKOUT_MODULE) return;
  if (!priorModulesSolved()) return; 

  unsigned long now = millis();
  static int lastRedState = HIGH;
  static unsigned long lastRedPress = 0;
  const unsigned long debounceDelay = 50;

  int redState = digitalRead(RED_BUTTON_PIN);
  
  // Detect crisp mechanical button depression loop
  if (redState == LOW && lastRedState == HIGH && (now - lastRedPress > debounceDelay)) {
    lastRedPress = now;
    
    // Evaluate the live TM1637 display for the serial's third digit
    int targetDigit = getSerialDigit3();
    bool timerContainsTarget = (currentDigits[0] == targetDigit || currentDigits[1] == targetDigit ||
                                currentDigits[2] == targetDigit || currentDigits[3] == targetDigit);
    
    if (timerContainsTarget) {
      lockoutOverrideSolved = true;
      
      playModuleCompleteTone();
    } else {
      // Levy efficiency penalty tax immediately against the countdown pool
      applyPenalty("LockoutOverrideWrong");
    }
  }
  lastRedState = redState;
}

#endif