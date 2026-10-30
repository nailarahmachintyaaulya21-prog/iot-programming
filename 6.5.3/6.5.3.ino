#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ===============================
// 1. KONFIGURASI WIFI
// ===============================
const char* ssid = "NAMA_WIFI";
const char* password = "PASSWORD_WIFI";

// ===============================
// 2. KONFIGURASI MQTT
// ===============================
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;

const char* clientId = "ESP8266_25782020";

// Topik MQTT
const char* topicTelemetry = "kampus/labiot/25782020/telemetri";
const char* topicAlert = "kampus/labiot/25782020/alert";
const char* topicControl = "kampus/labiot/25782020/kendali";
const char* topicStatus = "kampus/labiot/25782020/status";

// Pesan Last Will
const char* willMessage =
  "{\"status\":\"OFFLINE\",\"keterangan\":\"Perangkat Terputus Tak Terduga\"}";

// Pesan ONLINE
const char* onlineMessage =
  "{\"status\":\"ONLINE\",\"keterangan\":\"Perangkat Beroperasi Normal\"}";

// ===============================
// 3. KONFIGURASI SENSOR DAN LED
// ===============================
#define DHTPIN D4
#define DHTTYPE DHT22
#define LEDPIN D1

DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

// ===============================
// 4. VARIABEL SISTEM
// ===============================
const float batasSuhu = 31.0;

unsigned long previousSensor = 0;
unsigned long previousTelemetry = 0;
unsigned long previousWiFi = 0;
unsigned long previousMQTT = 0;

const unsigned long sensorInterval = 2000;
const unsigned long telemetryInterval = 5000;
const unsigned long reconnectInterval = 5000;

// Status keselamatan
bool kondisiBahaya = false;
bool alertSent = false;

// Kendali manual LED
bool manualLED = false;

// Data sensor
float suhu = 0;
float kelembapan = 0;

// ===============================
// 5. KENDALI LED
// ===============================
void updateLED() {
  if (kondisiBahaya) {
    digitalWrite(LEDPIN, HIGH);
  } else {
    digitalWrite(LEDPIN, manualLED ? HIGH : LOW);
  }
}

// ===============================
// 6. CALLBACK KENDALI MQTT
// ===============================
void callback(char* topic, byte* payload, unsigned int length) {

  StaticJsonDocument<128> doc;

  DeserializationError error = deserializeJson(
    doc, payload, length
  );

  if (error) {
    Serial.println("JSON perintah tidak valid!");
    return;
  }

  // Kendali manual hanya saat kondisi normal
  if (String(topic) == topicControl) {

    if (kondisiBahaya) {
      Serial.println("Kendali manual ditolak: SUHU BAHAYA!");
      return;
    }

    int perintah = doc["led"] | -1;

    if (perintah == 1) {
      manualLED = true;
      Serial.println("LED dinyalakan melalui MQTT");
    }
    else if (perintah == 0) {
      manualLED = false;
      Serial.println("LED dimatikan melalui MQTT");
    }

    updateLED();
  }
}

// ===============================
// 7. KONEKSI WIFI
// ===============================
void connectWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();

  if (now - previousWiFi >= 10000) {
    previousWiFi = now;

    Serial.println("Mencoba menghubungkan WiFi...");
    WiFi.begin(ssid, password);
  }
}

// ===============================
// 8. KONEKSI MQTT DAN LWT
// ===============================
void reconnectMQTT() {

  if (WiFi.status() != WL_CONNECTED ||
      client.connected()) {
    return;
  }

  unsigned long now = millis();

  if (now - previousMQTT >= reconnectInterval) {
    previousMQTT = now;

    Serial.println("Mencoba koneksi MQTT...");

    bool connected = client.connect(
      clientId,
      topicStatus,
      1,
      true,
      willMessage
    );

    if (connected) {

      Serial.println("MQTT berhasil terhubung!");

      // Mengirim status ONLINE
      client.publish(topicStatus, onlineMessage, true);

      // Subscribe topik kendali
      client.subscribe(topicControl);

      Serial.println("Subscribe topik kendali berhasil");
    }
    else {
      Serial.print("MQTT gagal, rc = ");
      Serial.println(client.state());
    }
  }
}

// ===============================
// 9. PEMBACAAN SENSOR DAN SAFETY
// ===============================
void readSensor() {

  unsigned long now = millis();

  if (now - previousSensor >= sensorInterval) {
    previousSensor = now;

    float newSuhu = dht.readTemperature();
    float newKelembapan = dht.readHumidity();

    if (isnan(newSuhu) || isnan(newKelembapan)) {
      Serial.println("Gagal membaca DHT!");
      return;
    }

    suhu = newSuhu;
    kelembapan = newKelembapan;

    Serial.print("Suhu: ");
    Serial.println(suhu);

    Serial.print("Kelembapan: ");
    Serial.println(kelembapan);

    // Deteksi kondisi bahaya
    kondisiBahaya = (suhu > batasSuhu);

    updateLED();

    // Kirim alarm hanya saat pertama kali bahaya
    if (kondisiBahaya && !alertSent) {

      if (client.connected()) {

        StaticJsonDocument<128> alertDoc;

        alertDoc["status"] = "BAHAYA";
        alertDoc["parameter"] = "OVERHEAT";
        alertDoc["nilai_suhu"] = suhu;

        char alertBuffer[128];

        serializeJson(alertDoc, alertBuffer);

        if (client.publish(topicAlert, alertBuffer)) {
          alertSent = true;
          Serial.println("PERINGATAN BAHAYA TERKIRIM!");
          Serial.println(alertBuffer);
        }
      }
    }

    // Reset alarm ketika suhu kembali normal
    if (!kondisiBahaya) {
      alertSent = false;
    }
  }
}

// ===============================
// 10. PENGIRIMAN TELEMETRI JSON
// ===============================
void publishTelemetry() {

  unsigned long now = millis();

  if (now - previousTelemetry >= telemetryInterval) {
    previousTelemetry = now;

    if (!client.connected()) {
      return;
    }

    StaticJsonDocument<256> doc;

    doc["node_id"] = clientId;
    doc["suhu"] = suhu;
    doc["kelembapan"] = kelembapan;

    // Klasifikasi kondisi suhu
    doc["kondisi_suhu"] =
      (suhu > batasSuhu) ? "PANAS" : "NORMAL";

    // Informasi tambahan
    doc["uptime_detik"] = millis() / 1000;
    doc["ip_lokal"] = WiFi.localIP().toString();

    char buffer[256];

    serializeJson(doc, buffer);

    client.publish(topicTelemetry, buffer);

    Serial.println("DATA TELEMETRI:");
    Serial.println(buffer);
  }
}

// ===============================
// 11. SETUP
// ===============================
void setup() {

  Serial.begin(115200);

  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  Serial.println("SISTEM INDUSTRI IoT DIMULAI");
}

// ===============================
// 12. LOOP UTAMA
// ===============================
void loop() {

  connectWiFi();

  reconnectMQTT();

  if (client.connected()) {
    client.loop();
  }

  readSensor();

  publishTelemetry();

  updateLED();
}
