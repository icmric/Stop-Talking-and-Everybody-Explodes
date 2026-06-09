#ifndef COMPASS_CALIBRATION_H
#define COMPASS_CALIBRATION_H

/*
  CompassCalibration.h
  One-shot calibration utility for the HMC5883L compass.

  Usage:
    1. Temporarily call runCompassCalibration() from setup().
    2. Slowly rotate the compass through a full 360° circle.
    3. Copy the four printed values into signalAlignment.h.
    4. Remove the call from setup().
*/

#include <Wire.h>

#define HMC5883L_ADDR 0x1E

void runCompassCalibration() {
  Serial.begin(9600);
  Wire.begin();

  Wire.beginTransmission(HMC5883L_ADDR);
  Wire.write(0x00); Wire.write(0x70); Wire.endTransmission();
  Wire.beginTransmission(HMC5883L_ADDR);
  Wire.write(0x01); Wire.write(0xA0); Wire.endTransmission();
  Wire.beginTransmission(HMC5883L_ADDR);
  Wire.write(0x02); Wire.write(0x00); Wire.endTransmission();

  Serial.println("Rotate compass slowly through a full circle...");
  Serial.println("You have 15 seconds.");

  int minX = 32767, maxX = -32768;
  int minY = 32767, maxY = -32768;

  unsigned long end = millis() + 15000;
  while (millis() < end) {
    Wire.beginTransmission(HMC5883L_ADDR);
    Wire.write(0x03); Wire.endTransmission(false);
    Wire.requestFrom(HMC5883L_ADDR, 6);
    int x = (Wire.read() << 8) | Wire.read();
    (Wire.read() << 8) | Wire.read();  // discard Z
    int y = (Wire.read() << 8) | Wire.read();

    if (x < minX) minX = x; if (x > maxX) maxX = x;
    if (y < minY) minY = y; if (y > maxY) maxY = y;

    Serial.print("Time left: "); Serial.println((end - millis()) / 1000 + 1);
    delay(200);
  }

  int offsetX  = (minX + maxX) / 2;
  int offsetY  = (minY + maxY) / 2;
  int rangeX   = (maxX - minX) / 2;
  int rangeY   = (maxY - minY) / 2;
  int avgRange = (rangeX + rangeY) / 2;
  int scaleX   = (avgRange * 100) / rangeX;
  int scaleY   = (avgRange * 100) / rangeY;

  Serial.println("\n--- Copy into signalAlignment.h ---");
  Serial.print("int calOffsetX = "); Serial.print(offsetX); Serial.println(";");
  Serial.print("int calOffsetY = "); Serial.print(offsetY); Serial.println(";");
  Serial.print("int calScaleX  = "); Serial.print(scaleX);  Serial.println(";");
  Serial.print("int calScaleY  = "); Serial.print(scaleY);  Serial.println(";");
  Serial.println("------------------------------------");
}

#endif