#include <AES.h>
#include <Arduino.h>
#include <GCM.h>
#include <RadioLib.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>

const char *WIFI_SSID = "xxxx";
const char *WIFI_PASS = "xxxx";

IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

#define SPI_SCK 6
#define SPI_MISO 2
#define SPI_MOSI 7
#define LORA_CS 10
#define LORA_RST 11
#define LORA_BUSY 12
#define LORA_DIO1 13

#define LORA_FREQ 433.0
#define LORA_BW 125.0
#define LORA_SF 7
#define LORA_CR 5
#define LORA_SYNC RADIOLIB_SX126X_SYNC_WORD_PRIVATE
#define LORA_POWER 14
#define LORA_PREAMBLE 8

#define WINDOW_MS 300000UL

uint8_t AES_KEY[16] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
                       0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};

#define TAG_SIZE 8
#define HDR_SIZE 9 // node_id(1) + seq(4) + random_id(4)

#define TYPE_DHT22 0x02
#define TYPE_AHT20 0x03 // Constante correspondant au Node 2

struct __attribute__((packed)) SensorPayload {
  int16_t temperature_x100;
  uint16_t humidity_x100;
  uint8_t type;
  uint8_t reset_reason;
  uint8_t error_code;
};

GCM<AES128> gcm;
uint32_t global_malformed_packets = 0;
uint32_t global_unknown_nodes = 0;
uint32_t global_rx_interrupts = 0;
uint32_t boot_time_ms = 0;

bool gcm_decrypt(const uint8_t *frame, uint8_t frame_len, uint8_t *payload,
                 uint8_t payload_size) {
  if (frame_len < HDR_SIZE + TAG_SIZE)
    return false;
  uint8_t payload_len = frame_len - HDR_SIZE - TAG_SIZE;
  if (payload_len > payload_size)
    return false;

  uint8_t iv[12] = {0};
  memcpy(iv, frame, 9);

  const uint8_t *ciphertext = frame + HDR_SIZE;
  const uint8_t *tag = frame + frame_len - TAG_SIZE;

  gcm.clear();
  gcm.setKey(AES_KEY, 16);
  gcm.setIV(iv, 12);
  gcm.addAuthData(frame, HDR_SIZE);
  gcm.decrypt(payload, ciphertext, payload_len);

  uint8_t computed_tag[TAG_SIZE];
  gcm.computeTag(computed_tag, TAG_SIZE);
  return memcmp(computed_tag, tag, TAG_SIZE) == 0;
}

SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
volatile bool rxFlag = false;
void IRAM_ATTR onReceive() { rxFlag = true; }

#define MAX_NODES 16
struct NodeData {
  uint32_t seq;
  uint32_t last_seen_ms;
  uint32_t packets_count;
  uint32_t auth_failures;
  uint32_t reboots;
  uint32_t window_seq_start;
  uint32_t window_received;
  uint32_t window_start_ms;
  float rssi;
  float snr;
  float loss_percent;
  float temperature;
  float humidity;
  uint8_t last_reset_reason;
  uint8_t last_error_code;
  bool seen;
  bool has_sensor;
};

NodeData nodes[MAX_NODES];

void resetWindow(struct NodeData &n, uint32_t seq) {
  n.window_seq_start = seq;
  n.window_received = 0;
  n.window_start_ms = millis();
  n.loss_percent = 0.0f;
}

void updateWindow(struct NodeData &n, uint32_t seq) {
  uint32_t now = millis();
  if (now - n.window_start_ms >= WINDOW_MS) {
    uint32_t expected = seq - n.window_seq_start;
    if (expected > 0) {
      uint32_t lost = expected - min(n.window_received, expected);
      n.loss_percent = (lost * 100.0f) / expected;
    }
    resetWindow(n, seq);
    return;
  }
  n.window_received++;
}

WebServer server(8080);

