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
  randomSeed(analogRead(A4) + analogRead(A5) + micros());
}

String generateSerialNumber() {
  String s = "";
  s += (char)('A' + random(0, 9));      // Position 1: A-I
  for (int i = 0; i < 3; i++)
    s += (char)('A' + random(0, 26));   // Positions 2-4: A-Z
  s += '-';
  for (int i = 0; i < 4; i++)
    s += (char)('0' + random(0, 10));   // Digits 1-4: 0-9
  return s;
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

// Convenience aliases used by DistanceModule
int getSecondToLastDigit() { return getSerialDigit(3); }
int getLastDigit()         { return getSerialDigit(4); }

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