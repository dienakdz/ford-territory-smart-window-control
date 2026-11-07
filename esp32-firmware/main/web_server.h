#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include "types.h"
#include "config.h"

extern WebServer webServer;
extern bool apReady;

// Khoi dong Wi-Fi AP va cac endpoint Web Dashboard
void startAP();

// Xu ly cac request HTTP tu trinh duyet (non-blocking)
void handleWebClient();
