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
  out += "# HELP lora_temperature_celsius Temperature from node\n";
  out += "# TYPE lora_temperature_celsius gauge\n";
  out += "# HELP lora_humidity_percent Humidity from node\n";
  out += "# TYPE lora_humidity_percent gauge\n";
  out += "# HELP lora_pressure_hpa Atmospheric pressure in hPa\n";
  out += "# TYPE lora_pressure_hpa gauge\n";
  out += "# HELP lora_light_lux Light level in Lux\n";
  out += "# TYPE lora_light_lux gauge\n";
  out += "# HELP lora_battery_millivolts Node battery voltage in mV\n";
  out += "# TYPE lora_battery_millivolts gauge\n";
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
    char label[64];
    if (nodes[i].name[0] != '\0') {
      snprintf(label, sizeof(label), "{node=\"%d\",name=\"%s\"}", i, nodes[i].name);
    } else {
      snprintf(label, sizeof(label), "{node=\"%d\"}", i);
    }
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
    if (nodes[i].has_pressure) {
      snprintf(line, sizeof(line), "lora_pressure_hpa%s %.2f\n", label,
               nodes[i].pressure);
      out += line;
    }
    if (nodes[i].has_light) {
      snprintf(line, sizeof(line), "lora_light_lux%s %u\n", label,
               nodes[i].light);
      out += line;
    }
    if (nodes[i].has_battery) {
      snprintf(line, sizeof(line), "lora_battery_millivolts%s %u\n", label,
               nodes[i].battery);
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

void handleNodesJson() {
  String json = "{\"nodes\":[";
  bool first = true;
  uint32_t now = millis();
  for (int i = 0; i < MAX_NODES; i++) {
    if (!nodes[i].seen) continue;
    
    // Cache les nœuds inactifs depuis plus de 5 minutes
    if (now - nodes[i].last_seen_ms > 300000UL) continue;

    if (!first) json += ",";
    first = false;

    json += "{";
    json += "\"id\":" + String(i) + ",";
    json += "\"name\":\"" + String(nodes[i].name) + "\",";
    json += "\"seq\":" + String(nodes[i].seq) + ",";
    json += "\"rssi\":" + String(nodes[i].rssi, 1) + ",";
    json += "\"snr\":" + String(nodes[i].snr, 1) + ",";
    json += "\"reboots\":" + String(nodes[i].reboots) + ",";
    json += "\"loss_percent\":" + String(nodes[i].loss_percent, 1) + ",";
    json += "\"packets_count\":" + String(nodes[i].packets_count) + ",";
    json += "\"last_reset_reason\":" + String(nodes[i].last_reset_reason) + ",";
    json += "\"last_error_code\":" + String(nodes[i].last_error_code) + ",";
    json += "\"tx_interval\":" + String(nodes[i].tx_interval) + ",";
    
    String type_str;
    if (nodes[i].type == 1) type_str = "DHT22";
    else if (nodes[i].type == 2) type_str = "AHT20";
    else if (nodes[i].type == 3) type_str = "BMP280";
    else if (nodes[i].type == 4) type_str = "Multi";
    else type_str = "Unknown";
    json += "\"type\":\"" + type_str + "\",";

    json += "\"has_sensor\":" + String(nodes[i].has_sensor ? "true" : "false") + ",";
    json += "\"temperature\":" + String(nodes[i].temperature, 2) + ",";
    json += "\"has_humidity\":" + String(nodes[i].has_humidity ? "true" : "false") + ",";
    json += "\"humidity\":" + String(nodes[i].humidity, 2) + ",";
    json += "\"has_temp_aht\":" + String(nodes[i].has_temp_aht ? "true" : "false") + ",";
    json += "\"temp_aht\":" + String(nodes[i].temp_aht, 2) + ",";
    json += "\"has_temp_bmp\":" + String(nodes[i].has_temp_bmp ? "true" : "false") + ",";
    json += "\"temp_bmp\":" + String(nodes[i].temp_bmp, 2) + ",";

    json += "\"has_pressure\":" + String(nodes[i].has_pressure ? "true" : "false") + ",";
    json += "\"pressure\":" + String(nodes[i].pressure, 1) + ",";

    json += "\"has_light\":" + String(nodes[i].has_light ? "true" : "false") + ",";
    json += "\"light\":" + String(nodes[i].light) + ",";

    json += "\"has_battery\":" + String(nodes[i].has_battery ? "true" : "false") + ",";
    json += "\"battery\":" + String(nodes[i].battery) + ",";

    uint32_t elapsed_sec = (now - nodes[i].last_seen_ms) / 1000;
    json += "\"elapsed_sec\":" + String(elapsed_sec);
    json += "}";
  }
  json += "],";
  json += "\"gateway\":{";
  json += "\"uptime_sec\":" + String(millis() / 1000) + ",";
  json += "\"free_heap_kb\":" + String(ESP.getFreeHeap() / 1024) + ",";
  json += "\"rx_interrupts\":" + String(global_rx_interrupts) + ",";
  json += "\"malformed_packets\":" + String(global_malformed_packets) + ",";
  json += "\"wifi_rssi\":" + String(WiFi.RSSI());
  json += "}";
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleAdminHtml() {
  if (!server.authenticate("admin", admin_pass.c_str())) {
    server.requestAuthentication(BASIC_AUTH, "LoRa Gateway Admin", "Authentification requise");
    return;
  }
  server.send(200, "text/html", ADMIN_HTML);
}

void handleSaveConfigHttp() {
  if (!server.authenticate("admin", admin_pass.c_str())) {
    server.requestAuthentication(BASIC_AUTH, "LoRa Gateway Admin", "Authentification requise");
    return;
  }
  String body = server.arg("plain");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  saveConfig(doc);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"saved\"}");
  delay(1000);
  shouldReboot = true;
}

void handleResetConfigHttp() {
  if (!server.authenticate("admin", admin_pass.c_str())) {
    server.requestAuthentication(BASIC_AUTH, "LoRa Gateway Admin", "Authentification requise");
    return;
  }
  Preferences tempPrefs;
  tempPrefs.begin("gw_cfg", false);
  tempPrefs.clear();
  tempPrefs.end();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"reseted\"}");
  delay(1000);
  shouldReboot = true;
}

void handleGetConfigHttp() {
  if (!server.authenticate("admin", admin_pass.c_str())) {
    server.requestAuthentication(BASIC_AUTH, "LoRa Gateway Admin", "Authentification requise");
    return;
  }
  JsonDocument resp;
  resp["wifi_ssid"] = wifi_ssid;
  resp["wifi_pass"] = wifi_pass;
  resp["admin_pass"] = admin_pass;
  resp["use_static"] = use_static_ip;
  resp["local_ip"] = local_IP.toString();
  resp["gateway_ip"] = gateway.toString();
  resp["subnet_mask"] = subnet.toString();
  
  char keyHex[33];
  for (int i = 0; i < 16; i++) {
    sprintf(keyHex + i * 2, "%02x", AES_KEY[i]);
  }
  keyHex[32] = '\0';
  resp["aes_key"] = String(keyHex);
  
  String out;
  serializeJson(resp, out);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", out);
}

void handleRootHtml() {
  server.send(200, "text/html", INDEX_HTML);
}
