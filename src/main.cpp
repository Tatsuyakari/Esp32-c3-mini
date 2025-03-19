#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include "esp_task_wdt.h" // ESP32-C3 sử dụng thư viện này

#define LED_BUILTIN 8
#define MAX_PEERS 20

// Khởi tạo web server chạy trên cổng 80
AsyncWebServer server(80);

// Cấu hình WiFi AP
const char *MASTER_AP_SSID = "ESP32-KARI";
const char *SLAVE_AP_SSID = "ESP32-KARI-SLAVE";
const char *AP_PASS = ""; // Không đặt mật khẩu
String AP_SSID_STR; // Biến lưu tên AP thực tế được sử dụng
const char *AP_SSID; // Con trỏ đến chuỗi char

// Biến trạng thái WiFi
bool wifiConnected = false;
String connectedSSID = "";
IPAddress localIP;

// Biến trạng thái ESP-NOW
#define IS_MASTER true  // Thay đổi thành false để nạp cho thiết bị slave
bool is_master = IS_MASTER; // Được quy định bằng define
bool espNowActive = false;
typedef struct {
  uint8_t mac[6];
  String name;
  bool connected;
  int rssi;
  uint8_t channel;
  bool ledState;
} PeerInfo;
PeerInfo peerList[MAX_PEERS];
int peerCount = 0;

// Cấu trúc dữ liệu cho ESP-NOW
typedef struct espnow_message {
  uint8_t type; // 0: discovery, 1: command, 2: status
  uint8_t data; // For commands: 0: LED OFF, 1: LED ON, others for future use
  char name[16]; // Device name
} espnow_message_t;

// Callback khi gửi dữ liệu ESP-NOW
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("📤 Gửi dữ liệu đến: ");
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
  Serial.print(" - Trạng thái: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Thành công" : "Thất bại");
}

// Callback khi nhận dữ liệu ESP-NOW
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  Serial.print("📥 Nhận dữ liệu từ: ");
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println(macStr);

  espnow_message_t *message = (espnow_message_t *)data;

  if (is_master) {
    // Master nhận dữ liệu
    if (message->type == 0) {  // Discovery response
      // Kiểm tra xem thiết bị đã có trong danh sách chưa
      bool found = false;
      for (int i = 0; i < peerCount; i++) {
        if (memcmp(peerList[i].mac, mac, 6) == 0) {
          // Cập nhật thông tin nếu đã có
          peerList[i].name = String(message->name);
          peerList[i].rssi = WiFi.RSSI();
          found = true;
          break;
        }
      }

      // Thêm thiết bị mới vào danh sách nếu chưa có
      if (!found && peerCount < MAX_PEERS) {
        memcpy(peerList[peerCount].mac, mac, 6);
        peerList[peerCount].name = String(message->name);
        peerList[peerCount].connected = false;
        peerList[peerCount].rssi = WiFi.RSSI();
        peerList[peerCount].channel = WiFi.channel();
        peerList[peerCount].ledState = false;
        peerCount++;
        Serial.println("🆕 Thêm thiết bị mới vào danh sách: " + String(message->name));
      }
    } else if (message->type == 2) {  // Status update
      // Cập nhật trạng thái LED của thiết bị
      for (int i = 0; i < peerCount; i++) {
        if (memcmp(peerList[i].mac, mac, 6) == 0) {
          peerList[i].ledState = message->data;
          Serial.println("🔄 Cập nhật trạng thái LED của " + peerList[i].name + ": " + (message->data ? "BẬT" : "TẮT"));
          break;
        }
      }
    }
  } else {
    // Slave nhận dữ liệu
    if (message->type == 0) {  // Discovery request
      // Gửi phản hồi tới master
      espnow_message_t response;
      response.type = 0;  // Discovery response
      response.data = 0;
      strncpy(response.name, AP_SSID, sizeof(response.name));
      esp_err_t result = esp_now_send(mac, (uint8_t *)&response, sizeof(response));
      if (result == ESP_OK) {
        Serial.println("✅ Đã gửi phản hồi khám phá đến master");
      }
    } else if (message->type == 1) {  // Command
      // Xử lý lệnh điều khiển LED
      if (message->data == 0 || message->data == 1) {
        digitalWrite(LED_BUILTIN, message->data);
        Serial.println("💡 LED đã được " + String(message->data ? "BẬT" : "TẮT"));
        
        // Gửi trạng thái LED về master
        espnow_message_t statusMsg;
        statusMsg.type = 2;  // Status
        statusMsg.data = message->data;  // LED state
        strncpy(statusMsg.name, AP_SSID, sizeof(statusMsg.name));
        esp_now_send(mac, (uint8_t *)&statusMsg, sizeof(statusMsg));
      }
    }
  }
}

// Hàm tắt Watchdog Timer để tránh reset ngoài ý muốn
void disableWDT()
{
  esp_task_wdt_deinit();
}

