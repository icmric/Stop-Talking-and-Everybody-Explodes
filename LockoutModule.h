#ifndef LOCKOUT_MODULE_H
#define LOCKOUT_MODULE_H

/*
  LockoutModule.h
  Section 4: Lockout Override Module
  
  Intercepts the shared primary red button input once all prior assigned 
  shift tasks are either fully neutralized or explicitly bypassed by management.
  
  Operator must press the button when any digit on the countdown timer displays a '1'.
  Penalty: Input when a '1' is absent = -3s efficiency tax.
*/

#include "SerialNumber.h"

// Reference global assets from the master controller unit
extern int currentDigits[4];
extern const int RED_BUTTON_PIN;
extern void applyPenalty(const char* reason);
extern void buzzerTone(int frequency, unsigned long duration);
extern void buzzerToneWait(int frequency, unsigned long duration, unsigned long waitMs);

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
    
    // Evaluate the live TM1637 display state array for the presence of a '1'
    bool timerContainsOne = (currentDigits[0] == 1 || currentDigits[1] == 1 || currentDigits[2] == 1 || currentDigits[3] == 1);
    
    if (timerContainsOne) {
      lockoutOverrideSolved = true;
      
      // Authoritative 3-note ascending corporate chime. Shift nearly complete.
      buzzerToneWait(750, 100, 120);
      buzzerToneWait(950, 100, 120);
      buzzerTone(1250, 300);
    } else {
      // Levy efficiency penalty tax immediately against the countdown pool
      applyPenalty("LockoutOverrideWrong");
    }
  }
  lastRedState = redState;
}

#endif