void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(22, INPUT);
  Serial.println("Testing pin 22...");
}

void loop() {
  bool reading = digitalRead(22);
  Serial.print("Pin 22: ");
  Serial.println(reading ? "HIGH" : "LOW");
  delay(500);
}