#pragma once

#include <Arduino.h>
#include <stdint.h>

// Trang thai motor
enum MotorState {
  STOPPED,
  MOVING_UP,
  MOVING_DOWN
};

// Che do nut bam 4 nac (ADC tren GPIO1)
enum SwitchMode {
  SW_UNKNOWN,
  SW_RELEASED,
  SW_UP,
  SW_AUTO_UP,
  SW_DOWN,
  SW_AUTO_DOWN
};

// Che do chay motor
enum RunMode {
  MODE_MANUAL,
  MODE_AUTO,
  MODE_ANTI_PINCH_REVERSE,
  MODE_CAL_DOWN,
  MODE_CAL_UP
};

// Cac giai doan cua Calibration
enum CalPhase {
  CAL_INACTIVE,
  CAL_GO_DOWN,
  CAL_WAIT_UP,
  CAL_COLLECT_UP,
  CAL_FAILED
};

// Trang thai chuoi thao tac vao CAL (Gesture)
enum CalGestureState {
  GESTURE_IDLE,
  GESTURE_AUTO_DOWN_HOLDING,
  GESTURE_WAIT_RELEASE_1,
  GESTURE_WAIT_AUTO_UP,
  GESTURE_AUTO_UP_HOLDING
};

// Cau truc luu tru NVS Flash
struct CalibrationRecord {
  uint32_t magic;
  uint32_t version;
  uint32_t idleMv;
  uint32_t baselineMv;
  uint32_t marginMvAtSave;
  uint32_t p10Mv;
  uint32_t p90Mv;
  uint32_t samples;
  uint32_t checksum;
};
