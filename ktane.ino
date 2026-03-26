/*
  BOMB DEFUSAL - FREQUENCY SCRAMBLER (Simplified)
  LED bar - D2
	Encoder (left) - D5
	Encoder (right) - D7

	Packages used
	Encoder - Paul Stoffregen (v1.4.4)

  CORE OVERHEATING MODULE
  DHT11 Temp Sensor - D4     
  LCD 16x2 I2C      - SDA/SCL
  Packages used
    DHT sensor library - Adafruit
    LiquidCrystal_I2C  - Frank de Brabander
*/

#include <Grove_LED_Bar.h>
#include <Encoder.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
//
// --- HARDWARE ---
Grove_LED_Bar ledBar(3, 2, 0); // Clock, Data, Green-to-Red
Encoder encLeft(5, 6);
Encoder encRight(7, 8);
// --- CORE OVERHEATING: Hardware ---
#define DHT_PIN  4
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); // change 0x27 to 0x3F if LCD doesn't show

// --- CORE OVERHEATING: Config ---
const float COOL_BY = 2.0; // degrees C the user must reduce by blowing

// --- CONFIGURATION ---
struct ComboStep {
  char encoder; 
  int direction; // 1 = CW, -1 = CCW
  int clicks;
};

ComboStep combination[] = {
  {'L', 1, 3},  // Left 3 Right
  {'R', -1, 2}, // Right 2 Left
  {'L', -1, 2}  // Left 2 Left
};

// --- STATE VARIABLES ---
int currentStep = 0;
const int totalSteps = 3;
int clicksInCurrentStep = 0;
long lastLeftPos, lastRightPos;

unsigned long lastMoveTime = 0;   // Tracks when the knob was last turned
const int confirmDelay = 500;     // 0.5 seconds pause required
bool frequencyModuleSolved = false;

void setup() {
  Serial.begin(9600);
  ledBar.begin();
  updateFrequencyDisplay();
 

  lcd.init();
  lcd.backlight();
  dht.begin();
// Added to initialise lastLeftPos and lastRightPos at startup.
// Without this, the first movement of the encoders could register as a huge jump 
// because lastLeftPos/lastRightPos would start at 0. 
// This would cause the first step clicks to count incorrectly, potentially 
// resetting the step immediately or misaligning the combination sequence.
  lastLeftPos = encLeft.read() / 4;
  lastRightPos = encRight.read() / 4;
  Serial.println("System Ready. Start turning...");
}

void loop() {
  if (!frequencyModuleSolved) {
    runFrequencyModule();
  }
}

void runFrequencyModule() {
  long currLeft = encLeft.read() / 4;  
  long currRight = encRight.read() / 4;
  ComboStep target = combination[currentStep];

  // 1. SENSE MOVEMENT
  bool moved = false;
  int moveDir = 0;

  if (target.encoder == 'L' && currLeft != lastLeftPos) {
    moveDir = (currLeft > lastLeftPos) ? 1 : -1;
    moved = true;
    lastLeftPos = currLeft;
  } 
  else if (target.encoder == 'R' && currRight != lastRightPos) {
    moveDir = (currRight > lastRightPos) ? 1 : -1;
    moved = true;
    lastRightPos = currRight;
  }

  // 2. PROCESS MOVEMENT
  if (moved) {
    lastMoveTime = millis(); // Reset the timer every time they turn
    
    if (moveDir == target.direction) {
      clicksInCurrentStep++;
      Serial.print("Click: "); Serial.println(clicksInCurrentStep);
    } else {
      clicksInCurrentStep = 0; // Penalty: Reset clicks if turned wrong way
      Serial.println("WRONG DIRECTION - RESET STEP");
    }
  }

  // 3. THE "PAUSE TO CONFIRM" LOGIC
  // If we have enough clicks AND the user has stopped moving for 500ms
  if (clicksInCurrentStep >= target.clicks && (millis() - lastMoveTime > confirmDelay)) {
    currentStep++;
    clicksInCurrentStep = 0;
    
    Serial.print("Step "); Serial.print(currentStep); Serial.println(" CONFIRMED");
    updateFrequencyDisplay();

    if (currentStep >= totalSteps) {
      frequencyModuleSolved = true;
      flashSuccess();
    }
  }
}

void updateFrequencyDisplay() {
  // LED Bar shows progress: 10 (full) down to 1 (final stage)
  int displayLevel = map(currentStep, 0, totalSteps, 10, 1);
  ledBar.setLevel(displayLevel);
}

void flashSuccess() {
  Serial.println("SOLVED!");
  for (int i = 0; i < 4; i++) {
    ledBar.setLevel(10); delay(150);
    ledBar.setLevel(0);  delay(150);
  }
  ledBar.setLevel(10);
}

void coreOverheating() {
  // Snapshot starting temperature
  float startTemp = dht.readTemperature();
  if (isnan(startTemp)) {
  delay(1500);
  startTemp = dht.readTemperature();
  if (isnan(startTemp)) return;
} // sensor failure - kills, so it doesn't break, delay gives it a chance

  float targetTemp = startTemp - COOL_BY;

  // Display warning
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CORE OVERHEAT!");
  lcd.setCursor(0, 1);
  lcd.print("BLOW TO COOL");

  // Loop until temp drops enough
  while (true) {
    float currentTemp = dht.readTemperature();

    if (!isnan(currentTemp)) {
      // Live temp readout on bottom row
      lcd.setCursor(0, 1);
      lcd.print("TEMP: ");
      lcd.print(currentTemp, 1); // 1 decimal place
      lcd.print("C   ");         // trailing spaces clear leftover characters

      if (currentTemp <= targetTemp) break;
    }

    delay(1200); 
  }

  // Show cleared message then wipe LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CORE STABLE");
  delay(1500);
  lcd.clear();
}
