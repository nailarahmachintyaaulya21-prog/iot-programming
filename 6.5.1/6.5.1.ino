#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Konfigurasi Wi-Fi
const char* ssid = "cireng kuah";
const char* password = "localhost090807";

// Konfigurasi Broker MQTT
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* clientId = "ESP8266_LabIoT_25782020_Full";

// Topik MQTT
const char* topicPublish =
  "kampus/labiot/nim_25782020/telemetri";
const char* topicSubscribe =
  "kampus/labiot/nim_25782020/kendali";

// Konfigurasi pin
const byte dhtPin = 2;  // D4 (GPIO 2)
const byte ledPin = 5;  // D1 (GPIO 5)

#define DHTTYPE DHT11

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTelemetryTime = 0;
const unsigned long telemetryInterval = 4000;

// Callback untuk menerima perintah LED
void callback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[256];

  if (length >= sizeof(incomingBuffer)) {
    Serial.println("Payload terlalu panjang!");
    return;
  }

  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("----------------------------------------");
  Serial.printf("[KONTROL DITERIMA] Topik: %s\n", topic);
  Serial.printf("Payload: %s\n", incomingBuffer);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);

  if (err) {
    Serial.println("Gagal membaca JSON!");
    return;
  }

  if (doc.containsKey("led")) {
    int val = doc["led"];

    if (val == 1) {
      digitalWrite(ledPin, HIGH);
      Serial.println("Status LED: MENYALA");
    } else if (val == 0) {
      digitalWrite(ledPin, LOW);
      Serial.println("Status LED: PADAM");
    } else {
      Serial.println("Perintah LED harus bernilai 0 atau 1.");
    }
  }

  Serial.println("----------------------------------------");
}

// Menghubungkan kembali ke broker MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke Broker MQTT...");

    if (client.connect(clientId)) {
      Serial.println(" Terhubung!");

      if (client.subscribe(topicSubscribe)) {
        Serial.print("Subskripsi Topik: ");
        Serial.println(topicSubscribe);
      } else {
        Serial.println("Gagal berlangganan topik!");
      }
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(". Coba lagi dalam 4 detik...");
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();

  // Koneksi Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Menghubungkan Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi Terkoneksi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Konfigurasi MQTT
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  // Menjaga koneksi dan menerima perintah MQTT
  client.loop();

  unsigned long now = millis();

  // Mengirim telemetri setiap 4 detik
  if (now - lastTelemetryTime >= telemetryInterval) {
    lastTelemetryTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      StaticJsonDocument<256> docOut;

      docOut["node_id"] = clientId;
      docOut["suhu"] = t;
      docOut["kelembapan"] = h;
      docOut["status_led"] = digitalRead(ledPin);

      char buffer[256];
      serializeJson(docOut, buffer);

      if (client.publish(topicPublish, buffer)) {
        Serial.print("[TELEMETRI TERKIRIM] ");
        Serial.println(buffer);
      } else {
        Serial.println("[ERROR] Gagal mengirim telemetri!");
      }
    } else {
      Serial.println("Gagal membaca sensor DHT. Periksa sensor dan kabel.");
    }
  }
}