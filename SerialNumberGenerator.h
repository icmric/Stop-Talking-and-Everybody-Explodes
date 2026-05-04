#ifndef SERIAL_NUMBER_GENERATOR_H
#define SERIAL_NUMBER_GENERATOR_H

/*
  Serial Number Generator for KTANE Bomb
  
  Format: ABCD-1234
  - First letter: A-I (constrained)
  - Next 3 letters: A-Z
  - Digits: 0-9
  
  Usage:
    String serialNumber = generateSerialNumber();
    // Returns something like: "F4K2-7391"
*/

// Generate a random serial number in format ABCD-1234
// First letter is constrained to A-I, rest are random A-Z and 0-9
String generateSerialNumber() {
  String serial = "";
  
  // First letter: A-I only
  char firstLetter = 'A' + random(0, 9);  // 0-8 gives A-I
  serial += firstLetter;
  
  // Next 3 letters: A-Z
  for (int i = 0; i < 3; i++) {
    serial += (char)('A' + random(0, 26));
  }
  
  serial += "-";
  
  // 4 digits: 0-9
  for (int i = 0; i < 4; i++) {
    serial += (char)('0' + random(0, 10));
  }
  
  return serial;
}

// Seed the random number generator with analog noise
// Call this in setup() before generating serial numbers
void seedRandomNumberGenerator() {
  // Read from an unconnected analog pin to get random noise
  // Use A3 or another unconnected pin to gather entropy
  randomSeed(analogRead(A4) + analogRead(A5) + micros());
}

#endif
