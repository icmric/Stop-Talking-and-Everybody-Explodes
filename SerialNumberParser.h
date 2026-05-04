#ifndef SERIAL_NUMBER_PARSER_H
#define SERIAL_NUMBER_PARSER_H

/*
  Serial Number Parser for KTANE Bomb
  
  Provides utility functions to extract and process components from the serial number.
  Serial format: ABCD-1234
  - Position 0: Letter A (constrained A-I)
  - Position 1: Letter B (A-Z)
  - Position 2: Letter C (A-Z)
  - Position 3: Letter D (A-Z)
  - Position 4: '-' (separator)
  - Position 5-8: Digits 1-4 (0-9)
  
  External Declaration:
  The global serial number string is declared as 'extern String bombSerialNumber'
  (defined in ktane.ino)
*/

extern String bombSerialNumber;

// ==========================================
// LETTER ACCESSORS
// ==========================================

/**
 * Get the letter at specified position (1-4)
 * @param position 1-4 (1 = first letter)
 * @return Character 'A'-'Z', or '?' if invalid
 */
char getSerialLetter(int position) {
  if (position < 1 || position > 4) return '?';
  if (bombSerialNumber.length() < position) return '?';
  return bombSerialNumber.charAt(position - 1);
}

/**
 * Get the first letter specifically (position 1)
 * @return Character 'A'-'I'
 */
char getSerialLetter1() {
  return getSerialLetter(1);
}

/**
 * Get the second letter (position 2)
 * @return Character 'A'-'Z'
 */
char getSerialLetter2() {
  return getSerialLetter(2);
}

/**
 * Get the third letter (position 3)
 * @return Character 'A'-'Z'
 */
char getSerialLetter3() {
  return getSerialLetter(3);
}

/**
 * Get the fourth letter (position 4)
 * @return Character 'A'-'Z'
 */
char getSerialLetter4() {
  return getSerialLetter(4);
}

// ==========================================
// DIGIT ACCESSORS
// ==========================================

/**
 * Get the digit at specified position (1-4)
 * Position 1 = first digit after hyphen
 * @param position 1-4
 * @return Integer 0-9, or -1 if invalid
 */
int getSerialDigit(int position) {
  if (position < 1 || position > 4) return -1;
  // Serial format: ABCD-1234
  // Digits start at index 5
  int charIndex = 5 + (position - 1);
  if (bombSerialNumber.length() <= charIndex) return -1;
  char digitChar = bombSerialNumber.charAt(charIndex);
  if (digitChar < '0' || digitChar > '9') return -1;
  return (int)(digitChar - '0');
}

/**
 * Get the first digit
 * @return Integer 0-9
 */
int getSerialDigit1() {
  return getSerialDigit(1);
}

/**
 * Get the second digit
 * @return Integer 0-9
 */
int getSerialDigit2() {
  return getSerialDigit(2);
}

/**
 * Get the third digit
 * @return Integer 0-9
 */
int getSerialDigit3() {
  return getSerialDigit(3);
}

/**
 * Get the fourth (last) digit
 * @return Integer 0-9
 */
int getSerialDigit4() {
  return getSerialDigit(4);
}

// ==========================================
// CONVERSIONS & CALCULATIONS
// ==========================================

/**
 * Convert a letter (A-I) to a number (1-9)
 * @param letter Character A-I
 * @return 1-9 for A-I, 0 for invalid input
 */
int letterToNumber(char letter) {
  if (letter >= 'A' && letter <= 'I') {
    return (int)(letter - 'A') + 1;  // A=1, B=2, ..., I=9
  }
  return 0;  // Invalid
}

/**
 * Get the sum of all 4 digits
 * @return Sum of digits (0-36)
 */
int getDigitSum() {
  int d1 = getSerialDigit1();
  int d2 = getSerialDigit2();
  int d3 = getSerialDigit3();
  int d4 = getSerialDigit4();
  
  // Handle invalid cases
  if (d1 < 0) d1 = 0;
  if (d2 < 0) d2 = 0;
  if (d3 < 0) d3 = 0;
  if (d4 < 0) d4 = 0;
  
  return d1 + d2 + d3 + d4;
}

/**
 * Get the second-to-last digit (3rd digit)
 * Used for Distance Module distance derivation
 * @return Integer 0-9
 */
int getSecondToLastDigit() {
  return getSerialDigit3();
}

/**
 * Get the last digit (4th digit)
 * Used for Distance Module hold time and Compass Stage 2
 * @return Integer 0-9
 */
int getLastDigit() {
  return getSerialDigit4();
}

// ==========================================
// VALIDATION & DEBUG
// ==========================================

/**
 * Verify the serial number format is valid
 * @return true if format matches ABCD-1234
 */
bool isSerialValid() {
  if (bombSerialNumber.length() != 9) return false;
  
  // Check letters (positions 0-3)
  for (int i = 0; i < 4; i++) {
    char c = bombSerialNumber.charAt(i);
    if (c < 'A' || c > 'Z') return false;
  }
  
  // Check hyphen (position 4)
  if (bombSerialNumber.charAt(4) != '-') return false;
  
  // Check digits (positions 5-8)
  for (int i = 5; i < 9; i++) {
    char c = bombSerialNumber.charAt(i);
    if (c < '0' || c > '9') return false;
  }
  
  return true;
}

/**
 * Print serial number and parsed components to serial monitor
 * (Useful for debugging - assumes Serial initialized)
 */
void debugPrintSerial() {
  Serial.print("Serial Number: ");
  Serial.println(bombSerialNumber);
  Serial.print("Letters: ");
  Serial.print(getSerialLetter1());
  Serial.print(getSerialLetter2());
  Serial.print(getSerialLetter3());
  Serial.println(getSerialLetter4());
  Serial.print("Digits: ");
  Serial.print(getSerialDigit1());
  Serial.print(getSerialDigit2());
  Serial.print(getSerialDigit3());
  Serial.println(getSerialDigit4());
  Serial.print("Digit Sum: ");
  Serial.println(getDigitSum());
  Serial.print("Letter1 as Number: ");
  Serial.println(letterToNumber(getSerialLetter1()));
}

#endif
