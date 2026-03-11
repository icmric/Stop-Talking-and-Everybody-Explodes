/*
Ultrasonic connected to plug D4 (d4+d5)
Chainable RGB LED connected to plug D8 (d8+d9)
Vibration sensor connected to plug A0 (a0+a1)
LED button conencted to plug D6
*/


#include "Ultrasonic.h"
#include <ChainableLED.h>
#include "Vibration.h"

const int buttonPin = 7;
// Initialize the ChainableLED object
// Syntax: ChainableLED(clock_pin, data_pin, number_of_leds)
ChainableLED leds(8, 9, 2);

// Initialise the vibration sensor on pin A0
VibrationSensor VBS(A0);

Ultrasonic ultrasonic(4);

void setup()
{
	pinMode(buttonPin, INPUT);
	leds.setColorRGB(1, 0, 255, 0);
}

bool triggered = false;

void loop()
{
	long distance = ultrasonic.MeasureInCentimeters();
	Serial.println(distance);

	// Measure vibrations for 50ms. Pauses loop during check, so acts as the delay
	VBS.measure(50);

	if (distance > 20) {
		leds.setColorRGB(0, 0, 255, 0);
	} else {
		leds.setColorRGB(0, 255, 0, 0);
	}

	if (VBS.zeroCount() > 1 || triggered == true) {
		triggered = true;
		leds.setColorRGB(1, 255, 0, 0);
	}

	if (digitalRead(buttonPin) == HIGH) {
		triggered = false;
	}
 }
