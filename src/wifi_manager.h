#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Khai báo biến toàn cục
extern bool wifiConnected;
extern String connectedSSID;
extern IPAddress localIP;
extern const char *AP_SSID;
extern String AP_SSID_STR;
extern bool is_master;
extern bool espNowActive;
extern int peerCount;

// Khai báo các hàm
void setupAP();
void disableWDT();

// API handlers
void handleScan(AsyncWebServerRequest *request);
void handleConnect(AsyncWebServerRequest *request);
void handleStatus(AsyncWebServerRequest *request);
void handleDisconnect(AsyncWebServerRequest *request);

#endif
