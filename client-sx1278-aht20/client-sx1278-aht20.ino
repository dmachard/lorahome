#include <AES.h>
#include <Adafruit_AHTX0.h>
#include <Arduino.h>
#include <GCM.h>
#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_task_wdt.h>

#define WDT_TIMEOUT_S 30

#define LORA_CS 10
#define LORA_RST 0
#define LORA_BUSY 5
#define LORA_DIO1 8

#define SPI_SCK 6
#define SPI_MISO 2
#define SPI_MOSI 7

#define I2C_SDA 3
#define I2C_SCL 4

#define NODE_ID 2

#define LORA_FREQ 433.0
#define LORA_BW 125.0
#define LORA_SF 7
#define LORA_CR 5
#define LORA_SYNC RADIOLIB_SX126X_SYNC_WORD_PRIVATE
#define LORA_POWER 10
#define LORA_PREAMBLE 8

uint8_t AES_KEY[16] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
                       0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};

#define TAG_SIZE 8
#define HDR_SIZE 9 // node_id(1) + seq(4) + random_id(4)

#define TYPE_AHT20 0x03

enum ErrorCode { ERR_NONE = 0, ERR_SENSOR_READ = 1, ERR_TX_FAILED = 2 };

struct __attribute__((packed)) SensorPayload {
  int16_t temperature_x100;
  uint16_t humidity_x100;
  uint8_t type;
  uint8_t reset_reason;
  uint8_t error_code;
};

GCM<AES128> gcm;

uint8_t gcm_encrypt(uint8_t *frame, const uint8_t *payload, uint8_t payload_len,
                    uint8_t node_id, uint32_t seq, uint32_t random_id) {
  frame[0] = node_id;
  frame[1] = (seq >> 24) & 0xFF;
  frame[2] = (seq >> 16) & 0xFF;
  frame[3] = (seq >> 8) & 0xFF;
  frame[4] = (seq) & 0xFF;
  frame[5] = (random_id >> 24) & 0xFF;
  frame[6] = (random_id >> 16) & 0xFF;
  frame[7] = (random_id >> 8) & 0xFF;
  frame[8] = (random_id) & 0xFF;

  uint8_t iv[12] = {0};
  memcpy(iv, frame, 9);

  gcm.clear();
  gcm.setKey(AES_KEY, 16);
  gcm.setIV(iv, 12);
  gcm.addAuthData(frame, HDR_SIZE);
  gcm.encrypt(frame + HDR_SIZE, payload, payload_len);
  gcm.computeTag(frame + HDR_SIZE + payload_len, TAG_SIZE);

  return HDR_SIZE + payload_len + TAG_SIZE;
}

SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
Adafruit_AHTX0 aht;
uint32_t seq = 0;
uint32_t node_random_id = 0;
uint8_t last_reset_reason = 0;
uint8_t current_error_code = ERR_NONE;

void setup() {
  Serial.begin(115200);
  delay(1000);

  esp_reset_reason_t reason = esp_reset_reason();
  last_reset_reason = (uint8_t)reason;
  Serial.printf("Reset reason: %d\n", reason);

  esp_task_wdt_config_t wdt_config = {.timeout_ms = WDT_TIMEOUT_S * 1000,
                                      .idle_core_mask = 0,
                                      .trigger_panic = true};
  esp_task_wdt_reconfigure(&wdt_config);
  esp_task_wdt_add(NULL);

  node_random_id = esp_random();
  Serial.printf("Random Node ID: %08X\n", node_random_id);

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!aht.begin()) {
    Serial.println("AHT20 not found!");
    while (true)
      delay(1000);
  }
  Serial.println("AHT20 OK");

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, LORA_CS);
  Serial.println("Init SX1262...");
  int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SYNC,
                          LORA_POWER, LORA_PREAMBLE);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("OK !");
  } else {
    Serial.printf("Echec: %d\n", state);
    while (true)
      delay(1000);
  }
}

void loop() {
  esp_task_wdt_reset();

  sensors_event_t humidity_event, temp_event;
  bool success = false;

  for (int retry = 0; retry < 3; retry++) {
    if (aht.getEvent(&humidity_event, &temp_event)) {
      success = true;
      break;
    }
    Serial.printf("AHT20 read failed (attempt %d/3)\n", retry + 1);
    delay(2000);
    esp_task_wdt_reset();
  }

  float t, h;
  if (!success) {
    Serial.println("AHT20 error: failed after 3 attempts.");
    current_error_code = ERR_SENSOR_READ;
    t = -99.99f;
    h = 0.0f;
  } else {
    current_error_code = ERR_NONE;
    t = temp_event.temperature;
    h = humidity_event.relative_humidity;
  }

  Serial.printf("T=%.2f°C | H=%.2f%% | ERR=%d\n", t, h, current_error_code);

  SensorPayload payload;
  payload.temperature_x100 = (int16_t)(t * 100);
  payload.humidity_x100 = (uint16_t)(h * 100);
  payload.type = TYPE_AHT20;
  payload.reset_reason = last_reset_reason;
  payload.error_code = current_error_code;

  uint8_t frame[64];
  uint8_t len = gcm_encrypt(frame, (uint8_t *)&payload, sizeof(payload),
                            NODE_ID, seq++, node_random_id);

  int state = radio.transmit(frame, len);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.printf("TX OK seq=%lu\n", seq - 1);
    if (current_error_code == ERR_TX_FAILED)
      current_error_code = ERR_NONE;
  } else {
    Serial.printf("TX failed: %d\n", state);
    current_error_code = ERR_TX_FAILED;
  }

  esp_task_wdt_reset();
  delay(5000);
}