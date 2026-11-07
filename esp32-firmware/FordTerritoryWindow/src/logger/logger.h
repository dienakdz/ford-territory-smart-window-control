#pragma once

#include <Arduino.h>
#include "../config/types.h"
#include "../config/config.h"

// Ring buffer luu tru log phuc vu hien thi tren Web Dashboard
extern char logRing[LOG_CAPACITY][LOG_LINE_LEN];
extern size_t logNext;
extern size_t logCount;

/**
 * @brief Ghi nhat ky su kien dong thoi ra Serial va vao bo dem Ring Buffer
 * @param message Noi dung thong diep can ghi
 */
void logEvent(const String &message);

// Cac ham chuyen doi enum trang thai sang chuoi van ban hien thi
const char *switchName(SwitchMode m);
const char *motorName();
const char *modeName();
const char *phaseName();
