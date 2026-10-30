#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Kredensial jaringan Wi-Fi
const char* ssid = "Astra_tailor-4G";
const char* password = "Nailarahma";

// Konfigurasi broker MQTT publik
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* clientId = "ESP8266_LabIoT_25782020_Sub";

// Topik subskripsi perintah
const char* topicSubCommand =
  "kampus/labiot/25782020/perintah";

// Pin indikator LED
const byte ledPin = 5; // D1 (GPIO 5)

WiFiClient espClient;
PubSubClient client(espClient);

void connectToWiFi() {
  Serial.print("Menghubungkan ke Wi-Fi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi Terkoneksi!");
}

// Callback saat menerima pesan pada topik terdaftar
void onMessageReceived(
  char* topic, byte* payload, unsigned int length
) {
  Serial.println("========================================");
  Serial.print("Pesan Baru Masuk pada Topik: ");
  Serial.println(topic);

  // Mengubah byte payload menjadi string
  char payloadStr[length + 1];
  memcpy(payloadStr, payload, length);
  payloadStr[length] = '\0';

  Serial.print("Payload Diterima: ");
  Serial.println(payloadStr);

  // Parsing JSON
  StaticJsonDocument<256> doc;
  DeserializationError error =
    deserializeJson(doc, payloadStr);

  if (error) {
    Serial.print("Gagal parsing JSON: ");
    Serial.println(error.f_str());
    return;
  }

  // Mengendalikan LED berdasarkan nilai "led"
  if (doc.containsKey("led")) {
    int stateLED = doc["led"];

    digitalWrite(
      ledPin,
      stateLED == 1 ? HIGH : LOW
    );

    Serial.printf(
      "--> Aksi: Status LED diubah menjadi %s\n",
      stateLED == 1 ? "MENYALA (ON)" : "PADAM (OFF)"
    );
  }

  Serial.println("========================================");
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Mencoba koneksi MQTT...");

    if (client.connect(clientId)) {
      Serial.println(" Terhubung!");

      // Berlangganan ke topik perintah
      client.subscribe(topicSubCommand);

      Serial.print("Subskripsi Topik: ");
      Serial.println(topicSubCommand);
    } else {
      Serial.printf(
        "Gagal (rc=%d). Ulangi dalam 5 detik...\n",
        client.state()
      );

      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);

  // Status awal LED padam
  digitalWrite(ledPin, LOW);

  connectToWiFi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(onMessageReceived);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }

  client.loop();
}