// Hàm khởi tạo Access Point
void setupAP()
{
  IPAddress apIP(192, 168, 4, 1);
  IPAddress netMsk(255, 255, 255, 0);

  // Chọn tên WiFi dựa trên vai trò master/slave
  AP_SSID_STR = is_master ? MASTER_AP_SSID : SLAVE_AP_SSID;
  AP_SSID = AP_SSID_STR.c_str(); // Cập nhật con trỏ

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.println("⚡ Access Point đã được khởi tạo:");
  Serial.println("📶 SSID: " + AP_SSID_STR);
  Serial.print("🌐 Địa chỉ IP: ");
  Serial.println(WiFi.softAPIP());
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
bool addPeer(uint8_t *mac, uint8_t channel) {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = channel;
  peerInfo.encrypt = false;  // Không mã hóa kết nối

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Thêm peer thất bại");
    return false;
  }
  return true;
}

// Hàm quét tìm thiết bị ESP-NOW
void scanESPNowDevices() {
  if (!is_master) {
    Serial.println("⚠️ Chỉ có master mới có thể quét thiết bị");
    return;
  }

  Serial.println("🔍 Đang quét thiết bị ESP-NOW...");
  
  // Gửi gói tin broadcast để tìm thiết bị
  uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastMac, 6);
  peerInfo.channel = 0;  // Kênh của WiFi hiện tại
  peerInfo.encrypt = false;
  
  // Xóa peer nếu đã tồn tại
  esp_now_del_peer(broadcastMac);
  
  // Thêm peer broadcast
  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK) {
    Serial.println("❌ Không thể thêm peer broadcast");
    return;
  }
  
  // Gửi tin nhắn discovery
  espnow_message_t discMsg;
  discMsg.type = 0;  // Discovery
  discMsg.data = 0;
  strncpy(discMsg.name, AP_SSID, sizeof(discMsg.name));
  
  result = esp_now_send(broadcastMac, (uint8_t *)&discMsg, sizeof(discMsg));
  if (result != ESP_OK) {
    Serial.println("❌ Không thể gửi tin nhắn discovery");
  } else {
    Serial.println("📨 Đã gửi gói tin discovery");
  }
}

// Hàm xử lý API quét ESP-NOW
void handleESPNowScan(AsyncWebServerRequest *request) {
  if (!espNowActive) {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"ESP-NOW chưa được kích hoạt\"}");
    return;
  }
  
  // Đặt lại danh sách thiết bị
  peerCount = 0;
  
  // Quét thiết bị ESP-NOW
  scanESPNowDevices();
  
  request->send(200, "application/json", "{\"success\":true,\"message\":\"Đang quét thiết bị ESP-NOW\"}");
}

// Hàm xử lý API lấy danh sách thiết bị ESP-NOW
void handleESPNowList(AsyncWebServerRequest *request) {
  if (!espNowActive) {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"ESP-NOW chưa được kích hoạt\"}");
    return;
  }
  
  DynamicJsonDocument doc(4096);
  JsonArray devices = doc.createNestedArray("devices");
  
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
  
  // Chuyển đổi chuỗi MAC thành mảng byte
  sscanf(macStr.c_str(), "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", 
         &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
  
  // Tìm thiết bị trong danh sách
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
  
  // Thêm peer vào ESP-NOW
  bool result = addPeer(mac, peerList[deviceIndex].channel);
  
  if (result) {
    peerList[deviceIndex].connected = true;
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Kết nối thành công\"}");
    Serial.println("✅ Đã kết nối với thiết bị: " + peerList[deviceIndex].name);
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
  
  if (!request->hasParam("state", true)) {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"Thiếu trạng thái LED\"}");
    return;
  }
  
  String macStr = request->getParam("mac", true)->value();
  String stateStr = request->getParam("state", true)->value();
  
  uint8_t mac[6];
  sscanf(macStr.c_str(), "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", 
         &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
  
  uint8_t state = stateStr == "1" ? 1 : 0;
  
  // Tìm thiết bị trong danh sách
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
  
  // Gửi lệnh điều khiển LED
  espnow_message_t cmdMsg;
  cmdMsg.type = 1;  // Command
  cmdMsg.data = state;  // 0: TẮT, 1: BẬT
  strncpy(cmdMsg.name, AP_SSID, sizeof(cmdMsg.name));
  
  esp_err_t result = esp_now_send(mac, (uint8_t *)&cmdMsg, sizeof(cmdMsg));
  
  if (result == ESP_OK) {
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Đã gửi lệnh điều khiển LED\"}");
    Serial.println("📤 Đã gửi lệnh " + String(state ? "BẬT" : "TẮT") + " LED đến " + peerList[deviceIndex].name);
  } else {
    request->send(500, "application/json", "{\"success\":false,\"message\":\"Không thể gửi lệnh\"}");
  }
}

// Không còn hàm đổi chế độ master/slave, vì chế độ được quy định bằng flag khi biên dịch

// Hàm quét WiFi và trả về kết quả dạng JSON
void handleScan(AsyncWebServerRequest *request)
{
  Serial.println("🔍 Đang quét WiFi...");
  int n = WiFi.scanNetworks();
  DynamicJsonDocument doc(4096);
  JsonArray networks = doc.createNestedArray("networks");

  if (n > 0)
  {
    for (int i = 0; i < n; i++)
    {
      JsonObject network = networks.createNestedObject();
      network["ssid"] = WiFi.SSID(i);
      network["rssi"] = WiFi.RSSI(i);
      network["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      yield(); // Tránh bị reset vì quá tải
    }
  }

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);

  WiFi.scanDelete();
  Serial.println("📡 Đã gửi kết quả quét WiFi");
}

