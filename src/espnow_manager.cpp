#include "espnow_manager.h"

// Định nghĩa biến toàn cục
PeerInfo peerList[MAX_PEERS];
int peerCount = 0;
bool espNowActive = false;
bool slaveLedState = false;
bool slaveConnectedToMaster = false;

// Hàm gửi tin nhắn ESP-NOW với retry
bool sendESPNowMessage(const uint8_t *mac, uint8_t type, uint8_t data) {
  espnow_message_t msg;
  msg.type = type;
  msg.data = data;
  strncpy(msg.name, AP_SSID, sizeof(msg.name));

  for (int retry = 0; retry < MAX_RETRIES; retry++) {
    esp_err_t result = esp_now_send(mac, (uint8_t *)&msg, sizeof(msg));
    if (result == ESP_OK) {
      return true;
    }
    delay(100 * (retry + 1));  // Tăng dần thời gian chờ giữa các lần thử
  }
  return false;
}

// Callback khi gửi dữ liệu ESP-NOW
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial.printf("❌ Gửi thất bại đến %s\n", macStr);
    // Tăng retry count cho peer này
    for (int i = 0; i < peerCount; i++) {
      if (memcmp(peerList[i].mac, mac_addr, 6) == 0) {
        peerList[i].retryCount++;
        if (peerList[i].retryCount >= MAX_RETRIES) {
          peerList[i].connected = false;  // Đánh dấu mất kết nối sau nhiều lần thử
        }
        break;
      }
    }
  } else {
    Serial.printf("✅ Gửi thành công đến %s\n", macStr);
  }
}

// Callback khi nhận dữ liệu ESP-NOW
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("📥 Nhận từ: %s\n", macStr);

  espnow_message_t *message = (espnow_message_t *)data;

  if (is_master) {
    // Master nhận dữ liệu
    bool found = false;
    int deviceIndex = -1;
    
    // Tìm hoặc thêm thiết bị vào danh sách
    for (int i = 0; i < peerCount; i++) {
      if (memcmp(peerList[i].mac, mac, 6) == 0) {
        deviceIndex = i;
        found = true;
        // Cập nhật thông tin thiết bị
        peerList[i].lastSeen = millis();
        peerList[i].retryCount = 0;  // Reset retry count khi nhận được phản hồi
        peerList[i].name = String(message->name);
        peerList[i].rssi = WiFi.RSSI();
        break;
      }
    }

    if (!found && peerCount < MAX_PEERS) {
      deviceIndex = peerCount;
      memcpy(peerList[peerCount].mac, mac, 6);
      peerList[peerCount].name = String(message->name);
      peerList[peerCount].connected = false;
      peerList[peerCount].rssi = WiFi.RSSI();
      peerList[peerCount].channel = WiFi.channel();
      peerList[peerCount].ledState = false;
      peerList[peerCount].lastSeen = millis();
      peerList[peerCount].retryCount = 0;
      peerCount++;
      Serial.printf("🆕 Thêm thiết bị mới: %s\n", message->name);
    }

    // Xử lý tin nhắn dựa vào loại
    switch (message->type) {
      case 0:  // Discovery response
        if (deviceIndex >= 0 && !peerList[deviceIndex].connected) {
          // Tự động kết nối khi nhận discovery
          if (addPeer(peerList[deviceIndex].mac, peerList[deviceIndex].channel)) {
            peerList[deviceIndex].connected = true;
            Serial.printf("✅ Tự động kết nối với %s\n", peerList[deviceIndex].name.c_str());
          }
        }
        break;
        
      case 2:  // Status update
        if (deviceIndex >= 0) {
          peerList[deviceIndex].ledState = message->data;
          Serial.printf("🔄 Cập nhật LED %s: %s\n", 
                       peerList[deviceIndex].name.c_str(), 
                       message->data ? "BẬT" : "TẮT");
        }
        break;
    }
  } else {
    // Slave nhận dữ liệu
    switch (message->type) {
      case 0:  // Discovery request
        slaveConnectedToMaster = true;
        // Gửi phản hồi với retry
        sendESPNowMessage(mac, 0, slaveLedState ? 1 : 0);
        break;
        
      case 1:  // Command
      case 3:  // Toggle command
        if (message->type == 3 || message->data == 2) {
          toggleSlaveLed();  // Toggle LED
        } else {
          slaveLedState = message->data == 1;  // Set trực tiếp trạng thái
          digitalWrite(LED_PIN, slaveLedState);
        }
        // Gửi trạng thái mới về master
        sendESPNowMessage(mac, 2, slaveLedState ? 1 : 0);
        break;
    }
  }
}

