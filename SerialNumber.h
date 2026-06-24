#ifndef SERIAL_NUMBER_H
#define SERIAL_NUMBER_H

/*
  SerialNumber.h
  Generates and parses the bomb serial number.

  Format: LLLL-DDDD
    L = Letter (position 1 constrained to A-I, positions 2-4 are A-Z)
    D = Digit (0-9)
  Example: "FGKB-3917"

  The global `bombSerialNumber` is defined in ktane.ino.
*/

extern String bombSerialNumber;

// ── Generation ────────────────────────────────────────────────────────────────

// Seed RNG from floating analog pins before calling generateSerialNumber().
void seedRandom() {
  randomSeed(analogRead(A14) + analogRead(A15) + micros());
}

// ── Letter accessors (1-based, positions 1-4) ─────────────────────────────────

char getSerialLetter(int pos) {
  if (pos < 1 || pos > 4 || bombSerialNumber.length() < (unsigned)pos) return '?';
  return bombSerialNumber.charAt(pos - 1);
}
char getSerialLetter1() { return getSerialLetter(1); }
char getSerialLetter2() { return getSerialLetter(2); }
char getSerialLetter3() { return getSerialLetter(3); }
char getSerialLetter4() { return getSerialLetter(4); }

// ── Digit accessors (1-based, positions 1-4 after the hyphen) ─────────────────

int getSerialDigit(int pos) {
  if (pos < 1 || pos > 4) return -1;
  int idx = 5 + (pos - 1);  // "LLLL-" = 5 chars before digits
  if (bombSerialNumber.length() <= (unsigned)idx) return -1;
  char c = bombSerialNumber.charAt(idx);
  if (c < '0' || c > '9') return -1;
  return c - '0';
}
int getSerialDigit1() { return getSerialDigit(1); }
int getSerialDigit2() { return getSerialDigit(2); }
int getSerialDigit3() { return getSerialDigit(3); }
int getSerialDigit4() { return getSerialDigit(4); }

// -- Determine the wire needed to be cut for each module.
int getMod1Color(int digit) {
    if (digit == 0 || digit == 5) return 0; // Red
    if (digit == 1 || digit == 6) return 1; // Blue
    if (digit == 2 || digit == 7) return 2; // Yellow
    if (digit == 3 || digit == 8) return 3; // Green
    return 4;                               // Black
}

int getMod2Color(int digit) {
    if (digit == 0 || digit == 1) return 1; // Blue
    if (digit == 2 || digit == 3) return 2; // Yellow
    if (digit == 4 || digit == 5) return 3; // Green
    if (digit == 6 || digit == 7) return 4; // Black
    return 0;                               // Red
}

int getMod3Color(char letter) {
    if (letter=='A'||letter=='F'||letter=='K'||letter=='P'||letter=='U'||letter=='Z') return 2; // Yellow
    if (letter=='B'||letter=='G'||letter=='L'||letter=='Q'||letter=='V'||letter=='W') return 3; // Green
    if (letter=='C'||letter=='H'||letter=='M'||letter=='R'||letter=='X'||letter=='Y') return 4; // Black
    if (letter=='D'||letter=='I'||letter=='N'||letter=='S'||letter=='T') return 0;             // Red
    return 1;                                                                                   // Blue
}

// Convenience aliases used by DistanceModule
int getSecondToLastDigit() { return getSerialDigit(3); }
int getLastDigit()         { return getSerialDigit(4); }



String generateSerialNumber() {
  String s = "";
  for (int i = 0; i < 4; i++) {
    s += (char)('A' + random(0, 26));
  }
  s += '-';
  for (int i = 0; i < 4; i++) {
    s += (char)('0' + random(0, 10));
  }

  bombSerialNumber = s;
  int mod1 = getMod1Color(getSerialDigit4());
  int mod2 = getMod2Color(getSerialDigit1());
  int mod3 = getMod3Color(getSerialLetter4());

  // mod1 is fixed for the whole process (depends on digit4, never touched below).
  // Resolve mod1 vs mod3 first by re-rolling letter4 specifically.
  while (mod1 == mod3) {
    s[3] = (char)('A' + random(0, 26));
    bombSerialNumber = s;
    mod3 = getMod3Color(getSerialLetter4());
  }

  // Now resolve mod2 against both mod1 and mod3 by re-rolling digit1 specifically.
  while (mod2 == mod1 || mod2 == mod3) {
    s[5] = (char)('0' + random(0, 10));
    bombSerialNumber = s;
    mod2 = getMod2Color(getSerialDigit1());
  }

  return s;
}

// ── Module selectors ──────────────────────────────────────────────────────────
 
// Maze layout: based on second letter, split into 4 groups.
//   A–G → 0,  H–N → 1,  O–U → 2,  V–Z → 3
// Manual table: check serial letter 2, find its group, that's the maze number.
int getMazeIndex() {
  char l = getSerialLetter2();
  if (l <= 'G') return 0;
  if (l <= 'N') return 1;
  if (l <= 'U') return 2;
  return 3;
}

// ── Calculations ──────────────────────────────────────────────────────────────

// Sum of all four digits (0-36).
int getDigitSum() {
  int s = 0;
  for (int i = 1; i <= 4; i++) {
    int d = getSerialDigit(i);
    s += (d >= 0) ? d : 0;
  }
  return s;
}

// Convert first-position letter (A-I) to integer 1-9. Returns 0 for invalid.
int letterToNumber(char letter) {
  if (letter >= 'A' && letter <= 'I') return letter - 'A' + 1;
  return 0;
}

// ── Validation / debug ────────────────────────────────────────────────────────

bool isSerialValid() {
  if (bombSerialNumber.length() != 9) return false;
  for (int i = 0; i < 4; i++)
    if (bombSerialNumber.charAt(i) < 'A' || bombSerialNumber.charAt(i) > 'Z') return false;
  if (bombSerialNumber.charAt(4) != '-') return false;
  for (int i = 5; i < 9; i++)
    if (bombSerialNumber.charAt(i) < '0' || bombSerialNumber.charAt(i) > '9') return false;
  return true;
}

void debugPrintSerial() {
  Serial.print("Serial: ");      Serial.println(bombSerialNumber);
  Serial.print("Letters: ");
  Serial.print(getSerialLetter1()); Serial.print(getSerialLetter2());
  Serial.print(getSerialLetter3()); Serial.println(getSerialLetter4());
  Serial.print("Digits: ");
  Serial.print(getSerialDigit1()); Serial.print(getSerialDigit2());
  Serial.print(getSerialDigit3()); Serial.println(getSerialDigit4());
  Serial.print("Digit sum: ");   Serial.println(getDigitSum());
  Serial.print("Letter1 value: "); Serial.println(letterToNumber(getSerialLetter1()));
}

#endif