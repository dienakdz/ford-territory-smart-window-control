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

// Micro so I2S INMP441 thu am giong noi
#define PIN_I2S_BCLK 14  // INMP441 SCK
#define PIN_I2S_WS   15  // INMP441 WS
#define PIN_I2S_DIN  16  // INMP441 SD (L/R pin noi xuong GND de thu kenh trai Mono)

// Nut bam vat ly kich hoat giong noi (Push-To-Talk PTT Voice Button)
// Dau 1 chan vao GPIO2, 1 chan vao GND (ESP32 dung INPUT_PULLUP noi, khong can them tro ngoai)
#define VOICE_BUTTON_PIN 2


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
const int ANTI_PINCH_MARGIN_MV = 20;
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

// --- WI-FI STATION CHO IPHONE HOTSPOT (GIAI PHAP 1 - IP TINH) ---
// Thay doi SSID va PASSWORD ben duoi neu ban doi ten Hotspot iPhone
const char * const STA_SSID = "Diệnn";
const char * const STA_PASSWORD = "39393939";

// IP tinh co dinh cho ESP32 trong mang iPhone Hotspot (Mo Dashboard: http://172.20.10.3)
// LUU Y: iPhone Hotspot su dung Subnet Mask /28 (255.255.255.240), dai IP hop le chi tu 172.20.10.1 den 172.20.10.14
const IPAddress STA_STATIC_IP(172, 20, 10, 3);
const IPAddress STA_GATEWAY(172, 20, 10, 1);
const IPAddress STA_SUBNET(255, 255, 255, 240);
const IPAddress STA_DNS(172, 20, 10, 1);

// Dia chi Flask Voice Server chay tren Laptop (cung mang iPhone Hotspot)
const char * const FLASK_SERVER_BASE = "http://172.20.10.2:5000";

// --- DIEU KHIEN TOC DO MOTOR (PWM BTS7960) ---
// 255: 100% dien ap (chay cuc nhanh ~2.64s, qua nhanh doi voi kinh oto)
// 180: ~70% dien ap (kinh chay em ai ~5.0 - 5.5s, rat truc quan de quan sat va ra lenh giong noi)
const uint8_t MOTOR_SPEED_PWM = 180;

// --- THAM SO THU AM GIONG NOI INMP441 ---
const uint32_t VOICE_SAMPLE_RATE = 16000;
const size_t VOICE_RECORD_SECONDS = 2; // Dong bo 2 giay toan he thong
const size_t VOICE_SAMPLE_COUNT = VOICE_SAMPLE_RATE * VOICE_RECORD_SECONDS; // 32,000 samples (2 giay)
const size_t VOICE_PCM_BYTES = VOICE_SAMPLE_COUNT * sizeof(int16_t);       // 64,000 bytes
const int VOICE_MIC_GAIN = 2; // He so khuech dai chuan (tranh vo am / clip am khi noi gan)
const int VOICE_MIN_SPEECH_PEAK = 180; // Nguong VAD (sau bo loc DC blocker): peak < 180 la im lang -> bo qua khong gui
