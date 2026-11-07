#pragma once

#include <Arduino.h>
#include "../config/types.h"
#include "../config/config.h"

extern int latestAcsMv;
extern bool acsHighDetected;
extern unsigned long acsHighSince;
extern unsigned long lastDebugMs;

// Khoi tao chan ADC ACS712
void antiPinchInit();

// Doc gia tri dien ap trung binh 10 mau tu ACS712
int readAcsVoltageMv();

// Giam sat chong ket trong AUTO UP
void monitorAntiPinch();

// Giam sat dao chieu ha kinh sau khi ket
void monitorAntiPinchReverse();
