#include <AES.h>
#include <Arduino.h>
#include <DHT.h>
#include <GCM.h>
#include <RadioLib.h>
#include <SPI.h>
#include <esp_task_wdt.h>

#define WDT_TIMEOUT_S 30

#define LORA_CS 10
#define LORA_DIO0 8
#define LORA_RST 0
#define LORA_DIO1 -1

#define SPI_SCK 6
#define SPI_MISO 2
#define SPI_MOSI 7

#define DHT_PIN 4
#define DHT_TYPE DHT22

#define NODE_ID 1

#define LORA_FREQ 433.0
#define LORA_BW 125.0
#define LORA_SF 7
#define LORA_CR 5
#define LORA_POWER 10
#define LORA_PREAMBLE 8

uint8_t AES_KEY[16] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
                       0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};

#define TAG_SIZE 8
#define HDR_SIZE 9 // node_id(1) + seq(4) + random_id(4)

#define TYPE_DHT22 0x02

enum ErrorCode { ERR_NONE = 0, ERR_DHT_READ = 1, ERR_TX_FAILED = 2 };

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

SX1278 radio = new Module(LORA_CS, LORA_DIO0, LORA_RST, LORA_DIO1);
DHT dht(DHT_PIN, DHT_TYPE);
uint32_t seq = 0;
uint32_t node_random_id = 0;
uint8_t last_reset_reason = 0;
uint8_t current_error_code = ERR_NONE;

void setup() {
  Serial.begin(115200);
  delay(1000);

  esp_reset_reason_t reason = esp_reset_reason();
  last_reset_reason = (uint8_t)reason;
  Serial.printf("Reset reason code: %d\n", reason);
  /*
   * ESP32 Reset Reason Codes:
   * 1: ESP_RST_POWERON   - Power-on reset
   * 2: ESP_RST_EXT       - Reset by external pin
   * 3: ESP_RST_SW        - Software reset
   * 4: ESP_RST_PANIC     - Software exception / panic
   * 5: ESP_RST_INT_WDT   - Interrupt watchdog
   * 6: ESP_RST_TASK_WDT  - Task watchdog
   * 7: ESP_RST_WDT       - Other watchdog
   * 8: ESP_RST_DEEPSLEEP - Deep sleep reset
   * 15: ESP_RST_BROWNOUT - Brownout (voltage drop)
   */

  Serial.println("Configuring Watchdog...");
  esp_task_wdt_config_t wdt_config = {.timeout_ms = WDT_TIMEOUT_S * 1000,
                                      .idle_core_mask = 0,
                                      .trigger_panic = true};
  esp_task_wdt_reconfigure(&wdt_config);
  esp_task_wdt_add(NULL);

  node_random_id = esp_random();
  Serial.printf("Random Node ID: %08X\n", node_random_id);

  dht.begin();
  Serial.println("DHT22 OK");

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, LORA_CS);
  Serial.println("Init SX1278...");
  int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, 0x12,
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

  float t = NAN, h = NAN;
  bool success = false;

  for (int retry = 0; retry < 3; retry++) {
    t = dht.readTemperature();
    h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      success = true;
      break;
    }

    Serial.printf("DHT22 read failed (attempt %d/3), retrying...\n", retry + 1);
    delay(2000);
    esp_task_wdt_reset();
  }

  if (!success) {
    Serial.println(
        "DHT22 sensor error: failed after 3 attempts. Sending error packet.");
    current_error_code = ERR_DHT_READ;
    t = -99.99; // Dummy value
    h = 0.0;    // Dummy value
  } else {
    current_error_code = ERR_NONE; // Clear error if reading successful
  }

  Serial.printf("T=%.2f°C | H=%.2f%% | ERR=%d\n", t, h, current_error_code);

  SensorPayload payload;
  payload.temperature_x100 = (int16_t)(t * 100);
  payload.humidity_x100 = (uint16_t)(h * 100);
  payload.type = TYPE_DHT22;
  payload.reset_reason = last_reset_reason;
  payload.error_code = current_error_code;

  uint8_t frame[64];
  uint8_t len = gcm_encrypt(frame, (uint8_t *)&payload, sizeof(payload),
                            NODE_ID, seq++, node_random_id);

  int state = radio.transmit(frame, len);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.printf("TX OK seq=%lu\n", seq - 1);
    if (current_error_code == ERR_TX_FAILED) {
      current_error_code = ERR_NONE; // Clear previous TX error if successful
    }
  } else {
    Serial.printf("TX failed: %d\n", state);
    current_error_code = ERR_TX_FAILED; // Store error for next transmission
  }
  esp_task_wdt_reset();
  delay(5000);
}