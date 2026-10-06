#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

// =========================
// WIFI
// =========================

const char* ssid = "Astra_tailor-4G";
const char* password = "Nailarahma";

// =========================
// PIN
// =========================

#define DHTPIN 2       // D4
#define DHTTYPE DHT22

const int buttonPin = 4;   // D2
const int ledPin = 12;     // D6

// =========================
// DHT22
// =========================

DHT dht(DHTPIN, DHTTYPE);

// =========================
// WEBSERVER & WEBSOCKET
// =========================

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// =========================
// VARIABEL
// =========================

bool ledState = false;

String currentTemp = "--";
String currentHum = "--";

int buttonState = LOW;
int lastButtonState = LOW;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

unsigned long lastDHTRead = 0;
const unsigned long dhtInterval = 3000;


// =========================
// HTML
// =========================

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>

<html>

<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>IoT Dashboard</title>

<style>

body {
  font-family: Arial, sans-serif;
  text-align: center;
  background: #f2f2f2;
  margin: 0;
  padding: 20px;
}

h1 {
  margin-bottom: 25px;
}

.card {
  background: white;
  padding: 20px;
  margin: 15px auto;
  max-width: 400px;
  border-radius: 10px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.15);
}

.value {
  font-size: 30px;
  font-weight: bold;
}

button {
  padding: 12px 25px;
  border: none;
  border-radius: 8px;
  font-size: 16px;
}

.btn-on {
  background: #e74c3c;
  color: white;
}

.btn-off {
  background: #2ecc71;
  color: white;
}

</style>

</head>


<body>

<h1>IoT Dashboard</h1>


<!-- SUHU -->

<div class="card">

<h2>Suhu</h2>

<div class="value">
<span id="tempValue">--</span> °C
</div>

</div>


<!-- KELEMBAPAN -->

<div class="card">

<h2>Kelembapan</h2>

<div class="value">
<span id="humValue">--</span> %
</div>

</div>


<!-- LED -->

<div class="card">

<h2>Status LED</h2>

<div class="value">

<span id="ledStatus">OFF</span>

</div>

<br>

<button
  id="toggleBtn"
  class="btn-off"
  onclick="toggleLED()">

Turn ON

</button>

</div>


<script>

// =========================
// WEBSOCKET
// =========================

var websocket;

function initWebSocket() {

  var gateway =
    "ws://" + window.location.hostname + "/ws";

  console.log("Menghubungkan WebSocket...");

  websocket = new WebSocket(gateway);


  websocket.onopen = function(event) {

    console.log("WebSocket terhubung");

  };


  websocket.onclose = function(event) {

    console.log("WebSocket terputus");

    setTimeout(initWebSocket, 2000);

  };


  websocket.onmessage = function(event) {

    onMessage(event);

  };

}


// =========================
// MENERIMA DATA
// =========================

function onMessage(event) {

  console.log("Data diterima:");
  console.log(event.data);

  var dataObj =
    JSON.parse(event.data);


  // =========================
  // SUHU
  // =========================

  if (dataObj.suhu !== undefined) {

    document.getElementById(
      "tempValue"
    ).innerHTML = dataObj.suhu;

  }


  // =========================
  // KELEMBAPAN
  // =========================

  if (dataObj.hum !== undefined) {

    document.getElementById(
      "humValue"
    ).innerHTML = dataObj.hum;

  }


  // =========================
  // STATUS LED
  // =========================

  if (dataObj.led !== undefined) {

    var status =
      document.getElementById("ledStatus");

    var button =
      document.getElementById("toggleBtn");


    if (dataObj.led == "1") {

      status.innerHTML = "ON";

      button.innerHTML = "Turn OFF";

      button.className = "btn-on";

    }

    else {

      status.innerHTML = "OFF";

      button.innerHTML = "Turn ON";

      button.className = "btn-off";

    }

  }

}


// =========================
// TOMBOL WEBSITE
// =========================

function toggleLED() {

  websocket.send("toggle");

}


// =========================
// JALANKAN WEBSOCKET
// =========================

window.addEventListener(
  "load",
  initWebSocket
);

</script>

</body>

</html>

)rawliteral";


// =========================
// KIRIM DATA KE BROWSER
// =========================

void notifyClients() {

  String jsonString = "{";

  jsonString += "\"led\":\"";

  jsonString += String(
    ledState ? 1 : 0
  );

  jsonString += "\",";


  jsonString += "\"suhu\":\"";

  jsonString += currentTemp;

  jsonString += "\",";


  jsonString += "\"hum\":\"";

  jsonString += currentHum;

  jsonString += "\"";


  jsonString += "}";


  Serial.print("Data dikirim: ");

  Serial.println(jsonString);


  ws.textAll(jsonString);

}


