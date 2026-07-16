void changePage() {
  int active_count = 0;
  for (int i = 0; i < MAX_NODES; i++) {
    if (nodes[i].seen) active_count++;
  }
  
  int total_pages = active_count + 2; // Overview (1) + Système (1) + N nœuds
  current_page = (current_page + 1) % total_pages;
  scroll_index = 0;
  updateDisplay();
}

void scrollDown() {
  // Collecter les nœuds actifs
  int active_indices[MAX_NODES];
  int active_count = 0;
  for (int i = 0; i < MAX_NODES; i++) {
    if (nodes[i].seen) {
      active_indices[active_count++] = i;
    }
  }

  if (current_page == 0) {
    // Défiler uniquement les nœuds en ligne (moins de 300s)
    int online_count = 0;
    uint32_t now = millis();
    for (int i = 0; i < MAX_NODES; i++) {
      if (nodes[i].seen) {
        uint32_t elapsed_sec = (now - nodes[i].last_seen_ms) / 1000;
        if (elapsed_sec < 300) {
          online_count++;
        }
      }
    }
    if (online_count > 0) {
      scroll_index = (scroll_index + 1) % online_count;
    }
  }
  else if (current_page == 1) {
    // Défiler la liste système
    scroll_index = (scroll_index + 1) % 6;
  } 
  else {
    // Défiler les détails du nœud courant (current_page - 2)
    scroll_index = scroll_index + 1;
  }
  updateDisplay();
}

