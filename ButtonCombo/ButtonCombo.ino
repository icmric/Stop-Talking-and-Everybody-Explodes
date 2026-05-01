// -------- PINS --------
#define RED_BUTTON 11
#define GREEN_BUTTON 12
#define BUZZER 13

// -------- SETTINGS --------
const unsigned long TIME_LIMIT = 5000;

// -------- VARIABLES --------
int sequence[3] = {RED_BUTTON, RED_BUTTON, GREEN_BUTTON};
int stepIndex = 0;

unsigned long startTime = 0;
bool started = false;
bool complete = false;

// -------- SETUP --------
void setup() {
  Serial.begin(9600);

  pinMode(RED_BUTTON, INPUT_PULLUP);
  pinMode(GREEN_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  Serial.println("Button Module Started");
  Serial.println("Press RED → RED → GREEN");
}

// -------- LOOP --------
void loop() {

  if (complete) return;

  if (!started) {
    startTime = millis();
    started = true;
  }

  if (millis() - startTime > TIME_LIMIT) {
    Serial.println("TIME OUT - RESET");
    stepIndex = 0;
    startTime = millis();
  }

  if (digitalRead(RED_BUTTON) == LOW) {
    handlePress(RED_BUTTON);
    delay(250);
  }

  if (digitalRead(GREEN_BUTTON) == LOW) {
    handlePress(GREEN_BUTTON);
    delay(250);
  }
}

// -------- HANDLE INPUT --------
void handlePress(int btn) {

  if (btn == RED_BUTTON) Serial.println("RED");
  else Serial.println("GREEN");

  if (btn == sequence[stepIndex]) {

    tone(BUZZER, 900, 100);
    stepIndex++;

    if (stepIndex == 3) {
      complete = true;
      Serial.println("BUTTON COMPLETE");
      successTone();
    }

  } else {
    Serial.println("WRONG - RESET");
    tone(BUZZER, 200, 300);
    stepIndex = 0;
  }
}

// -------- SOUND --------
void successTone() {
  tone(BUZZER, 800, 120); delay(140);
  tone(BUZZER, 1000, 120); delay(140);
  tone(BUZZER, 1400, 200); delay(220);
}