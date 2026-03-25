/*
  BOMB DEFUSAL - FREQUENCY SCRAMBLER (Simplified)
  LED bar - D2
	Encoder (left) - D5
	Encoder (right) - D7

	Packages used
	Encoder - Paul Stoffregen (v1.4.4)
*/

#include <Grove_LED_Bar.h>
#include <Encoder.h> 

// --- HARDWARE ---
Grove_LED_Bar ledBar(3, 2, 0); // Clock, Data, Green-to-Red
Encoder encLeft(5, 6);
Encoder encRight(7, 8);

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