#pragma once

#include <Arduino.h>
#include "../config/types.h"
#include "../config/config.h"

extern bool calMode;
extern CalPhase calPhase;
extern bool calDownStarted;
extern unsigned long calStartMs;
extern unsigned long calLastSampleMs;
extern unsigned long calAbnormalSinceMs;
extern int calIdleMv;
extern int calSamples[CAL_MAX_SAMPLES];
extern int calSampleCount;
extern String lastCalResult;

extern CalGestureState calGestureState;
extern unsigned long calGestureTimer;

// Theo doi chuoi thao tac nut de kich hoat CAL khi dang o NORMAL
void trackCalGesture(unsigned long now);

// Kiem tra thao tac CAL ngay khi vua cap nguon ESP32
void checkBootCal();

// May trang thai dieu khien toan bo quy trinh Calibration
void handleCal(unsigned long now);

// Tinh toan baseline trimmed mean va luu vao NVS Flash
void computeAndSaveCal();

// Thoat khoi CAL va quay ve NORMAL
void exitCal(const String &result);
void abortCal(const String &why);