// Hàm xử lý kết nối WiFi
void handleConnect(AsyncWebServerRequest *request)
{
  Serial.println("📨 Nhận yêu cầu kết nối WiFi");

  if (!request->hasParam("ssid", true) || !request->hasParam("pass", true))
  {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"Thiếu SSID hoặc Password\"}");
    return;
  }

  String ssid = request->getParam("ssid", true)->value();
  String pass = request->getParam("pass", true)->value();

  DynamicJsonDocument doc(256);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  Serial.print("⏳ Đang kết nối ");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    doc["success"] = true;
    doc["message"] = "✅ Kết nối thành công!";
    doc["ip"] = WiFi.localIP().toString();
    wifiConnected = true;
    connectedSSID = ssid;
    localIP = WiFi.localIP();
    Serial.println("🌍 Kết nối WiFi thành công, IP: " + localIP.toString());
  }
  else
  {
    doc["success"] = false;
    doc["message"] = "❌ Không thể kết nối!";
    Serial.println("⚠️ Kết nối WiFi thất bại");
  }

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

// Hàm trả về trạng thái kết nối hiện tại
void handleStatus(AsyncWebServerRequest *request)
{
  DynamicJsonDocument doc(512);
  doc["ap_ssid"] = AP_SSID_STR;
  doc["ap_ip"] = WiFi.softAPIP().toString();
  doc["wifi_connected"] = wifiConnected;
  doc["espnow_active"] = espNowActive;
  doc["is_master"] = is_master;
  doc["peer_count"] = peerCount;

  if (wifiConnected)
  {
    doc["wifi_ssid"] = connectedSSID;
    doc["wifi_ip"] = localIP.toString();
  }

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

// Hàm ngắt kết nối WiFi
void handleDisconnect(AsyncWebServerRequest *request)
{
  if (wifiConnected)
  {
    WiFi.disconnect();
    wifiConnected = false;
    connectedSSID = "";
    request->send(200, "application/json", "{\"success\":true,\"message\":\"🔌 Đã ngắt kết nối WiFi\"}");
    Serial.println("🔌 Đã ngắt kết nối WiFi");
  }
  else
  {
    request->send(200, "application/json", "{\"success\":false,\"message\":\"⚠️ Không có kết nối nào\"}");
  }
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
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
      Serial.println("❌ Format SPIFFS thất bại! Dừng chương trình.");
      return;
    }
  }
  Serial.println("✅ SPIFFS đã được mount thành công");

  // Kiểm tra danh sách tệp trong SPIFFS
  Serial.println("📂 Danh sách tệp trong SPIFFS:");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file)
  {
    Serial.println("- " + String(file.name()));
    file = root.openNextFile();
  }

  // Khởi tạo Access Point
  setupAP();

  // Khởi tạo ESP-NOW
  espNowActive = initESPNow();
  
  // Nếu là slave, bật LED để sẵn sàng kiểm tra
  if (!is_master) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("💡 LED đã sẵn sàng cho điều khiển từ xa");
  }

  // Đăng ký các đường dẫn xử lý tĩnh
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.serveStatic("/css", SPIFFS, "/css/");
  server.serveStatic("/js", SPIFFS, "/js/");

  // Đăng ký API endpoints
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/connect", HTTP_POST, handleConnect);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/disconnect", HTTP_GET, handleDisconnect);
  
  // API ESP-NOW
  server.on("/api/espnow/scan", HTTP_GET, handleESPNowScan);
  server.on("/api/espnow/list", HTTP_GET, handleESPNowList);
  server.on("/api/espnow/connect", HTTP_POST, handleESPNowConnect);
  server.on("/api/espnow/toggle", HTTP_POST, handleESPNowToggleLED);

  // Xử lý 404 Not Found
  server.onNotFound([](AsyncWebServerRequest *request)
                    { request->send(SPIFFS, "/index.html", "text/html"); });

  server.begin();
  Serial.println("🌐 HTTP server đã chạy!");
}

void loop()
{
  // Nếu là slave, gửi gói tin broadcast để thông báo sự hiện diện
  // Slave liên tục broadcast để Master phát hiện
  if (!is_master && espNowActive) {
    static unsigned long lastBroadcast = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastBroadcast > 3000) {  // Broadcast mỗi 3 giây
      lastBroadcast = currentTime;
      
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
        discMsg.data = 0;
        strncpy(discMsg.name, AP_SSID, sizeof(discMsg.name));
        
        esp_now_send(broadcastMac, (uint8_t *)&discMsg, sizeof(discMsg));
        Serial.println("📢 Gửi tín hiệu broadcast");
        
        // Nhấp nháy LED để cho biết đang tìm master
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      }
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

  digitalWrite(LED_BUILTIN, LOW);
  delay(50);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(950);
}