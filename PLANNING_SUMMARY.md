# KTANE Implementation Planning Summary

## Changes from Original Specs

### 1. **Penalty System Replacement**
**Old:** 3-strike system (3 strikes = explosion/game over)  
**New:** Penalty-based time reduction system

**Details:**
- No strike tracking needed
- Wrong action → -3 seconds from timer immediately
- Buzzer tone plays on penalty (1kHz, 200ms)
- Game continues after penalty
- If timer would go negative, game ends
- Much simpler to implement and debug

**Penalties Triggered By:**
- Encoder Tuning: Wrong encoder turn OR wrong direction
- Maze: Hitting a wall
- Button Combo: Wrong button press
- Compass: Holding wrong direction

---

## Critical Design Decisions Needed (Before Implementation)

### 1. **Encoder Tuning Sequence Selection** ⚠️ DECISION REQUIRED
You have two options:

**Option A: 5 Fixed Presets**
- Simpler to implement
- Predetermined sequences (5 options)
- Selected based on some serial digit
- Example:
  - If digit sum % 5 == 0: Use sequence 1
  - If digit sum % 5 == 1: Use sequence 2
  - etc.

**Option B: Dynamic Generation**
- More complex but more variety
- Generate unique sequence from serial number
- Example:
  - Parse first 4 digits: A=1, B=2, ..., I=9, J-Z=10
  - Use these values to construct encoder steps
  - Create different sequence each time

**Constraint:** Maximum 4 total steps in sequence (3-4 encoder turns maximum)  
**Goal:** "Relatively easy to understand but not too easy"

**Recommendation:** Start with Option A (5 presets), can always upgrade to Option B later
- **Request:** Use option A

---

### 2. **Distance Module Serial Derivation** ⚠️ DECISION REQUIRED

Current implementation has fixed distance (10-15cm) and fixed hold time (3s).  
Need to derive both from serial number.

**Examples for consideration:**
- Distance: 2nd-to-last digit × 10 = required distance (20-90cm)
- Hold time: Last digit × 0.5 = seconds (0-4.5 seconds)

**Example with serial ABC1-2345:**
- 2nd-to-last digit = 4 → distance = 40cm
- Last digit = 5 → hold time = 2.5 seconds

**Constraints:**
- Should be deducible by someone reading manual
- Reasonable physical distances (10-100cm range)
- Reasonable hold times (0.5-5 seconds)

**Your choice:** Use suggested formula above or propose different mapping?
- **Request:** Use the above formulas

---

### 3. **Button Combo Sequence Derivation** ⚠️ DECISION REQUIRED

Currently hardcoded as: Red → Red → Green

Options:
- Map serial letters to button patterns
- Map serial digits to button patterns
- Combine multiple serial elements

**Example approach (not required):**
- A-D = Sequence 1
- E-H = Sequence 2
- I, J = Sequence 3
- 0-3 digits = Sequence 4
- 4-9 digits = Sequence 5

**Constraint:** Maximum 5 buttons (3-5 button presses reasonable)

**Your choice:** Fixed presets or dynamic generation?
- **Request:** How about instead we use the 4 numbers - there will always be 4 button presses, if its even then press red, odd press green

---

## Module-by-Module Implementation Plan

### Module 1: Timer with Red Button (Priority: HIGH)
**Current Issue:** Red LED lights up when digit '1' visible, but button pressing is unclear  
**Action:** Clarify current behavior and intended behavior
- Does pressing button while lit complete sub-module?
- Does it pause? Resume? Do nothing?
- Use hardware interrupt for button or polling in loop?

**Recommendations:**
- System interrupt approach = best for time-critical response
- Attach interrupt on D11 to detect button press
- When digit '1' visible AND button pressed = sub-module solved
- Only one completion per game session

---

### Module 2: Core Overheating (Priority: HIGH)
**Changes Required:**
1. Implement random triggering (Option B: After ≥1 module solved, probabilistic)
2. Add synchronized flashing:
   - LCD backlight (already exists)
   - 4-digit timer display (NEW)
   - LED bar (NEW)
   - Frequency: ~2Hz (500ms ON, 500ms OFF)

