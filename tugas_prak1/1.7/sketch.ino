const int buttonPin = 4;
const int ledPin = 5;

// Menyimpan kondisi LED.
// false = LED mati, true = LED menyala.
bool ledState = false;

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  // Saat pertama dinyalakan, LED dalam keadaan mati.
  digitalWrite(ledPin, LOW);
}

void loop() {

  // Pada rangkaian pull-down:
  // tombol ditekan = HIGH.
  if (digitalRead(buttonPin) == HIGH) {

    // Balikkan kondisi LED:
    // mati -> menyala
    // menyala -> mati
    ledState = !ledState;

    // Terapkan kondisi baru ke LED.
    digitalWrite(ledPin, ledState);

    // Debounce: mencegah satu tekanan
    // terbaca berkali-kali akibat pantulan tombol.
    delay(200);

    // Tunggu sampai tombol benar-benar dilepas.
    // Jadi tombol yang ditahan tidak membuat LED
    // berubah berkali-kali.
    while (digitalRead(buttonPin) == HIGH) {
      delay(10);
    }

    // Tampilkan kondisi LED pada Serial Monitor.
    if (ledState == true) {
      Serial.println("LED Menyala");
    } else {
      Serial.println("LED Mati");
    }
  }
}