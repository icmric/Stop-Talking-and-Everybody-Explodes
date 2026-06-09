#ifndef FREQUENCY_PRESETS_H
#define FREQUENCY_PRESETS_H

/*
  FrequencyPresets.h
  Five encoder-sequence presets for the Frequency Scrambler module.
  
  Preset is selected by: digitSum % 5

  Each step specifies which encoder (L/R), direction (1=CW / -1=CCW),
  and click count. Click values are in raw encoder ticks (4 ticks per detent).

  Manual reference table:
    sum%5 = 0 → L+3CW,  R-2CCW, L-2CCW
    sum%5 = 1 → R+2CW,  L-3CCW, R+1CW
    sum%5 = 2 → L+4CW,  R-1CCW
    sum%5 = 3 → R+3CW,  L-2CCW, R-1CCW, L+2CW
    sum%5 = 4 → L-1CCW, R+4CW,  L+1CW
*/

struct EncoderSequence {
  char encoder[4];   // 'L', 'R', or ' ' (unused step)
  int  direction[4]; // 1 = CW, -1 = CCW
  int  clicks[4];    // Raw ticks (detents × 4)
  int  stepCount;
};

// Preset 0: L+3CW, R-2CCW, L-2CCW
const EncoderSequence PRESET_0 = {
  {'L', 'R', 'L', ' '},
  { 1,  -1,  -1,   0},
  {12,   8,   8,   0},
  3
};

// Preset 1: R+2CW, L-3CCW, R+1CW
const EncoderSequence PRESET_1 = {
  {'R', 'L', 'R', ' '},
  { 1,  -1,   1,   0},
  { 8,  12,   4,   0},
  3
};

// Preset 2: L+4CW, R-1CCW
const EncoderSequence PRESET_2 = {
  {'L', 'R', ' ', ' '},
  { 1,  -1,   0,   0},
  {16,   4,   0,   0},
  2
};

// Preset 3: R+3CW, L-2CCW, R-1CCW, L+2CW
const EncoderSequence PRESET_3 = {
  {'R', 'L', 'R', 'L'},
  { 1,  -1,  -1,   1},
  {12,   8,   4,   8},
  4
};

// Preset 4: L-1CCW, R+4CW, L+1CW
const EncoderSequence PRESET_4 = {
  {'L', 'R', 'L', ' '},
  {-1,   1,   1,   0},
  { 4,  16,   4,   0},
  3
};

const EncoderSequence* getFrequencySequence(int digitSum) {
  switch (digitSum % 5) {
    case 0: return &PRESET_0;
    case 1: return &PRESET_1;
    case 2: return &PRESET_2;
    case 3: return &PRESET_3;
    case 4: return &PRESET_4;
    default: return &PRESET_0;
  }
}

#endif