**Implementation:**
```
if (coreActive && millis() - coreStartTime) % 500 < 250:
  flashOn = true  // LED bar + timer both ON
else:
  flashOn = false // Both OFF
```

---

### Module 3: Encoder Tuning (Priority: CRITICAL)
**Changes Required:**
1. Derive sequence from serial number (5 presets OR dynamic)
2. Implement variable-length sequences (support 1-4 steps)
3. Add penalty: wrong encoder/direction = -3 seconds

**Current Sequence:** L+3CW, R+2CCW, L+2CCW (3 steps)

**TBD:** How to generate new sequences? (await your decision)

**Integration Point:** Call `applyPenalty("Encoder")` when error detected

---

### Module 4: Maze (Priority: HIGH)
**Changes Required:**
1. Keep current maze layout (matches manual)
2. Detect wall collisions (currently may not be detecting)
3. Add penalty: wall hit = -3 seconds

**Current Logic:** Encoders move player, RGB matrix displays position

**TBD:** Verify collision detection is working

**Integration Point:** Call `applyPenalty("Maze")` when wall detected

---

### Module 5: Distance (Priority: CRITICAL)
**Changes Required:**
1. Derive required distance from serial (awaiting your formula decision)
2. Derive required hold time from serial (awaiting your formula decision)
3. Keep current ultrasonic sensor and LED bar feedback

**Example Implementation:**
```cpp
// Based on serial number: ABC1-2345
char lastTwoDigits = "45";
int requiredDistance = (lastTwoDigits[0] - '0') * 10;  // 40cm
float requiredHoldTime = (lastTwoDigits[1] - '0') * 0.5;  // 2.5s
```

