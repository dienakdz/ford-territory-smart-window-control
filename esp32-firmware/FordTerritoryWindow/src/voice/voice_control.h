#pragma once

#include <Arduino.h>
#include "../config/config.h"
#include "../config/types.h"

extern String lastVoiceResult;
extern bool isVoiceRecording;

// Khoi tao micro so INMP441 qua giao tiep I2S (GPIO14, 15, 16)
void voiceInit();

// Kiem tra ket noi den Flask Voice Server tren Laptop
bool testVoiceServerPing();

// Thu am 3 giay tu INMP441, gui HTTP POST sang Laptop, nhan va thuc thi lenh an toan
bool recordAndRecognize();
