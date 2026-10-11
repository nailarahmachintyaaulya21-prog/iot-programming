
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

// ==================== WIFI ====================
const char* ssid = "Astra_tailor-4G";
const char* password = "Nailarahma";

// ==================== PIN =====================
#define pinLED D1
#define pinDHT D4
#define tipeDHT DHT22

// ==================== OBJEK ===================
DHT dht(pinDHT, tipeDHT);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ==================== VARIABEL =================
int nilaiPWM = 0;
bool serverDimulai = false;
unsigned long waktuSensor = 0;
unsigned long waktuWiFi = 0;

// ==================== HALAMAN WEB ==============
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Lighting</title>

  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      background: #f2f5f9;
      padding: 20px;
    }

    .card {
      max-width: 420px;
      margin: auto;
      padding: 24px;
      background: white;
      border-radius: 16px;
      box-shadow: 0 3px 12px #0002;
    }

    input[type=range] {
      width: 100%;
      margin: 20px 0;
    }

    .angka {
      font-size: 28px;
      font-weight: bold;
    }

    .data {
      padding: 12px;
      margin-top: 10px;
      background: #eef3f8;
      border-radius: 8px;
    }
  </style>
</head>

<body>
  <div class="card">
    <h2>Smart Lighting</h2>
    <p>Kontrol Intensitas Cahaya LED</p>

    <input type="range" min="0" max="1023"
           value="0" id="pwmSlider">

    <div class="angka">
      <span id="pwmValue">0</span>
    </div>

    <p><span id="persen">0</span>% kecerahan PWM</p>

    <hr>

    <h3>Monitoring Sensor DHT22</h3>

    <div class="data">
      Suhu: <span id="suhu">--</span> &deg;C
    </div>

    <div class="data">
      Kelembapan: <span id="lembap">--</span> %
    </div>

    <p>Status WebSocket: <span id="status">Menghubungkan...</span></p>
  </div>

  <script>
    let websocket;
    let timerReconnect;
    const slider = document.getElementById("pwmSlider");

    function connectWebSocket() {
      clearTimeout(timerReconnect);

      websocket = new WebSocket(
        "ws://" + window.location.hostname + "/ws"
      );

      websocket.onopen = function() {
        document.getElementById("status").textContent = "Terhubung";
        sendPWM(slider.value);
      };

      websocket.onclose = function() {
        document.getElementById("status").textContent = "Terputus";
        timerReconnect = setTimeout(connectWebSocket, 2000);
      };

      websocket.onerror = function() {
        document.getElementById("status").textContent = "Error";
        websocket.close();
      };

      websocket.onmessage = function(event) {
        const data = event.data.split(",");

        if (data[0] === "dht" && data.length === 3) {
          document.getElementById("suhu").textContent = data[1];
          document.getElementById("lembap").textContent = data[2];
        }
      };
    }

    function sendPWM(value) {
      document.getElementById("pwmValue").textContent = value;
      document.getElementById("persen").textContent =
        Math.round(Number(value) * 100 / 1023);

      if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("pwm," + value);
      }
    }

    slider.addEventListener("input", function() {
      sendPWM(this.value);
    });

    connectWebSocket();
  </script>
</body>
</html>
)rawliteral";

// ============== PESAN WEBSOCKET =================
void handleWebSocketMessage(
  void *arg,
  uint8_t *data,
  size_t len
) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;

  if (!(info->final && info->index == 0 &&
        info->len == len &&
        info->opcode == WS_TEXT)) {
    return;
  }

  String pesan = "";

  for (size_t i = 0; i < len; i++) {
    pesan += (char)data[i];
  }

  if (pesan.startsWith("pwm,")) {
    String angka = pesan.substring(4);

    if (angka.length() == 0) return;

    for (size_t i = 0; i < angka.length(); i++) {
      if (!isDigit(angka[i])) return;
    }

    nilaiPWM = constrain(angka.toInt(), 0, 1023);
    analogWrite(pinLED, nilaiPWM);

    Serial.print("PWM diterima: ");
    Serial.println(nilaiPWM);
  }
}

// ============== EVENT WEBSOCKET =================
void onWebSocketEvent(
  AsyncWebSocket *server,
  AsyncWebSocketClient *client,
  AwsEventType type,
  void *arg,
  uint8_t *data,
  size_t len
) {
  if (type == WS_EVT_CONNECT) {
    Serial.println("Browser terhubung ke WebSocket");
  }

  if (type == WS_EVT_DISCONNECT) {
    Serial.println("Browser terputus dari WebSocket");
  }

  if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  }
}

// ============== WEB SERVER ======================
void mulaiServer() {
  if (serverDimulai) return;

  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.begin();
  serverDimulai = true;

  Serial.println("Web server berjalan!");
  Serial.print("Buka browser: http://");
  Serial.println(WiFi.localIP());
}

// ============== SETUP ===========================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== SMART LIGHTING ===");

  pinMode(pinLED, OUTPUT);
  analogWriteRange(1023);
  analogWriteFreq(1000);
  analogWrite(pinLED, 0);

  dht.begin();

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  Serial.print("Menghubungkan ke Wi-Fi: ");
  Serial.println(ssid);

  // Tunggu maksimal 20 detik
  unsigned long mulai = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - mulai < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  Serial.print("Status Wi-Fi: ");
  Serial.println(WiFi.status());

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi berhasil terhubung!");
    Serial.print("Alamat IP: ");
    Serial.println(WiFi.localIP());

    mulaiServer();
  } else {
    Serial.println("Wi-Fi belum terhubung.");
    Serial.println("Periksa nama Wi-Fi, password, dan jaringan 2.4 GHz.");
    Serial.println("Akan mencoba kembali.");
  }
}

// ============== LOOP ============================
void loop() {
  // Periksa koneksi Wi-Fi setiap 5 detik
  if (millis() - waktuWiFi >= 5000) {
    waktuWiFi = millis();

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi terputus, mencoba kembali...");
      WiFi.reconnect();
    } else if (!serverDimulai) {
      Serial.println("Wi-Fi berhasil terhubung!");
      Serial.print("Alamat IP: ");
      Serial.println(WiFi.localIP());

      mulaiServer();
    }
  }

  // Baca sensor setiap 2 detik
  if (millis() - waktuSensor >= 2000) {
    waktuSensor = millis();

    float suhu = dht.readTemperature();
    float kelembapan = dht.readHumidity();

    if (!isnan(suhu) && !isnan(kelembapan)) {
      Serial.print("Suhu: ");
      Serial.print(suhu);
      Serial.print(" C | Kelembapan: ");
      Serial.print(kelembapan);
      Serial.println(" %");

      if (WiFi.status() == WL_CONNECTED && serverDimulai) {
        String data = "dht," + String(suhu, 1) +
                      "," + String(kelembapan, 1);

        ws.textAll(data);
      }
    } else {
      Serial.println("Gagal membaca DHT22. Periksa kabel.");
    }
  }

  ws.cleanupClients();
}