**No penalty on this module** (specs don't mention it)

---

### Module 6: Button Combo (Priority: CRITICAL)
**Changes Required:**
1. Derive sequence from serial number (awaiting your decision)
2. Support variable-length sequences
3. Add penalty: wrong button = -3 seconds + buzzer tone
4. Make completable only AFTER another module (not immediately)

**Current Sequence:** Red → Red → Green (hardcoded)

**TBD:** Sequence derivation logic (await your decision)

**Integration Point:** 
- Call `applyPenalty("ButtonCombo")` on wrong press
- Check module dependency before allowing input

---

### Module 7: Compass Alignment (Priority: CRITICAL - NOT INTEGRATED)
**Current Status:** Code exists in `signalAlignment.h` but not called in main loop

**Changes Required:**
1. Add to main game loop's module update sequence
2. Implement stage logic:
   - **Stage 1:** If sum(4 digits) % 2 == 0 → North; else South
   - **Stage 2:** If firstLetter.toNumber() == lastDigit → North; else South
3. Use LED bar for directional feedback (instead of audio)
4. Add penalty: wrong direction hold = -3 seconds
5. Require 5-second hold per stage

**LED Bar Feedback:**
- Direction North: LED bar fills left-to-right OR top half
- Direction South: LED bar fills right-to-left OR bottom half
- Magnitude: How close to correct direction

**Example Logic for Letter-to-Number:**
```
A=1, B=2, C=3, D=4, E=5, F=6, G=7, H=8, I=9
(J-Z not valid - enforced in SerialNumberGenerator)
```

**Integration Point:**
- Call `applyPenalty("Compass")` on wrong hold
- Add compass module to `allModulesSolved()` check

---

## Shared Functions Needed

### 1. **Penalty Function**
```cpp
void applyPenalty(const char* reason) {
  // Play tone: 1kHz for 200ms
  tone(BUZZER_PIN, 1000, 200);
  
  // Reduce timer
  if (remainingTime > 3000) {
    remainingTime -= 3000;
  } else {
    remainingTime = 0;
    // Trigger game over
  }
}
```

### 2. **Serial Number Parser**
```cpp
// Helper functions needed:
char getSerialLetter1();     // Return 'A'-'I'
char getSerialLetter2();     // Return 'A'-'Z'
char getSerialLetter3();     // Return 'A'-'Z'
char getSerialLetter4();     // Return 'A'-'Z'
int  getSerialDigit1();      // Return 0-9
int  getSerialDigit2();      // Return 0-9
int  getSerialDigit3();      // Return 0-9
int  getSerialDigit4();      // Return 0-9
int  getDigitSum();          // Return sum of 4 digits
int  letterToNumber(char);   // A=1, B=2, ..., I=9
```

### 3. **Module Control Helpers**
```cpp
// Check if module dependency is met before allowing input
bool isModuleReadyToStart(ModuleName module);

// For Red Button interrupt handler
void handleRedButtonPress();
```

---

## Serial Number Examples & Logic

### Example 1: Serial `ABC1-2345`
- Letters: A, B, C
- Digits: 1, 2, 3, 4, 5
- **Compass Stage 1:** 1+2+3+4=10 (even) → North
- **Compass Stage 2:** A=1, last digit=5, 1≠5 → South
- **Distance:** 2nd-to-last digit=4 → 40cm (if using × 10)

### Example 2: Serial `DEF2-3456`
- Letters: D, E, F
- Digits: 2, 3, 4, 5, 6
- **Compass Stage 1:** 2+3+4+5=14 (even) → North
- **Compass Stage 2:** D=4, last digit=6, 4≠6 → South
- **Distance:** 2nd-to-last digit=5 → 50cm

### Example 3: Serial `HIB7-8901`
- Letters: H, I, B
- Digits: 7, 8, 9, 0, 1
- **Compass Stage 1:** 7+8+9+0=24 (even) → North
- **Compass Stage 2:** H=8, last digit=1, 8≠1 → South
- **Distance:** 2nd-to-last digit=0 → 0cm (EDGE CASE! Needs handling)

---

## Testing Strategy

### Unit Testing (per module):
1. **Penalty System:** 
   - Verify timer reduces by 3000ms
   - Verify buzzer sounds
   - Verify game ends if timer goes negative

2. **Serial Parsing:**
   - Test with known serials (ABC1-2345, DEF6-7890, HIJ0-0001)
   - Verify each digit/letter parsed correctly
   - Verify digit sum calculated correctly

3. **Compass Logic:**
   - Test Stage 1 with even/odd digit sums
   - Test Stage 2 with matching/non-matching letter-digit pairs
   - Test all 9 possible first letters (A-I)

4. **Encoder Sequences:**
   - Test each of the 5 presets (if using Option A)
   - Verify sequence length doesn't exceed 4
   - Verify wrong turn triggers penalty

5. **Distance Derivation:**
   - Test formula with edge cases (digit=0, digit=9)
   - Verify distance is in reasonable range (10-100cm)
   - Verify hold time is reasonable (0.5-5 seconds)

### Integration Testing:
1. Start game with key switch
2. Test module execution order
3. Test penalties don't cascade (only -3s per penalty)
4. Test game-over on timer exhaustion
5. Test all modules in sequence
6. Test Core randomization (trigger at different times)

---

## Open Questions for You

1. **Encoder Tuning:** 5 fixed presets (Option A) or dynamic generation (Option B)?
2. **Distance Formula:** Use suggested × 10 mapping or different approach?
3. **Button Combo:** Fixed presets or dynamic sequence generation?
4. **Red Button:** Current behavior (pause/nothing) vs intended (complete module)?
5. **Distance Edge Case:** When digit=0, resulting distance=0cm - acceptable?
6. **Module Dependencies:** Which modules must be completed before Button Combo becomes active?

---

## Files to Modify

When implementation begins, these files will need changes:

1. **ktane.ino** - Add penalty function, Core randomization, Red Button interrupt
2. **FrequencyModule.h** - Add serial derivation, penalty on wrong turn
3. **MazeModule.h** - Verify collision detection, add penalty
4. **DistanceModule.h** - Add serial derivation logic
5. **ButtonComboModule.h** - Add serial derivation, penalty, module dependency check
6. **CoreModule.h** - Add synchronized flashing
7. **signalAlignment.h** - Complete stage logic, integrate into loop
8. **New file needed:** Serial parser helpers or expand SerialNumberGenerator.h

---

## Next Steps

1. **Review this planning document**
2. **Answer the 6 open questions above**
3. **Finalize sequence/distance/button derivation logic**
4. **I will then create implementation code**

This document provides the architectural foundation - once decisions are made, implementation should proceed smoothly!
