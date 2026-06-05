#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#define LORA_CS 10
#define LORA_RST 0
#define LORA_BUSY 5
#define LORA_DIO1 8
#define SPI_SCK 6
#define SPI_MISO 2
#define SPI_MOSI 7

#define NODE_ID 1

#define LORA_FREQ 433.0
#define LORA_BW 125.0
#define LORA_SF 7
#define LORA_CR 5
#define LORA_SYNC RADIOLIB_SX126X_SYNC_WORD_PRIVATE
#define LORA_POWER 14
#define LORA_PREAMBLE 8

SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

uint8_t seq = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
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
  uint8_t buf[2] = {NODE_ID, seq++};
  int state = radio.transmit(buf, sizeof(buf));
  if (state == RADIOLIB_ERR_NONE) {
    Serial.printf("Ping seq=%d\n", seq - 1);
  } else {
    Serial.printf("TX failed: %d\n", state);
  }
  delay(5000);
}