# ESP32 LoRa Gateway & Sensor Nodes System

An end-to-end telemetry system featuring an **ESP32 Web Gateway** and multiple **ESP32 sensor client nodes** transmitting encrypted data using **LoRa (433 MHz)** and **AES-128 GCM** security. The gateway acts as a web server exporting **Prometheus metrics** (`/metrics`) for visualization in Grafana.

Based on ESP32-C6, ESP32-S3 and using LoRa SX1262 and SX1278 transceivers.

## 📁 Architecture

### 1. Gateway
Acts as the central receiver and metrics exporter.
*   **Microcontroller:** ESP32-C6
*   **LoRa Transceiver:** SX1262
*   **Wi-Fi Connection:** Connected to Local Wi-Fi with a static IP.
*   **Prometheus Metrics:** Exposes a web server on port `8080` at path `/metrics`.

#### Gateway Pinout (SX1262 SPI)
*   `SCK`: `6`
*   `MISO`: `2`
*   `MOSI`: `7`
*   `NSS/CS`: `10`
*   `RST`: `11`
*   `BUSY`: `12`
*   `DIO1`: `13` (Interrupt-driven RX)


---

### 2. Client SX1262 Ping 
Simple node sending sequence numbers for range/hardware testing.
*   **Microcontroller:** ESP32-S3
*   **LoRa Transceiver:** SX1262
*   **Node ID:** `1`

#### SX1262 Pinout
*   `SCK`: `6` | `MISO`: `2` | `MOSI`: `7` | `CS`: `10`
*   `RST`: `0` | `BUSY`: `5` | `DIO1`: `8`


---

### 3. Client AHT20 
Telemetry node reading temperature and humidity from an AHT20 sensor, sending GCM-encrypted LoRa payloads.
*   **Microcontroller:** ESP32-S3
*   **LoRa Transceiver:** SX1262
*   **Sensor:** AHT20 (I2C)
*   **Node ID:** `2`

#### Pinout & Connections
*   **SX1262 SPI:** `SCK` = `6`, `MISO` = `2`, `MOSI` = `7`, `CS` = `10`, `RST` = `0`, `BUSY` = `5`, `DIO1` = `8`
*   **AHT20 I2C:** `SDA` = `3`, `SCL` = `4`

---

### 4. Client DHT22
Telemetry node reading from a DHT22 sensor, transmitting encrypted packets using an SX1278 transceiver.
*   **Microcontroller:** ESP32-S3
*   **LoRa Transceiver:** SX1278
*   **Sensor:** DHT22 (Single-wire digital)
*   **Node ID:** `1`

#### Pinout & Connections
*   **SX1278 SPI:** `SCK` = `6`, `MISO` = `2`, `MOSI` = `7`, `CS` = `10`, `RST` = `0`, `DIO0` = `8`, `DIO1` = `-1`
*   **DHT22:** Pin `4`


---

## 📊 Available Prometheus Metrics
The gateway exports the following metrics on port `8080` at `/metrics` (all labeled by `{node="<ID>"}`):
*   `lora_rssi_dbm` - Last packet Signal Strength Indication (dBm)
*   `lora_snr_db` - Last packet Signal-to-Noise Ratio (dB)
*   `lora_last_seen_seconds` - Uptime duration since the last received packet
*   `lora_packet_seq` - Latest packet sequence counter
*   `lora_packet_loss_percent` - Calculated packet loss percentage over a 5-minute rolling window
*   `lora_reboots` - Number of client-side reboots detected
*   `lora_temperature_celsius` & `lora_humidity_percent` - Decrypted sensor readings
*   `lora_node_reset_reason` - Remote ESP32 restart cause code (e.g. `1` = Power-on, `6` = Task Watchdog)
*   `lora_node_error_code` - Remote sensor states (`0` = OK, `1` = Sensor Failure, `2` = TX Failure)


---

## 📡 LoRa Configuration

All devices share the following radio parameters:
*   **Frequency:** `433.0 MHz`
*   **Bandwidth:** `125.0 kHz`
*   **Spreading Factor (SF):** `7`
*   **Coding Rate (CR):** `5`
*   **Preamble Length:** `8`
*   **Sync Word:** `RADIOLIB_SX126X_SYNC_WORD_PRIVATE` (or `0x12` for SX1278 node)


---

## 🔒 Security & Packet Format (AES-128 GCM)

Data payload authentication and confidentiality are secured via **AES-128 GCM**.

*   **Pre-Shared Key:** `2B 7E 15 16 28 AE D2 A6 AB F7 15 88 09 CF 4F 3C`
*   **IV (Initialization Vector) Construction:** First 9 bytes of the frame (unencrypted header) padded with 3 bytes of `0x00` to form a 12-byte IV.
*   **Tag Size:** `8 bytes` (attached to the end of the frame).

### Over-The-Air Frame Structure

| Byte Index | Field | Description |
| :--- | :--- | :--- |
| `0` | **Node ID** | `uint8_t` Identifier of the transmitting node |
| `1 - 4` | **Sequence Number** | `uint32_t` Big-endian packet counter |
| `5 - 8` | **Random Session ID** | `uint32_t` Big-endian node session randomizer (resets on boot) |
| `9 - ...` | **Encrypted Payload** | Ciphertext of the payload |
| `End - 7` to `End` | **GCM Tag** | `8 bytes` auth tag |

### Sensor Payload Structure (`SensorPayload` - Packed)

| Offset | Type | Field | Description |
| :--- | :--- | :--- | :--- |
| `0` | `int16_t` | `temperature_x100` | Temperature scaled by 100 (e.g. `2350` = `23.50°C`) |
| `2` | `uint16_t` | `humidity_x100` | Humidity scaled by 100 (e.g. `5520` = `55.20%`) |
| `4` | `uint8_t` | `type` | Sensor Type (`0x02` = DHT22, `0x03` = AHT20) |
| `5` | `uint8_t` | `reset_reason` | ESP32 boot reason code (from `esp_reset_reason()`) |
| `6` | `uint8_t` | `error_code` | Status (`0` = OK, `1` = Sensor Error, `2` = TX Error) |
