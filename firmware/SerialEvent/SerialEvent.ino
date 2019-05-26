String inputString = "";         // a String to hold incoming data

void setup() {
  // initialize serial:
  Serial.begin(115200);
   pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
}
void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    Serial.println(inChar);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
  }
}
