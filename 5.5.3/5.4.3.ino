#include <painlessMesh.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define MESH_PREFIX   "LabIoTMesh"
#define MESH_PASSWORD "iotmeshpassword"
#define MESH_PORT     5555

#define DHTPIN 2
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

// Nama Node 1
const char* nodeName = "Node-2";

Scheduler userScheduler;
painlessMesh mesh;

void sendMessage();

Task taskSendMessage(
  TASK_SECOND * 3,
  TASK_FOREVER,
  &sendMessage
);

void sendMessage() {
  float suhu = dht.readTemperature();
  float kelembapan = dht.readHumidity();

  if (isnan(suhu) || isnan(kelembapan)) {
    Serial.println("[DHT Error] Gagal membaca data dari sensor DHT!");
    return;
  }

  StaticJsonDocument<200> doc;

  doc["node"] = nodeName;
  doc["chipId"] = mesh.getNodeId();
  doc["suhu"] = suhu;
  doc["kelembapan"] = kelembapan;

  String msg;

  serializeJson(doc, msg);

  mesh.sendBroadcast(msg);

  Serial.print("[KIRIM MESH] ");
  Serial.println(msg);
}

void receivedCallback(uint32_t from, String &msg) {
  StaticJsonDocument<200> doc;

  DeserializationError error =
    deserializeJson(doc, msg);

  if (!error) {
    const char* sender = doc["node"];
    uint32_t chipId = doc["chipId"];
    float suhu = doc["suhu"];
    float kelembapan = doc["kelembapan"];

    Serial.println("========================================");

    Serial.printf(
      "[TERIMA DARI] %s (Node ID: %u | Chip ID: %u)\n",
      sender,
      from,
      chipId
    );

    Serial.printf(
      "Suhu       : %.2f °C\n",
      suhu
    );

    Serial.printf(
      "Kelembapan : %.2f %%\n",
      kelembapan
    );

    Serial.println("========================================");
  }
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf(
    "--> Koneksi Baru Terdeteksi! Node ID: %u\n",
    nodeId
  );
}

void changedConnectionCallback() {
  Serial.println(
    "--> Topologi mesh telah diperbarui"
  );
}

void setup() {
  Serial.begin(115200);

  dht.begin();

  mesh.setDebugMsgTypes(ERROR | STARTUP);

  mesh.init(
    MESH_PREFIX,
    MESH_PASSWORD,
    &userScheduler,
    MESH_PORT
  );

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(
    &changedConnectionCallback
  );

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();

  Serial.printf(
    "Mesh Node [%s] Berjalan.\n",
    nodeName
  );
}

void loop() {
  mesh.update();
}