#pragma once

#include <Arduino.h>
#include "types.h"
#include "config.h"

// Ring buffer luu tru log phuc vu Web Dashboard
extern char logRing[LOG_CAPACITY][LOG_LINE_LEN];
extern size_t logNext;
extern size_t logCount;

// Ham ghi log dong thoi ra Serial va Ring Buffer
void logEvent(const String &message);

// Cac ham tien ich chuyen doi trang thai thanh chuoi van ban
const char *switchName(SwitchMode m);
const char *motorName();
const char *modeName();
const char *phaseName();