void updateDisplay() {
  if (!oled_initialized) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  uint32_t now = millis();

  // Collecter les nœuds vus
  int active_indices[MAX_NODES];
  int active_count = 0;
  for (int i = 0; i < MAX_NODES; i++) {
    if (nodes[i].seen) {
      active_indices[active_count++] = i;
    }
  }

  int total_pages = active_count + 2;
  if (current_page >= total_pages) {
    current_page = 0;
  }

  if (current_page == 0) {
    // --- Page 0: Overview (Vue d'ensemble) ---
    display.setCursor(0, 0);
    display.print("GW LoRa | Nodes");
    display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

    // Collecter uniquement les nœuds actifs (moins de 300s)
    int online_indices[MAX_NODES];
    int online_count = 0;
    for (int i = 0; i < MAX_NODES; i++) {
      if (nodes[i].seen) {
        uint32_t elapsed_sec = (now - nodes[i].last_seen_ms) / 1000;
        if (elapsed_sec < 300) {
          online_indices[online_count++] = i;
        }
      }
    }

    if (online_count > 0) {
      scroll_index = scroll_index % online_count;
      for (int row = 0; row < 4; row++) {
        if (row >= online_count) break;
        int idx = online_indices[(scroll_index + row) % online_count];
        int y = 15 + row * 11;
        display.setCursor(0, y);

        const char* bars = getRssiBars(nodes[idx].rssi);
        char node_label[9];
        if (nodes[idx].name[0] != '\0') {
          snprintf(node_label, sizeof(node_label), "%-8.8s", nodes[idx].name);
        } else {
          snprintf(node_label, sizeof(node_label), "Node %-2d", idx);
        }
        display.printf("%-16.16s%-4.4s", node_label, bars);
      }
    } else {
      display.setCursor(0, 24);
      display.print("No active nodes");
      display.setCursor(0, 42);
      int dot_count = (millis() / 500) % 4;
      char dots[4] = {0};
      for (int i = 0; i < dot_count; i++) dots[i] = '.';
      display.printf("Searching signal%s", dots);
    }
  } 
  else if (current_page == 1) {
    // --- Page 1: Système ---
    display.setCursor(0, 0);
    display.print("GW LoRa | System");
    display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

    String sys_items[6];
    sys_items[0] = "IP:   " + WiFi.localIP().toString();
    sys_items[1] = "SSID: " + wifi_ssid;

    uint32_t sec = now / 1000;
    uint32_t min = sec / 60;
    uint32_t hr = min / 60;
    char up_str[32];
    if (hr > 0) {
      snprintf(up_str, sizeof(up_str), "Up:   %luh%02lum%02lus", hr, min % 60, sec % 60);
    } else {
      snprintf(up_str, sizeof(up_str), "Up:   %lum %lus", min, sec % 60);
    }
    sys_items[2] = String(up_str);
    sys_items[3] = "WiFi: " + String(WiFi.RSSI()) + " dBm";
    sys_items[4] = "RAM:  " + String(ESP.getFreeHeap() / 1024) + " KB";
    sys_items[5] = "Ints: " + String(global_rx_interrupts);

    scroll_index = scroll_index % 6;
    for (int row = 0; row < 4; row++) {
      int idx = (scroll_index + row) % 6;
      display.setCursor(0, 15 + row * 11);
      display.print(sys_items[idx]);
    }
  }
  else {
    // --- Page 2+: Détails du Nœud courant ---
    int idx = current_page - 2;
    if (idx < active_count) {
      int node_id = active_indices[idx];
      NodeData &n = nodes[node_id];

      display.setCursor(0, 0);
      if (n.name[0] != '\0') {
        display.printf("GW LoRa | %.7s", n.name);
      } else {
        display.printf("GW LoRa | Node %d", node_id);
      }
      display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

      String det_items[24];
      int det_count = 0;

      if (n.name[0] != '\0') {
        det_items[det_count++] = "Name: " + String(n.name);
      }
      det_items[det_count++] = "Node: ID " + String(node_id);
      String type_str;
      if (n.type == 1) type_str = "DHT22";
      else if (n.type == 2) type_str = "AHT20";
      else if (n.type == 3) type_str = "BMP280";
      else if (n.type == 4) type_str = "Multi";
      else type_str = "Unknown";
      det_items[det_count++] = "Type: " + type_str;
      det_items[det_count++] = "Seq:  " + String(n.seq);
      if (n.has_sensor) {
        det_items[det_count++] = "Temp: " + String(n.temperature, 1) + " C";
        det_items[det_count++] = "Hum:  " + String(n.humidity, 1) + " %";
      }
      if (n.has_pressure) {
        det_items[det_count++] = "Pres: " + String(n.pressure, 1) + " hPa";
      }
      if (n.has_light) {
        det_items[det_count++] = "Lux:  " + String(n.light) + " lx";
      }
      if (n.has_battery) {
        det_items[det_count++] = "Batt: " + String(n.battery) + " mV";
      }
      det_items[det_count++] = "RSSI: " + String(n.rssi, 0) + " dBm " + String(getRssiBars(n.rssi));
      det_items[det_count++] = "SNR:  " + String(n.snr, 1);
      det_items[det_count++] = "Reboots:  " + String(n.reboots);

      // Temps écoulé depuis la dernière réception
      uint32_t elapsed_sec = (now - n.last_seen_ms) / 1000;
      if (elapsed_sec < 60) {
        det_items[det_count++] = "Seen: " + String(elapsed_sec) + "s ago";
      } else {
        det_items[det_count++] = "Seen: " + String(elapsed_sec / 60) + "m ago";
      }

      // Taux de perte et total des paquets
      det_items[det_count++] = "Loss: " + String(n.loss_percent, 1) + " %";
      det_items[det_count++] = "Packets: " + String(n.packets_count);

      // Raison de reboot humaine
      String rst_str;
      switch(n.last_reset_reason) {
        case 1: rst_str = "PowerOn"; break;
        case 3: rst_str = "Software"; break;
        case 4: rst_str = "Panic"; break;
        case 5:
        case 6:
        case 7: rst_str = "Watchdog"; break;
        case 8: rst_str = "DeepSleep"; break;
        case 9: rst_str = "Brownout"; break;
        default: rst_str = "Code " + String(n.last_reset_reason);
      }
      det_items[det_count++] = "Reset: " + rst_str;
      det_items[det_count++] = "ErrCode: " + String(n.last_error_code);

      scroll_index = scroll_index % det_count;
      for (int row = 0; row < 4; row++) {
        int idx_det = (scroll_index + row) % det_count;
        display.setCursor(0, 15 + row * 11);
        display.print(det_items[idx_det]);
      }
    }
  }
  display.display();
}
