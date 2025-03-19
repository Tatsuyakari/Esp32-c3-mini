#include "wifi_manager.h"
#include "espnow_manager.h"
#include "esp_task_wdt.h"

// Định nghĩa biến toàn cục
bool wifiConnected = false;
String connectedSSID = "";
IPAddress localIP;
String AP_SSID_STR;
const char *AP_SSID;

// Cấu hình WiFi AP
const char *MASTER_AP_SSID = "ESP32-KARI";
const char *SLAVE_AP_SSID = "ESP32-KARI-SLAVE";
const char *AP_PASS = ""; // Không đặt mật khẩu

// Hàm tắt Watchdog Timer để tránh reset ngoài ý muốn
void disableWDT() {
  esp_task_wdt_deinit();
}

// Hàm khởi tạo Access Point
void setupAP() {
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

// Hàm quét WiFi và trả về kết quả dạng JSON
void handleScan(AsyncWebServerRequest *request) {
  Serial.println("🔍 Đang quét WiFi...");
  int n = WiFi.scanNetworks();
  DynamicJsonDocument doc(4096);
  JsonArray networks = doc.createNestedArray("networks");

  if (n > 0) {
    for (int i = 0; i < n; i++) {
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
void handleConnect(AsyncWebServerRequest *request) {
  Serial.println("📨 Nhận yêu cầu kết nối WiFi");

  if (!request->hasParam("ssid", true) || !request->hasParam("pass", true)) {
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
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    doc["success"] = true;
    doc["message"] = "✅ Kết nối thành công!";
    doc["ip"] = WiFi.localIP().toString();
    wifiConnected = true;
    connectedSSID = ssid;
    localIP = WiFi.localIP();
    Serial.println("🌍 Kết nối WiFi thành công, IP: " + localIP.toString());
  } else {
    doc["success"] = false;
    doc["message"] = "❌ Không thể kết nối!";
    Serial.println("⚠️ Kết nối WiFi thất bại");
  }

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

// Hàm trả về trạng thái kết nối hiện tại
void handleStatus(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(512);
  doc["ap_ssid"] = AP_SSID_STR;
  doc["ap_ip"] = WiFi.softAPIP().toString();
  doc["wifi_connected"] = wifiConnected;
  doc["espnow_active"] = espNowActive;
  doc["is_master"] = is_master;
  doc["peer_count"] = peerCount;

  if (wifiConnected) {
    doc["wifi_ssid"] = connectedSSID;
    doc["wifi_ip"] = localIP.toString();
  }

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

// Hàm ngắt kết nối WiFi
void handleDisconnect(AsyncWebServerRequest *request) {
  if (wifiConnected) {
    WiFi.disconnect();
    wifiConnected = false;
    connectedSSID = "";
    request->send(200, "application/json", "{\"success\":true,\"message\":\"🔌 Đã ngắt kết nối WiFi\"}");
    Serial.println("🔌 Đã ngắt kết nối WiFi");
  } else {
    request->send(200, "application/json", "{\"success\":false,\"message\":\"⚠️ Không có kết nối nào\"}");
  }
}