// Hàm điều khiển LED trên slave
void toggleSlaveLed() {
  slaveLedState = !slaveLedState;
  digitalWrite(LED_PIN, slaveLedState);
  Serial.printf("💡 LED đã %s\n", slaveLedState ? "BẬT" : "TẮT");
}

// Hàm dọn dẹp các thiết bị không hoạt động
void cleanupStaleDevices() {
  unsigned long currentTime = millis();
  int i = 0;
  
  while (i < peerCount) {
    if (currentTime - peerList[i].lastSeen > PEER_TIMEOUT) {
      // Xóa peer khỏi ESP-NOW nếu đã kết nối
      if (peerList[i].connected) {
        esp_now_del_peer(peerList[i].mac);
      }
      
      // Xóa khỏi danh sách bằng cách dịch các phần tử còn lại
      for (int j = i; j < peerCount - 1; j++) {
        memcpy(&peerList[j], &peerList[j + 1], sizeof(PeerInfo));
      }
      peerCount--;
      Serial.printf("🗑️ Xóa thiết bị không hoạt động: %s\n", peerList[i].name.c_str());
    } else {
      i++;
    }
  }
}

// Hàm khởi tạo ESP-NOW
bool initESPNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Khởi tạo ESP-NOW thất bại");
    return false;
  }

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  
  Serial.println("✅ Khởi tạo ESP-NOW thành công");
  return true;
}

// Hàm thêm peer vào ESP-NOW
bool addPeer(const uint8_t *mac, uint8_t channel) {
  // Xóa peer cũ nếu đã tồn tại
  esp_now_del_peer(mac);
  
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = channel;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Thêm peer thất bại");
    return false;
  }
  return true;
}

// Hàm quét tìm thiết bị ESP-NOW
void scanESPNowDevices() {
  if (!is_master) {
    Serial.println("⚠️ Chỉ master mới có thể quét thiết bị");
    return;
  }

  // Dọn dẹp các thiết bị không hoạt động trước khi quét
  cleanupStaleDevices();

  Serial.println("🔍 Đang quét thiết bị ESP-NOW...");
  
  uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  if (addPeer(broadcastMac, 0)) {  // Sử dụng kênh hiện tại
    sendESPNowMessage(broadcastMac, 0, 0);  // Gửi discovery với retry
  }
}

// Hàm xử lý API quét ESP-NOW
void handleESPNowScan(AsyncWebServerRequest *request) {
  if (!espNowActive) {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"ESP-NOW chưa được kích hoạt\"}");
    return;
  }
  
  scanESPNowDevices();
  request->send(200, "application/json", "{\"success\":true,\"message\":\"Đang quét thiết bị ESP-NOW\"}");
}

