const int buttonPin = 4;
const int ledPin = 5;

// Menyimpan status LED, awalnya OFF
bool ledState = false;

// Menyimpan kondisi tombol sebelumnya
int lastButtonState = LOW;

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  // Saat sistem pertama dinyalakan, LED mati
  digitalWrite(ledPin, LOW);
}

void loop() {
  int buttonState = digitalRead(buttonPin);

  // Mendeteksi tombol baru ditekan
  if (buttonState == HIGH && lastButtonState == LOW) {

    // Membalik status LED: OFF menjadi ON, ON menjadi OFF
    ledState = !ledState;

    digitalWrite(ledPin, ledState);

    if (ledState == true) {
      Serial.println("Tombol ditekan -> LED ON");
    } else {
      Serial.println("Tombol ditekan -> LED OFF");
    }

    // Debounce agar satu tekanan tidak terbaca berkali-kali
    delay(200);
  }

  // Menyimpan kondisi tombol sebelumnya
  lastButtonState = buttonState;
}