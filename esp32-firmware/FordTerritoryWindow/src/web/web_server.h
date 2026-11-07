#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include "../config/types.h"
#include "../config/config.h"

extern WebServer webServer;
extern bool apReady;

// Khoi dong SoftAP va thiet lap cac REST endpoint cho Web Dashboard
void startAP();

// Xu ly cac ket noi HTTP tu thiet bi client (non-blocking)
void handleWebClient();