// Hàm xử lý API lấy danh sách thiết bị ESP-NOW
void handleESPNowList(AsyncWebServerRequest *request) {
  if (!espNowActive) {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"ESP-NOW chưa được kích hoạt\"}");
    return;
  }
  
  // Dọn dẹp các thiết bị không hoạt động
  cleanupStaleDevices();
  
  DynamicJsonDocument doc(4096);
  JsonArray devices = doc.createNestedArray("devices");
  
  if (is_master) {
    for (int i = 0; i < peerCount; i++) {
      JsonObject device = devices.createNestedObject();
      char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               peerList[i].mac[0], peerList[i].mac[1], peerList[i].mac[2], 
               peerList[i].mac[3], peerList[i].mac[4], peerList[i].mac[5]);
      
      device["mac"] = String(macStr);
      device["name"] = peerList[i].name;
      device["connected"] = peerList[i].connected;
      device["rssi"] = peerList[i].rssi;
      device["ledState"] = peerList[i].ledState;
      device["lastSeen"] = peerList[i].lastSeen;
    }
  } else {
    JsonObject device = devices.createNestedObject();
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    device["mac"] = String(macStr);
    device["name"] = String(AP_SSID);
    device["connected"] = slaveConnectedToMaster;
    device["rssi"] = 0;
    device["ledState"] = slaveLedState;
  }
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

// Hàm xử lý API kết nối đến thiết bị ESP-NOW
void handleESPNowConnect(AsyncWebServerRequest *request) {
  if (!espNowActive || !is_master) {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"Chỉ master mới có thể kết nối\"}");
    return;
  }
  
  if (!request->hasParam("mac", true)) {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"Thiếu địa chỉ MAC\"}");
    return;
  }
  
  String macStr = request->getParam("mac", true)->value();
  uint8_t mac[6];
  sscanf(macStr.c_str(), "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", 
         &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
  
  int deviceIndex = -1;
  for (int i = 0; i < peerCount; i++) {
    if (memcmp(peerList[i].mac, mac, 6) == 0) {
      deviceIndex = i;
      break;
    }
  }
  
  if (deviceIndex == -1) {
    request->send(404, "application/json", "{\"success\":false,\"message\":\"Không tìm thấy thiết bị\"}");
    return;
  }
  
  if (addPeer(mac, peerList[deviceIndex].channel)) {
    peerList[deviceIndex].connected = true;
    peerList[deviceIndex].retryCount = 0;
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Kết nối thành công\"}");
    Serial.printf("✅ Đã kết nối với: %s\n", peerList[deviceIndex].name.c_str());
  } else {
    request->send(500, "application/json", "{\"success\":false,\"message\":\"Không thể kết nối\"}");
  }
}

// Hàm xử lý API điều khiển LED
void handleESPNowToggleLED(AsyncWebServerRequest *request) {
  if (!espNowActive || !is_master) {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"Chỉ master mới có thể điều khiển LED\"}");
    return;
  }
  
  if (!request->hasParam("mac", true)) {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"Thiếu địa chỉ MAC\"}");
    return;
  }
  
  String macStr = request->getParam("mac", true)->value();
  uint8_t mac[6];
  sscanf(macStr.c_str(), "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", 
         &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
  
  int deviceIndex = -1;
  for (int i = 0; i < peerCount; i++) {
    if (memcmp(peerList[i].mac, mac, 6) == 0) {
      deviceIndex = i;
      break;
    }
  }
  
  if (deviceIndex == -1 || !peerList[deviceIndex].connected) {
    request->send(404, "application/json", "{\"success\":false,\"message\":\"Thiết bị không được kết nối\"}");
    return;
  }
  
  // Gửi lệnh toggle LED
  if (sendESPNowMessage(mac, 3, 2)) {  // Type 3: Toggle command, Data 2: Toggle
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Đã gửi lệnh toggle LED\"}");
    Serial.printf("📤 Đã gửi lệnh toggle LED đến %s\n", peerList[deviceIndex].name.c_str());
  } else {
    request->send(500, "application/json", "{\"success\":false,\"message\":\"Không thể gửi lệnh\"}");
  }
}

// Hàm xử lý API để slave tự điều khiển LED
void handleSlaveToggleLED(AsyncWebServerRequest *request) {
  if (is_master) {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"Chức năng này chỉ dành cho slave\"}");
    return;
  }
  
  toggleSlaveLed();  // Toggle LED
  
  request->send(200, "application/json", 
    "{\"success\":true,\"message\":\"Đã " + String(slaveLedState ? "BẬT" : "TẮT") + " LED\"}"
  );
}
