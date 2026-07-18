#include <WiFi.h>
#include <Adafruit_SSD1306.h>

extern String wifi_ssid;
extern String wifi_pass;
extern bool use_static_ip;
extern IPAddress local_IP;
extern IPAddress gateway;
extern IPAddress subnet;
extern bool oled_initialized;
extern Adafruit_SSD1306 display;

void updateDisplay();

void initWiFi() {
  if (oled_initialized) {
    display.println("Connecting WiFi...");
    display.display();
  }

  WiFi.mode(WIFI_STA);
  if (use_static_ip) {
    if (!WiFi.config(local_IP, gateway, subnet)) {
      Serial.println("STA Failed to configure");
    }
  }
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  
  int wifi_retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    wifi_retry++;
    if (oled_initialized && wifi_retry % 2 == 0) {
      display.print(".");
      display.display();
    }
    if (wifi_retry > 40) { // 20 seconds timeout
      if (oled_initialized) {
        display.println("\nWiFi: FAILED!");
        display.display();
      }
      Serial.println("\nWiFi connection failed!");
      delay(2000);
      break;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());
  }
  updateDisplay();
}
