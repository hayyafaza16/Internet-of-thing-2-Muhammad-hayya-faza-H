const byte ldrPin = A0;
const byte ledPin = D1;

void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, LOW);
}

void loop() {
  int ldrValue = analogRead(ldrPin);

  Serial.print("Nilai ADC LDR: ");
  Serial.println(ldrValue);

  if (ldrValue < 200) {
    digitalWrite(ledPin, HIGH);
    Serial.println("Gelap - LED Menyala");
  }
  else {
    digitalWrite(ledPin, LOW);
    Serial.println("Cahaya cukup - LED Mati");
  }

  delay(1000);
}