// =========================
// PESAN DARI BROWSER
// =========================

void handleWebSocketMessage(
  void *arg,
  uint8_t *data,
  size_t len
) {

  if (
    len == 6 &&
    strncmp(
      (char*)data,
      "toggle",
      6
    ) == 0
  ) {

    ledState = !ledState;


    digitalWrite(
      ledPin,
      ledState ? HIGH : LOW
    );


    notifyClients();

  }

}


// =========================
// EVENT WEBSOCKET
// =========================

void onEvent(
  AsyncWebSocket *server,
  AsyncWebSocketClient *client,
  AwsEventType type,
  void *arg,
  uint8_t *data,
  size_t len
) {

  switch (type) {

    case WS_EVT_CONNECT:

      Serial.printf(
        "WebSocket client #%u terhubung\n",
        client->id()
      );

      notifyClients();

      break;


    case WS_EVT_DISCONNECT:

      Serial.printf(
        "WebSocket client #%u terputus\n",
        client->id()
      );

      break;


    case WS_EVT_DATA:

      handleWebSocketMessage(
        arg,
        data,
        len
      );

      break;


    case WS_EVT_PONG:
      break;


    case WS_EVT_ERROR:
      break;

  }

}


// =========================
// SETUP
// =========================

void setup() {

  Serial.begin(115200);


  // LED

  pinMode(
    ledPin,
    OUTPUT
  );

  digitalWrite(
    ledPin,
    LOW
  );


  // TOMBOL

  pinMode(
    buttonPin,
    INPUT
  );


  // DHT22

  dht.begin();


  Serial.println();
  Serial.println("======================");
  Serial.println("ESP8266 START");
  Serial.println("======================");


  // =========================
  // CONNECT WIFI
  // =========================

  WiFi.begin(
    ssid,
    password
  );


  Serial.print(
    "Menghubungkan WiFi"
  );


  while (
    WiFi.status() != WL_CONNECTED
  ) {

    delay(500);

    Serial.print(".");

  }


  Serial.println();

  Serial.println(
    "WiFi terhubung!"
  );


  Serial.print(
    "IP Address: "
  );

  Serial.println(
    WiFi.localIP()
  );


  // =========================
  // WEBSOCKET
  // =========================

  ws.onEvent(onEvent);

  server.addHandler(&ws);


  // =========================
  // HALAMAN WEB
  // =========================

  server.on(
    "/",
    HTTP_GET,
    [](AsyncWebServerRequest *request) {

      request->send_P(
        200,
        "text/html",
        index_html
      );

    }
  );


  // =========================
  // MULAI SERVER
  // =========================

  server.begin();


  Serial.println(
    "Web Server berjalan!"
  );

  Serial.println(
    "Buka IP Address di browser."
  );

}


// =========================
// LOOP
// =========================

void loop() {

  ws.cleanupClients();


  // =========================
  // BACA TOMBOL
  // =========================

  int reading =
    digitalRead(buttonPin);


  if (
    reading != lastButtonState
  ) {

    lastDebounceTime =
      millis();

  }


  if (
    (millis() - lastDebounceTime)
    > debounceDelay
  ) {

    if (
      reading != buttonState
    ) {

      buttonState =
        reading;


      if (
        buttonState == HIGH
      ) {

        ledState =
          !ledState;


        digitalWrite(
          ledPin,
          ledState ? HIGH : LOW
        );


        notifyClients();

      }

    }

  }


  lastButtonState =
    reading;


  // =========================
  // BACA DHT22
  // SETIAP 3 DETIK
  // =========================

  if (
    millis() - lastDHTRead
    >= dhtInterval
  ) {

    lastDHTRead =
      millis();


    float temperature =
      dht.readTemperature();


    float humidity =
      dht.readHumidity();


    // =========================
    // CEK HASIL SENSOR
    // =========================

    if (
      !isnan(temperature) &&
      !isnan(humidity)
    ) {

      currentTemp =
        String(
          temperature,
          1
        );


      currentHum =
        String(
          humidity,
          1
        );


      Serial.print(
        "Suhu: "
      );

      Serial.print(
        currentTemp
      );

      Serial.print(
        " °C | Kelembapan: "
      );

      Serial.print(
        currentHum
      );

      Serial.println(
        " %"
      );


      notifyClients();

    }

    else {

      Serial.println(
        "Gagal membaca DHT22!"
      );

    }

  }

}