void handleMetrics() {
  String out;
  out.reserve(2048);
  out += "# HELP lora_rssi_dbm Last RSSI\n";
  out += "# TYPE lora_rssi_dbm gauge\n";
  out += "# HELP lora_snr_db Last SNR\n";
  out += "# TYPE lora_snr_db gauge\n";
  out += "# HELP lora_last_seen_seconds Seconds since last packet\n";
  out += "# TYPE lora_last_seen_seconds gauge\n";
  out += "# HELP lora_packet_seq Last sequence number\n";
  out += "# TYPE lora_packet_seq counter\n";
  out += "# HELP lora_packet_loss_percent Packet loss over last window\n";
  out += "# TYPE lora_packet_loss_percent gauge\n";
  out += "# HELP lora_auth_failures Total authentication failures\n";
  out += "# TYPE lora_auth_failures counter\n";
  out += "# HELP lora_reboots Total node reboots detected\n";
  out += "# TYPE lora_reboots counter\n";
  out += "# HELP lora_temperature_celsius Temperature from DHT22\n";
  out += "# TYPE lora_temperature_celsius gauge\n";
  out += "# HELP lora_humidity_percent Humidity from DHT22\n";
  out += "# TYPE lora_humidity_percent gauge\n";
  out += "# HELP lora_node_reset_reason Last ESP Reset Reason (1=POWERON)\n";
  out += "# TYPE lora_node_reset_reason gauge\n";
  out += "# HELP lora_node_error_code Node Error Code (0=OK)\n";
  out += "# TYPE lora_node_error_code gauge\n";
  out += "# HELP lora_global_malformed_packets_total Total malformed packets "
         "received\n";
  out += "# HELP lora_global_malformed_packets_total Total malformed packets "
         "received\n";
  out += "# TYPE lora_global_malformed_packets_total counter\n";
  out += "# HELP lora_global_unknown_nodes_total Total packets from unknown "
         "node IDs\n";
  out += "# TYPE lora_global_unknown_nodes_total counter\n";
  out += "# HELP lora_global_rx_interrupts_total Total radio interrupts "
         "received\n";
  out += "# TYPE lora_global_rx_interrupts_total counter\n";
  out += "# HELP lora_gateway_uptime_seconds Gateway uptime in seconds\n";
  out += "# TYPE lora_gateway_uptime_seconds gauge\n";
  out +=
      "# HELP lora_packets_received_total Total successful packets per node\n";
  out += "# TYPE lora_packets_received_total counter\n";

  uint32_t now = millis();
  for (int i = 0; i < MAX_NODES; i++) {
    if (!nodes[i].seen)
      continue;
    char label[32];
    snprintf(label, sizeof(label), "{node=\"%d\"}", i);
    char line[128];
    snprintf(line, sizeof(line), "lora_rssi_dbm%s %.1f\n", label,
             nodes[i].rssi);
    out += line;
    snprintf(line, sizeof(line), "lora_snr_db%s %.1f\n", label, nodes[i].snr);
    out += line;
    snprintf(line, sizeof(line), "lora_last_seen_seconds%s %.1f\n", label,
             (now - nodes[i].last_seen_ms) / 1000.0f);
    out += line;
    snprintf(line, sizeof(line), "lora_packet_seq%s %lu\n", label,
             nodes[i].seq);
    out += line;
    snprintf(line, sizeof(line), "lora_packet_loss_percent%s %.1f\n", label,
             nodes[i].loss_percent);
    out += line;
    snprintf(line, sizeof(line), "lora_auth_failures%s %lu\n", label,
             nodes[i].auth_failures);
    out += line;
    snprintf(line, sizeof(line), "lora_reboots%s %lu\n", label,
             nodes[i].reboots);
    out += line;
    snprintf(line, sizeof(line), "lora_packets_received_total%s %lu\n", label,
             nodes[i].packets_count);
    out += line;
    snprintf(line, sizeof(line), "lora_node_reset_reason%s %u\n", label,
             nodes[i].last_reset_reason);
    out += line;
    snprintf(line, sizeof(line), "lora_node_error_code%s %u\n", label,
             nodes[i].last_error_code);
    out += line;
    if (nodes[i].has_sensor) {
      snprintf(line, sizeof(line), "lora_temperature_celsius%s %.2f\n", label,
               nodes[i].temperature);
      out += line;
      snprintf(line, sizeof(line), "lora_humidity_percent%s %.2f\n", label,
               nodes[i].humidity);
      out += line;
    }
  }
  char global_line[128];
  snprintf(global_line, sizeof(global_line),
           "lora_global_malformed_packets_total %lu\n",
           global_malformed_packets);
  out += global_line;
  snprintf(global_line, sizeof(global_line),
           "lora_global_unknown_nodes_total %lu\n", global_unknown_nodes);
  out += global_line;

  server.send(200, "text/plain; version=0.0.4; charset=utf-8", out);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  boot_time_ms = millis();
  memset(nodes, 0, sizeof(nodes));

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SYNC,
                          LORA_POWER, LORA_PREAMBLE);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Init failed: %d\n", state);
    while (true)
      delay(1000);
  }

  radio.setDio1Action(onReceive);
  radio.startReceive();

  WiFi.mode(WIFI_STA);
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());

  server.on("/metrics", handleMetrics);
  server.on("/", []() { server.send(200, "text/plain", "LoRa Gateway"); });
  server.begin();
}

