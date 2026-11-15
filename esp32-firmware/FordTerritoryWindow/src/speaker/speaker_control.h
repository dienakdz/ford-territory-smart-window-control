#pragma once

#include <Arduino.h>
#include "../config/config.h"

/**
 * @brief Cac loai am thanh thong bao cua he thong
 */
enum SoundType {
  SOUND_NONE = 0,
  SOUND_UP_DONE,       // "Đã nâng kính"
  SOUND_DOWN_DONE,     // "Đã hạ kính"
  SOUND_ANTI_PINCH,    // "Cảnh báo kẹt kính"
  SOUND_BEEP           // Tiếng Beep nhẹ khi bấm nút nói (PTT)
};

/**
 * @brief Khoi tao module loa MAX98357A qua giao tiep I2S (I2S_NUM_1) tren ESP32-S3
 */
void speakerInit();

/**
 * @brief Yeu cau phat mot am thanh thong bao (Non-blocking, xu ly tren Task rieng)
 * @param sound Loai am thanh can phat
 */
void speakerPlay(SoundType sound);

/**
 * @brief Kiem tra loa co dang phat am thanh hay khong
 */
bool isSpeakerPlaying();

/**
 * @brief Dung phat am thanh ngay lap tuc
 */
void speakerStop();

/**
 * @brief Ham callback khi motor hoan tat hanh trinh nang/ha kinh
 * @param wasUp True neu vua thuc hien hanh trinh nang kinh, False neu ha kinh
 * @param byVoice True neu do lenh giong noi kich hoat
 */
void notifyWindowMotionCompleted(bool wasUp, bool byVoice);
