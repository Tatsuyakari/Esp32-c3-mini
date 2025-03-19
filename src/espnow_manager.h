#ifndef ESPNOW_MANAGER_H
#define ESPNOW_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#define MAX_PEERS 20
#define PEER_TIMEOUT 60000  // 60 seconds timeout for inactive peers
#define MAX_RETRIES 3      // Maximum number of retries for discovery
#define LED_PIN 8          // LED builtin pin for ESP32-C3

// Cấu trúc thông tin thiết bị ESP-NOW
typedef struct {
  uint8_t mac[6];
  String name;
  bool connected;
  int rssi;
  uint8_t channel;
  bool ledState;
  unsigned long lastSeen;  // Timestamp of last communication
  uint8_t retryCount;     // Count of connection retries
} PeerInfo;

// Cấu trúc dữ liệu cho ESP-NOW
typedef struct espnow_message {
  uint8_t type;    // 0: discovery, 1: command, 2: status, 3: toggle
  uint8_t data;    // For commands: 0: LED OFF, 1: LED ON, 2: TOGGLE
  char name[16];   // Device name
} espnow_message_t;

// Khai báo biến toàn cục
extern PeerInfo peerList[MAX_PEERS];
extern int peerCount;
extern bool espNowActive;
extern bool is_master;
extern const char *AP_SSID;
extern bool slaveLedState;        // Trạng thái LED của slave
extern bool slaveConnectedToMaster; // Trạng thái kết nối với master

// Khai báo các hàm
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len);
bool initESPNow();
bool addPeer(const uint8_t *mac, uint8_t channel);
void scanESPNowDevices();
void toggleSlaveLed();  // Sửa thành toggle không cần tham số
void cleanupStaleDevices();  // Hàm mới để dọn dẹp các thiết bị không hoạt động
bool sendESPNowMessage(const uint8_t *mac, uint8_t type, uint8_t data); // Sửa thành const uint8_t*

// API handlers
void handleESPNowScan(AsyncWebServerRequest *request);
void handleESPNowList(AsyncWebServerRequest *request);
void handleESPNowConnect(AsyncWebServerRequest *request);
void handleESPNowToggleLED(AsyncWebServerRequest *request);
void handleSlaveToggleLED(AsyncWebServerRequest *request);

#endif