void loop() {
  if (rxFlag) {
    rxFlag = false;
    global_rx_interrupts++;
    uint8_t frame[32];
    int state = radio.readData(frame, sizeof(frame));
    int len = radio.getPacketLength();

    if (state == RADIOLIB_ERR_NONE) {
      if (len >= HDR_SIZE + TAG_SIZE) {
        uint8_t node_id = frame[0];
        uint32_t seq = ((uint32_t)frame[1] << 24) | ((uint32_t)frame[2] << 16) |
                       ((uint32_t)frame[3] << 8) | ((uint32_t)frame[4]);
        uint8_t payload_len = len - HDR_SIZE - TAG_SIZE;
        uint8_t payload[16] = {0};

        if (node_id < MAX_NODES) {
          if (payload_len > sizeof(payload)) {
            global_malformed_packets++;
            Serial.printf("Node %d | Paquet trop grand (%d bytes)\n", node_id,
                          len);
            radio.startReceive();
            return;
          }
          NodeData &n = nodes[node_id];
          if (gcm_decrypt(frame, len, payload, sizeof(payload))) {
            uint8_t current_reset_reason = 0;
            uint8_t current_error_code = 0;

            bool is_sensor_payload = (payload_len >= 7);
            SensorPayload sp;

            if (is_sensor_payload) {
              memcpy(&sp, payload, 7); // Use exactly 7 safe bytes
              current_reset_reason = sp.reset_reason;
              current_error_code = sp.error_code;
            }

            if (n.seen) {
              if (seq == 0 &&
                  current_reset_reason == 1) { // 1 = ESP_RST_POWERON
                Serial.printf("Node %d | LEGITIMATE REBOOT (POWERON) DETECTE\n",
                              node_id);
                n.reboots++;
                resetWindow(n, seq);
              } else if (seq < n.seq) {
                Serial.printf("Node %d | WARNING: UNEXPECTED REBOOT DETECTE "
                              "(seq < n.seq)\n",
                              node_id);
                n.reboots++;
                resetWindow(n, seq);
              } else if (seq == n.seq) {
                // doublon, ignorer
              } else {
                updateWindow(n, seq);
              }
            } else {
              resetWindow(n, seq);
            }
            n.seen = true;
            n.seq = seq;
            n.rssi = radio.getRSSI();
            n.snr = radio.getSNR();
            n.last_seen_ms = millis();
            n.packets_count++;
            n.last_reset_reason = current_reset_reason;
            n.last_error_code = current_error_code;

            if (is_sensor_payload) {
              if ((sp.type == TYPE_DHT22 || sp.type == TYPE_AHT20) &&
                  current_error_code == 0) {
                n.temperature = sp.temperature_x100 / 100.0f;
                n.humidity = sp.humidity_x100 / 100.0f;
                n.has_sensor = true;
              } else {
                n.has_sensor = false;
                if (sp.type != TYPE_DHT22 && sp.type != TYPE_AHT20) {
                  Serial.printf("Node %d | Type de capteur inconnu : 0x%02X\n",
                                node_id, sp.type);
                } else if (current_error_code != 0) {
                  Serial.printf("Node %d | Capteur en erreur : code %d\n",
                                node_id, current_error_code);
                }
              }
            } else {
              n.has_sensor = false;
              Serial.printf("Node %d | Payload ignoré car taille inattendue : "
                            "recu=%d bytes\n",
                            node_id, payload_len);
            }
          } else {
            n.auth_failures++;
            Serial.printf("Node %d | AUTH FAILED\n", node_id);
          }
        } else {
          global_unknown_nodes++;
          Serial.printf("Node %d | UNKNOWN NODE\n", node_id);
        }
      } else {
        global_malformed_packets++;
        Serial.printf("Packet too short: %d bytes\n", len);
      }
    } else {
      global_malformed_packets++;
    }
    radio.startReceive();
  }
  server.handleClient();
}