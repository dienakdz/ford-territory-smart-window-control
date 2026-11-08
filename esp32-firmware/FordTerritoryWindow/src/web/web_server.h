#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include "../config/types.h"
#include "../config/config.h"

extern WebServer webServer;
extern bool apReady;
extern bool wifiStaConnected;

// Khoi dong Wi-Fi che do kep (WIFI_AP_STA): ket noi iPhone (IP tinh 172.20.10.50) + AP du phong (192.168.4.1)
void startWiFi();
inline void startAP() { startWiFi(); }

// Kiem tra trang thai ket noi Wi-Fi voi iPhone Hotspot dinh ky
void checkWiFiStatus();

// Xu ly cac ket noi HTTP tu thiet bi client (non-blocking)
void handleWebClient();
