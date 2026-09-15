#include <DHT.h>

const byte dhtPin = D7;
const byte ldrPin = A0;
const byte relayPin = D6;
const byte ledPin = D1;

#define DHTTYPE DHT11

DHT dht(dhtPin, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(relayPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Relay mati saat awal
  digitalWrite(relayPin, LOW);
  digitalWrite(ledPin, LOW);

  Serial.println("Memulai Sistem Sensor Lingkungan...");
}

void loop() {
  // Membaca sensor
  int ldrValue = analogRead(ldrPin);
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // Cek apakah DHT berhasil dibaca
  if (isnan(temp) || isnan(hum)) {
    Serial.println("Gagal membaca data DHT!");
    delay(2500);
    return;
  }

  // Menampilkan data sensor
  Serial.print("Suhu: ");
  Serial.print(temp);
  Serial.print(" C | Kelembapan: ");
  Serial.print(hum);
  Serial.print(" % | LDR: ");
  Serial.println(ldrValue);

// Rule Engine
if (temp > 34 || ldrValue < 300) {
  digitalWrite(relayPin, HIGH);
    digitalWrite(ledPin, HIGH);
  Serial.println("Peringatan: Aktuator Aktif!");
} 
else {
  digitalWrite(relayPin, LOW);
   digitalWrite(ledPin, LOW);
  Serial.println("Kondisi Aman");
}

Serial.println("-------------------------");

delay(2500);
}