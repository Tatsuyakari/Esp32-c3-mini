#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "wifi_manager.h"
#include "espnow_manager.h"

// Khởi tạo web server chạy trên cổng 80
AsyncWebServer server(80);

// Biến trạng thái ESP-NOW
bool is_master = IS_MASTER; // Được quy định bằng define

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Tắt LED khi khởi động
  
  Serial.begin(115200);
  delay(1000);
  
  // In thông tin về chế độ hoạt động
  Serial.println("🚀 ESP32 C3 WiFi Manager đang khởi động...");
  Serial.println(is_master ? "🔷 Khởi động ở chế độ MASTER" : "🔶 Khởi động ở chế độ SLAVE");

  // Tắt Watchdog Timer
  disableWDT();

  // Khởi động SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("❌ Lỗi mount SPIFFS, thử format...");
    SPIFFS.format();
    if (!SPIFFS.begin())
    {
      Serial.println("❌ SPIFFS mount thất bại!");
      return;
    }
  }
  Serial.println("✅ SPIFFS đã sẵn sàng");

  // Thiết lập Access Point
  setupAP();

  // Khởi tạo ESP-NOW
  espNowActive = initESPNow();
  if (espNowActive) {
    Serial.println("✅ ESP-NOW đã sẵn sàng");
    
    // Khởi tạo trạng thái LED cho slave
    if (!is_master) {
      slaveLedState = false;
      slaveConnectedToMaster = false;
      digitalWrite(LED_PIN, LOW); // Tắt LED ban đầu
    }
  } else {
    Serial.println("❌ Khởi tạo ESP-NOW thất bại");
  }

  // Cấu hình các route cho web server
  // Phục vụ các file tĩnh từ SPIFFS
  server.serveStatic("/", SPIFFS, "/");
  
  // API WiFi
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/connect", HTTP_POST, handleConnect);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/disconnect", HTTP_GET, handleDisconnect);
  
  // API ESP-NOW
  server.on("/api/espnow/scan", HTTP_GET, handleESPNowScan);
  server.on("/api/espnow/list", HTTP_GET, handleESPNowList);
  server.on("/api/espnow/connect", HTTP_POST, handleESPNowConnect);
  server.on("/api/espnow/toggle", HTTP_POST, handleESPNowToggleLED);
  server.on("/api/espnow/slave/toggle", HTTP_POST, handleSlaveToggleLED);

  // Xử lý 404 Not Found
  server.onNotFound([](AsyncWebServerRequest *request)
                    { request->send(SPIFFS, "/index.html", "text/html"); });

  server.begin();
  Serial.println("🌐 HTTP server đã chạy!");
}

void loop()
{
  // Xử lý ESP-NOW
  if (!is_master && espNowActive) {
    static unsigned long lastBlink = 0;
    static unsigned long lastDiscovery = 0;
    unsigned long currentTime = millis();
    
    // Gửi tín hiệu discovery mỗi 5 giây
    if (currentTime - lastDiscovery > 5000) {
      lastDiscovery = currentTime;
      
      // Gửi tín hiệu broadcast để master phát hiện
      uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, broadcastMac, 6);
      peerInfo.channel = 0;  // Sử dụng kênh hiện tại
      peerInfo.encrypt = false;
      
      // Xóa peer cũ nếu đã tồn tại
      esp_now_del_peer(broadcastMac);
      
      // Thêm peer broadcast
      esp_err_t result = esp_now_add_peer(&peerInfo);
      if (result == ESP_OK) {
        // Gửi tin nhắn broadcast
        espnow_message_t discMsg;
        discMsg.type = 0;  // Discovery
        discMsg.data = slaveLedState ? 1 : 0;  // Gửi trạng thái LED hiện tại
        strncpy(discMsg.name, AP_SSID, sizeof(discMsg.name));
        
        esp_now_send(broadcastMac, (uint8_t *)&discMsg, sizeof(discMsg));
        Serial.println("📢 Gửi tín hiệu broadcast");
      }
    }
    
    // Nhấp nháy LED khi chưa kết nối với master
    if (!slaveConnectedToMaster) {
      if (currentTime - lastBlink > 500) {  // Nhấp nháy mỗi 0.5 giây
        lastBlink = currentTime;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      }
    } else {
      // Khi đã kết nối, hiển thị trạng thái LED đã lưu
      digitalWrite(LED_PIN, slaveLedState);
    }
  }
  
  // Master định kỳ quét tìm slave mới
  if (is_master && espNowActive) {
    static unsigned long lastScan = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastScan > 30000) {  // Quét mỗi 30 giây
      lastScan = currentTime;
      scanESPNowDevices();
    }
  }
}