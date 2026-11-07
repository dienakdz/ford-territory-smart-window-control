#pragma once

#include <Arduino.h>
#include "types.h"
#include "config.h"

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

// Theo doi chuoi thao tac de kich hoat CAL khi o NORMAL
void trackCalGesture(unsigned long now);

// Kiem tra thao tac CAL ngay khi vua cap nguon ESP32
void checkBootCal();

// State machine dieu khien quy trinh Calibration
void handleCal(unsigned long now);

// Tinh toan baseline va luu vao NVS
void computeAndSaveCal();

// Thoat hoac huy quy trinh Calibration
void exitCal(const String &result);
void abortCal(const String &why);
