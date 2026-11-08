#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "types.h"

// =====================================================
// 1. HARDWARE PIN DEFINITIONS (ESP32-S3)
// =====================================================

// Driver cau H BTS7960
#define RPWM_PIN 4
#define LPWM_PIN 5
#define REN_PIN  6
#define LEN_PIN  7

// Cong tac hanh trinh vi co (NC: thuong dong LOW, cham hanh trinh mo HIGH)
#define LIMIT_TOP    8
#define LIMIT_BOTTOM 9

// Cong tac kinh 4 che do (Dien ap chia tro doc qua ADC GPIO1)
#define SWITCH_ADC_PIN 1

// Cam bien dong ACS712 qua mach chia ap (Doc qua ADC GPIO10)
#define ACS_ADC_PIN 10


// =====================================================
// 2. CAU HINH THAM SO VAN HANH CHUNG
// =====================================================

// Thoi gian loc rung chong doi cong tac goc (ms)
const unsigned long SWITCH_DEBOUNCE_MS = 40;

// Gioi han thoi gian chay motor toi da phong ngua su co (ms)
const unsigned long MAX_RUN_MS = 8000;

// Thoi gian tu dong ha kinh khi dao chieu chong ket (ms)
const unsigned long ANTI_PINCH_REVERSE_MS = 1000;

// Thoi gian chet (Deadtime) bao ve cau H BTS7960 khi bat/dao chieu motor (ms)
const unsigned long MOTOR_DIRECTION_DEADTIME_MS = 150;

// Bo qua dong khoi dong motor (Inrush current transient) (ms)
const unsigned long ACS_IGNORE_START_MS = 350;

// Thoi gian xac nhan dien ap bat thuong truoc khi kich hoat chong ket (ms)
const unsigned long ACS_STALL_CONFIRM_MS = 150;

// Chu ky in thong so debug len serial/log dinh ky (ms)
const unsigned long DEBUG_INTERVAL_MS = 250;

// --- NGUONG THU NGHIEM CHONG KET ---
// Nguong phat hien ket = Baseline da do + Margin
const int ANTI_PINCH_MARGIN_MV = 40;
const bool ENABLE_EXPERIMENTAL_REVERSE = true; // true: tu dong dao chieu ha kinh 1s khi phat hien ket AUTO UP

// --- THAM SO HIEU CHUAN (CALIBRATION) ---
const unsigned long CAL_BOOT_HOLD_MS = 2000;        // Thoi gian giu nut khi thao tac vao CAL (ms)
const unsigned long CAL_BOOT_NEXT_MS = 3000;        // Timeout cho buoc tiep theo trong chuoi CAL (ms)
const unsigned long CAL_SAMPLE_INTERVAL_MS = 40;    // Chu ky lay mau dong khi nang kinh trong CAL (ms)
const unsigned long CAL_IGNORE_SAMPLE_START_MS = 550; // Bo qua doan dong khoi dong motor ban dau khi hoc baseline (ms)
const unsigned long CAL_HARD_TIMEOUT_MS = 8000;     // Gioi han thoi gian toi da cho 1 chieu nang/ha trong CAL (ms)

const int CAL_MIN_SAMPLES = 20;                     // So mau toi thieu de cong nhan mot hanh trinh CAL hop le (~0.8s)
const int CAL_MAX_SAMPLES = 210;                    // So luong mau toi da luu trong RAM
const int CAL_MIN_SIGNAL_ABOVE_IDLE_MV = 40;        // Dk kiem tra: Baseline phai lon hon dong nghi it nhat 40mV
const int CAL_MAX_P10_P90_SPREAD_MV = 250;          // Dk kiem tra: Do phan tan p10-p90 khong vuot qua 250mV (chong nhieu)
const int CAL_TOP_STALL_ABOVE_IDLE_MV = 550;        // Nhan dien cham dinh co hoc (Stall tai TOP): dong tang >= idle + 550mV
const unsigned long CAL_TOP_STALL_CONFIRM_MS = 150; // Thoi gian duy tri dong cao de xac nhan kịch nóc (ms)

// Magic & Version xac thuc NVS
const uint32_t CAL_MAGIC = 0x464F5244u;
const uint32_t CAL_VERSION = 1u;

// --- WI-FI ACCESS POINT & DASHBOARD ---
const char * const AP_SSID = "FORD-WINDOW-DEBUG";
const char * const AP_PASSWORD = "12345678";
const size_t LOG_CAPACITY = 90;
const size_t LOG_LINE_LEN = 148;
