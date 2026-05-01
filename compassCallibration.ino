/*
  compass_calibration.ino
  Compass (HMC5883L) - I2C 0x1E
  Rotate the compass slowly in a full circle,
  then copy the printed values into signal_alignment.ino
*/

#include <Wire.h>

#define HMC5883L_ADDR 0x1E

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.beginTransmission(HMC5883L_ADDR);
  Wire.write(0x00);
  Wire.write(0x70);
  Wire.endTransmission();

  Wire.beginTransmission(HMC5883L_ADDR);
  Wire.write(0x01);
  Wire.write(0xA0);
  Wire.endTransmission();

  Wire.beginTransmission(HMC5883L_ADDR);
  Wire.write(0x02);
  Wire.write(0x00);
  Wire.endTransmission();

  Serial.println("Rotate compass slowly in a full circle...");
  Serial.println("You have 15 seconds.");

  int minX = 32767, maxX = -32768;
  int minY = 32767, maxY = -32768;

  unsigned long calEnd = millis() + 15000;
  while (millis() < calEnd) {
    Wire.beginTransmission(HMC5883L_ADDR);
    Wire.write(0x03);
    Wire.endTransmission(false);
    Wire.requestFrom(HMC5883L_ADDR, 6);
    int x = (Wire.read() << 8) | Wire.read();
    (Wire.read() << 8) | Wire.read(); //discard Z
    int y = (Wire.read() << 8) | Wire.read();

    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;

    Serial.print("Time left: "); Serial.println((calEnd - millis()) / 1000 + 1);
    delay(200);
  }

  int offsetX  = (minX + maxX) / 2;
  int offsetY  = (minY + maxY) / 2;
  int rangeX   = (maxX - minX) / 2;
  int rangeY   = (maxY - minY) / 2;
  int avgRange = (rangeX + rangeY) / 2;
  int scaleX   = (avgRange * 100) / rangeX;
  int scaleY   = (avgRange * 100) / rangeY;

  Serial.println("\n--- COPY THESE INTO signal_alignment.ino ---");
  Serial.print("int calOffsetX = "); Serial.print(offsetX); Serial.println(";");
  Serial.print("int calOffsetY = "); Serial.print(offsetY); Serial.println(";");
  Serial.print("int calScaleX  = "); Serial.print(scaleX);  Serial.println(";");
  Serial.print("int calScaleY  = "); Serial.print(scaleY);  Serial.println(";");
  Serial.println("--------------------------------------------");
}

void loop() {}
