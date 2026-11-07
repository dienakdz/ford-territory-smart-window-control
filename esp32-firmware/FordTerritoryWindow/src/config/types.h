#pragma once

#include <Arduino.h>
#include <stdint.h>

/**
 * @brief Trang thai chuyen dong cua motor kinh
 */
enum MotorState {
  STOPPED,      // Motor dang dung
  MOVING_UP,    // Motor dang nang kinh len
  MOVING_DOWN   // Motor dang ha kinh xuong
};

/**
 * @brief Che do cong tac cua nut bam 4 nac (Doc qua ADC tren GPIO1)
 */
enum SwitchMode {
  SW_UNKNOWN,   // Dien ap khong hop le / chap chon
  SW_RELEASED,  // Khong nhan nut (trang thai tha tay)
  SW_UP,        // Nhan giu UP thu cong (Manual UP)
  SW_AUTO_UP,   // Nhan 1 cham AUTO UP (Tu dong len het hanh trinh)
  SW_DOWN,      // Nhan giu DOWN thu cong (Manual DOWN)
  SW_AUTO_DOWN  // Nhan 1 cham AUTO DOWN (Tu dong ha het hanh trinh)
};

/**
 * @brief Che do van hanh motor
 */
enum RunMode {
  MODE_MANUAL,              // Chay thu cong (nha nut la dung)
  MODE_AUTO,                // Chay tu dong (nha nut van tiep tuc chay toi khi cham hanh trinh)
  MODE_ANTI_PINCH_REVERSE,  // Che do dac biet: dao chieu ha kinh 1s sau khi phat hien ket
  MODE_CAL_DOWN,            // Che do CAL: tu dong ha kinh ve BOTTOM
  MODE_CAL_UP               // Che do CAL: nang kinh len TOP de do dong baseline
};

/**
 * @brief Cac giai doan cua may trang thai Hieu chuan (Calibration Phase)
 */
enum CalPhase {
  CAL_INACTIVE,     // Khong o che do CAL (dang chay che do NORMAL)
  CAL_GO_DOWN,      // Giai doan 1: Tu dong ha kinh xuong day (BOTTOM)
  CAL_WAIT_UP,      // Giai doan 2: Da o day, cho nguoi dung nhan va giu UP
  CAL_COLLECT_UP,   // Giai doan 3: Dang nang kinh len TOP de ghi nhan cac mau dong ACS712
  CAL_FAILED        // Giai doan loi: Huy CAL va giu nguyen baseline cu
};

/**
 * @brief May trang thai nhan dien thao tac nut de vao CAL (CAL Gesture)
 */
enum CalGestureState {
  GESTURE_IDLE,               // Cho thao tac
  GESTURE_AUTO_DOWN_HOLDING,  // Dang giu AUTO DOWN 2s
  GESTURE_WAIT_RELEASE_1,     // Cho tha tay sau khi giu AUTO DOWN
  GESTURE_WAIT_AUTO_UP,       // Cho nhan AUTO UP trong 3s
  GESTURE_AUTO_UP_HOLDING     // Dang giu AUTO UP 2s de kich hoat CAL
};

/**
 * @brief Cau truc ban ghi luu tru vao NVS Flash ESP32
 */
struct CalibrationRecord {
  uint32_t magic;           // Magic number xac thuc (0x464F5244)
  uint32_t version;         // Phien ban cau truc du lieu (1)
  uint32_t idleMv;          // Dien ap cam bien ACS712 luc motor nghi (mV)
  uint32_t baselineMv;      // Dien ap dong nang kinh binh thuong dai dien (mV)
  uint32_t marginMvAtSave;  // Bien do chenh lech bao ve chong ket (mV)
  uint32_t p10Mv;           // Phan vi 10% cua dai mau dong
  uint32_t p90Mv;           // Phan vi 90% cua dai mau dong
  uint32_t samples;         // So luong mau hop le thu thap duoc
  uint32_t checksum;        // FNV checksum kiem tra toan ven du lieu
};
