#ifndef FREQUENCY_PRESETS_H
#define FREQUENCY_PRESETS_H

/*
  Encoder Tuning Module - Sequence Presets
  
  5 predetermined sequences, selected based on serial number.
  Serial number digits determine which preset is used:
  - digitSum % 5 determines the sequence (0-4)
  
  Each sequence is defined as:
  - ENCODER: 'L' (left encoder) or 'R' (right encoder)
  - DIRECTION: 1 (clockwise) or -1 (counterclockwise)
  - CLICKS: Number of clicks/rotations needed
  - STEPS: Total number of steps in this sequence
  
  How manual operator deduces the preset:
  1. Generate serial number (ABCD-1234)
  2. Sum the 4 digits: 1+2+3+4 = 10
  3. Divide by 5 and take remainder: 10 % 5 = 0 → Preset 0
  4. Look up the sequence from the reference table
  
  Reference for Manual (to be printed):
  ┌──────────────────────────────────────────────┐
  │  ENCODER TUNING SEQUENCE SELECTOR             │
  │  (Digit Sum mod 5)                            │
  ├──────────────────────────────────────────────┤
  │ If sum % 5 = 0: L+3CW, R-2CCW, L-2CCW        │
  │ If sum % 5 = 1: R+2CW, L-3CCW, R+1CW         │
  │ If sum % 5 = 2: L+4CW, R-1CCW                │
  │ If sum % 5 = 3: R+3CW, L-2CCW, R-1CCW, L+2CW │
  │ If sum % 5 = 4: L-1CCW, R+4CW, L+1CW         │
  └──────────────────────────────────────────────┘
  
  CW = Clockwise (positive)
  CCW = Counterclockwise (negative)
*/

struct EncoderSequence {
  char encoder[4];      // 'L' or 'R' for each step
  int direction[4];     // 1 for CW, -1 for CCW
  int clicks[4];        // Number of clicks for each step
  int stepCount;        // Total steps (1-4)
};

// Preset 0: L+3CW, R-2CCW, L-2CCW (3 steps)
const EncoderSequence PRESET_0 = {
  {'L', 'R', 'L', ' '},
  {1, -1, -1, 0},
  {3, 2, 2, 0},
  3
};

// Preset 1: R+2CW, L-3CCW, R+1CW (3 steps)
const EncoderSequence PRESET_1 = {
  {'R', 'L', 'R', ' '},
  {1, -1, 1, 0},
  {2, 3, 1, 0},
  3
};

// Preset 2: L+4CW, R-1CCW (2 steps)
const EncoderSequence PRESET_2 = {
  {'L', 'R', ' ', ' '},
  {1, -1, 0, 0},
  {4, 1, 0, 0},
  2
};

// Preset 3: R+3CW, L-2CCW, R-1CCW, L+2CW (4 steps - maximum)
const EncoderSequence PRESET_3 = {
  {'R', 'L', 'R', 'L'},
  {1, -1, -1, 1},
  {3, 2, 1, 2},
  4
};

// Preset 4: L-1CCW, R+4CW, L+1CW (3 steps)
const EncoderSequence PRESET_4 = {
  {'L', 'R', 'L', ' '},
  {-1, 1, 1, 0},
  {1, 4, 1, 0},
  3
};

// =====================================================
// SEQUENCE SELECTOR
// =====================================================

/**
 * Get the appropriate sequence preset based on serial number
 * @param digitSum Sum of the 4 serial number digits
 * @return Pointer to the appropriate EncoderSequence preset
 */
const EncoderSequence* getFrequencySequence(int digitSum) {
  int presetIndex = digitSum % 5;
  
  switch (presetIndex) {
    case 0:
      return &PRESET_0;
    case 1:
      return &PRESET_1;
    case 2:
      return &PRESET_2;
    case 3:
      return &PRESET_3;
    case 4:
      return &PRESET_4;
    default:
      return &PRESET_0;  // Fallback
  }
}

#endif
