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

// Module khuech dai am thanh I2S MAX98357A + Loa ngoai
#define PIN_I2S_SPK_BCLK 17 // MAX98357A BCLK (Bit Clock)
#define PIN_I2S_SPK_LRC  18 // MAX98357A LRC / WS (Left/Right Clock / Word Select)
#define PIN_I2S_SPK_DIN  21 // MAX98357A DIN (Data In noi voi GPIO21 ESP32)


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
const size_t VOICE_RECORD_SECONDS = 3; // Dong bo 3 giay toan he thong (Click-to-talk)
const size_t VOICE_SAMPLE_COUNT = VOICE_SAMPLE_RATE * VOICE_RECORD_SECONDS; // 48,000 samples (3 giay)
const size_t VOICE_PCM_BYTES = VOICE_SAMPLE_COUNT * sizeof(int16_t);       // 96,000 bytes
const int VOICE_MIC_GAIN = 2; // He so khuech dai chuan (tranh vo am / clip am khi noi gan)
const int VOICE_MIN_SPEECH_PEAK = 180; // Nguong VAD khi bam nut thu am (Click-to-talk, do nhay cao)
// (Da bo co che tu dong thu am ngat khi motor chay)

// --- THOI GIAN HANH TRINH KINH UOC TINH DE CHIA BUOC THOI GIAN (MS) ---
// Voi MOTOR_SPEED_PWM = 180, kinh di chuyen em ai:
// Toan hanh trinh UP mat ~5.2 giay, DOWN mat ~4.8 giay
const unsigned long FULL_TRAVEL_TIME_UP_MS = 5200;
const unsigned long FULL_TRAVEL_TIME_DOWN_MS = 4800;

// Thoi gian buoc nhay 1/2, 1/3 va 2/3:
const unsigned long STEP_TIME_UP_HALF_MS = FULL_TRAVEL_TIME_UP_MS / 2;                 // 2600 ms (~2.6s)
const unsigned long STEP_TIME_UP_THIRD_MS = FULL_TRAVEL_TIME_UP_MS / 3;                // 1733 ms (~1.73s)
const unsigned long STEP_TIME_UP_TWO_THIRDS_MS = (FULL_TRAVEL_TIME_UP_MS * 2) / 3;    // 3466 ms (~3.47s)

const unsigned long STEP_TIME_DOWN_HALF_MS = FULL_TRAVEL_TIME_DOWN_MS / 2;             // 2400 ms (~2.4s)
const unsigned long STEP_TIME_DOWN_THIRD_MS = FULL_TRAVEL_TIME_DOWN_MS / 3;           // 1600 ms (~1.6s)
const unsigned long STEP_TIME_DOWN_TWO_THIRDS_MS = (FULL_TRAVEL_TIME_DOWN_MS * 2) / 3;// 3200 ms (~3.2s)

// --- CAU HINH LOA THONG MINH MAX98357A ---
// false: Phat tieng thong bao cho ca lenh giong noi & nut bam / web khi hoan tat hanh trinh
// true:  Chi phat tieng thong bao khi duoc dieu khien bang giong noi
const bool SPEAKER_FEEDBACK_VOICE_ONLY = false;
const float SPEAKER_VOLUME = 0.75f; // Muc am luong toi uu (75% am luong van rat to ro, tiet kiem 40% dong dien nguon)
