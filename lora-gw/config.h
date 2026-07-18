#ifndef CONFIG_H
#define CONFIG_H

#include <RadioLib.h>

// ==========================================
// SYSTEM & WATCHDOG CONFIGURATION
// ==========================================
#define WDT_TIMEOUT_S 15

// ==========================================
// HARDWARE BUTTON CONFIGURATION
// ==========================================
#define BUTTON_PIN 9

// ==========================================
// I2C & OLED SCREEN CONFIGURATION
// ==========================================
#define I2C_SDA 4
#define I2C_SCL 5
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// ==========================================
// LORA PHYSICAL TRANSCEIVER PIN CONFIG
// ==========================================
#define SPI_SCK 6
#define SPI_MISO 2
#define SPI_MOSI 7
#define LORA_CS 10
#define LORA_RST 11
#define LORA_BUSY 12
#define LORA_DIO1 13

// ==========================================
// LORA RF CONFIGURATION (SX1262)
// ==========================================
#define LORA_FREQ 433.0
#define LORA_BW 125.0
#define LORA_SF 9
#define LORA_CR 5
#define LORA_SYNC RADIOLIB_SX126X_SYNC_WORD_PRIVATE
#define LORA_POWER 14
#define LORA_PREAMBLE 8

// ==========================================
// SECURITY & PACKET SIZE CONFIGURATION
// ==========================================
#define TAG_SIZE 8
#define HDR_SIZE 9 // node_id(1) + seq(4) + random_id(4)

// ==========================================
// MONITORING CONFIGURATION
// ==========================================
#define WINDOW_MS 300000UL

#endif // CONFIG_H
