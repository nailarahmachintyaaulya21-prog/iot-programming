#include <ESP8266WiFi.h>
#include <espnow.h>

uint8_t hubMacAddress[] = {0x84, 0xF3, 0xEB, 0x92, 0xA9, 0xBC};

#define BOARD_ID 2

typedef struct struct_sensor {
  int id;
  float pembacaan1;
  float pembacaan2;
} struct_sensor;

struct_sensor dataKirim;

unsigned long prevTime = 0;
const unsigned long sendInterval = 3000;

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Pengiriman Node #");
  Serial.print(BOARD_ID);

  if (sendStatus == 0) {
    Serial.println(" -> Sukses Diterima Hub");
  } else {
    Serial.println(" -> Gagal Sampai");
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println("Inisialisasi ESP-NOW gagal");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);

  esp_now_add_peer(
    hubMacAddress,
    ESP_NOW_ROLE_SLAVE,
    1,
    NULL,
    0
  );

  Serial.println("Node 2 siap mengirim data");
}

void loop() {
  unsigned long now = millis();

  if (now - prevTime >= sendInterval) {
    prevTime = now;

    dataKirim.id = BOARD_ID;
    dataKirim.pembacaan1 = random(250, 350) / 10.0;
    dataKirim.pembacaan2 = random(400, 800) / 10.0;

    esp_now_send(
      hubMacAddress,
      (uint8_t *)&dataKirim,
      sizeof(dataKirim)
    );
  }
}