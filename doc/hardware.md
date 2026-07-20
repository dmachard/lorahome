# Hardware Pinouts & Wiring Guide

This document describes the physical pinouts and connections for both the **ESP32-C6 Central Gateway** and the **ESP32-C3 Telemetry Client Nodes**.

---

## 1. Gateway Hardware Setup (ESP32-C6)

The Gateway uses an **ESP32-C6** microcontroller board paired with a LoRa module and an OLED display.

### 1.1 SPI LoRa Transceiver Wiring

| Signal Name | ESP32-C6 Pin | SX1262 Pin | SX1278 Pin | Description |
|---|---|---|---|---|
| **SPI SCK** | `GPIO 6` | SCK | SCK | SPI Clock |
| **SPI MISO** | `GPIO 2` | MISO | MISO | SPI Master In Slave Out |
| **SPI MOSI** | `GPIO 7` | MOSI | MOSI | SPI Master Out Slave In |
| **LoRa CS** | `GPIO 10` | NSS / CS | NSS / CS | Chip Select |
| **LoRa RST** | `GPIO 11` | NRESET | NRESET | Hardware Reset |
| **LoRa BUSY / DIO0** | `GPIO 12` (SX1262) / `GPIO 1` (SX1278) | BUSY | DIO0 | Interrupt / Busy line |
| **LoRa DIO1** | `GPIO 13` (SX1262) | DIO1 | - | Radio Interrupt Line |

### 1.2 OLED Display (SSD1306 128x64 I2C)

| Signal Name | ESP32-C6 Pin | SSD1306 OLED Pin | Description |
|---|---|---|---|
| **I2C SDA** | `GPIO 4` | SDA | Serial Data |
| **I2C SCL** | `GPIO 5` | SCL | Serial Clock |
| **VCC** | `3.3V` | VCC | Power Supply |
| **GND** | `GND` | GND | Ground |

---

## 2. Node Hardware Setup (ESP32-C3)

The Telemetry Client Node uses an **ESP32-C3** board (e.g. SuperMini or Seeed Studio XIAO ESP32-C3).

### 2.1 SPI LoRa Transceiver Wiring

| Signal Name | ESP32-C3 Pin | SX1262 Pin | SX1278 Pin | Description |
|---|---|---|---|---|
| **SPI SCK** | `GPIO 6` | SCK | SCK | SPI Clock |
| **SPI MISO** | `GPIO 2` | MISO | MISO | SPI Master In Slave Out |
| **SPI MOSI** | `GPIO 7` | MOSI | MOSI | SPI Master Out Slave In |
| **LoRa CS** | `GPIO 10` | NSS / CS | NSS / CS | Chip Select |
| **LoRa RST** | `GPIO 0` | NRESET | NRESET | Hardware Reset |
| **LoRa BUSY / DIO0** | `GPIO 5` (SX1262) / `GPIO 1` (SX1278) | BUSY | DIO0 | Interrupt / Busy line |
| **LoRa DIO1** | `GPIO 8` (SX1262) | DIO1 | - | Radio Interrupt Line |

### 2.2 I2C Sensors & User Controls

| Signal / Control | ESP32-C3 Pin | Connected Hardware | Notes |
|---|---|---|---|
| **I2C SDA** | `GPIO 3` | AHT20, BMP280, TSL2561, SCD41 SDA pins | Shared I2C Bus |
| **I2C SCL** | `GPIO 4` | AHT20, BMP280, TSL2561, SCD41 SCL pins | Shared I2C Bus |
| **BOOT Button** | `GPIO 9` | On-board BOOT push button | Active LOW (Input Pullup). Press during boot to enter BLE mode |
| **Status LED** | `GPIO 8` | On-board LED or External LED | Blinks during BLE config mode; solid during reboot |
