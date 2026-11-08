#pragma once

#include <Arduino.h>
#include "../config/config.h"
#include "../config/types.h"

extern String lastVoiceResult;
extern bool isVoiceRecording;
extern volatile VoiceCommandReq voicePendingReq;
extern volatile unsigned long voiceActiveUntilMs;

// Khoi tao micro so INMP441 qua giao tiep I2S va khoi dong Dual-Core Task tren Core 0
void voiceInit();

// Kiem tra ket noi den Flask Voice Server tren Laptop
bool testVoiceServerPing();

// Kich hoat lang nghe giong noi (mo cua so 10 giay, Non-blocking)
void triggerVoiceListen();

// Kiem tra va thuc thi lenh giong noi tren Core 1 (goi trong loop)
void checkVoicePendingReq();

// Ham tuong thich: Kich hoat lang nghe
bool recordAndRecognize();
