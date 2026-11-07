#pragma once

#include <Arduino.h>
#include <stdint.h>

// =====================================================
// 1. HARDWARE PIN DEFINITIONS (ESP32-S3)
// =====================================================

// Driver BTS7960
#define RPWM_PIN 4
#define LPWM_PIN 5
#define REN_PIN  6
#define LEN_PIN  7

// Cong tac hanh trinh (NC: binh thuong LOW, cham/ho mach HIGH)
#define LIMIT_TOP    8
#define LIMIT_BOTTOM 9

// Cong tac kinh 4 che do (ADC GPIO1)
#define SWITCH_ADC_PIN 1

// ACS712 OUT sau mach chia ap (ADC GPIO10)
#define ACS_ADC_PIN 10


// =====================================================
// 2. CAU HINH & THAM SO HOAT DONG
// =====================================================

// Chong doi cong tac goc (ms)
const unsigned long SWITCH_DEBOUNCE_MS = 40;

// Gioi han chay motor toi da (ms)
const unsigned long MAX_RUN_MS = 8000;

// Thoi gian ha kinh khi dao chieu chong ket (ms)
const unsigned long ANTI_PINCH_REVERSE_MS = 1000;

// Thoi gian chet bao ve H-bridge khi bat/dao chieu motor (ms)
const unsigned long MOTOR_DIRECTION_DEADTIME_MS = 150;

// Bo qua dong khoi dong motor (ms)
const unsigned long ACS_IGNORE_START_MS = 350;

// Thoi gian xac nhan dien ap bat thuong de kich hoat chong ket (ms)
const unsigned long ACS_STALL_CONFIRM_MS = 150;

// Chu ky in thong so debug len serial/log (ms)
const unsigned long DEBUG_INTERVAL_MS = 250;

// --- NGUONG THU NGHIEM CHONG KET ---
const int ANTI_PINCH_MARGIN_MV = 50;            // Nguong = baseline + margin
const bool ENABLE_EXPERIMENTAL_REVERSE = false; // false: STOP + LOG; true: dao chieu 1s (chi bat sau khi kiem chung an toan)

// --- THAM SO HIEU CHUAN (CALIBRATION) ---
const unsigned long CAL_BOOT_HOLD_MS = 2000;
const unsigned long CAL_BOOT_NEXT_MS = 3000;
const unsigned long CAL_SAMPLE_INTERVAL_MS = 40;
const unsigned long CAL_IGNORE_SAMPLE_START_MS = 550;
const unsigned long CAL_HARD_TIMEOUT_MS = 8000;
const int CAL_MIN_SAMPLES = 20;
const int CAL_MAX_SAMPLES = 210;
const int CAL_MIN_SIGNAL_ABOVE_IDLE_MV = 40;
const int CAL_MAX_P10_P90_SPREAD_MV = 250;
const int CAL_TOP_STALL_ABOVE_IDLE_MV = 550;
const unsigned long CAL_TOP_STALL_CONFIRM_MS = 150;

// Magic & Version NVS Flash
const uint32_t CAL_MAGIC = 0x464F5244u;
const uint32_t CAL_VERSION = 1u;

// --- WI-FI AP & DASHBOARD ---
const char * const AP_SSID = "FORD-WINDOW-DEBUG";
const char * const AP_PASSWORD = "FordWindow!26";
const size_t LOG_CAPACITY = 90;
const size_t LOG_LINE_LEN = 148;
