#include "anti_pinch.h"
#include "../motor/motor_control.h"
#include "../storage/nvs_storage.h"
#include "../logger/logger.h"
#include "../speaker/speaker_control.h"

int latestAcsMv = 0;
bool acsHighDetected = false;
unsigned long acsHighSince = 0;
unsigned long lastDebugMs = 0;

void antiPinchInit() {
  pinMode(ACS_ADC_PIN, INPUT);
  analogSetPinAttenuation(ACS_ADC_PIN, ADC_11db);
}

int readAcsVoltageMv() {
  // Trung binh 10 mau ADC
  long totalMv = 0;
  for (int i = 0; i < 10; i++) {
    totalMv += analogReadMilliVolts(ACS_ADC_PIN);
    delayMicroseconds(200);
  }
  return int(totalMv / 10);
}

void monitorAntiPinch() {
  // Kiem tra khi dang nang tu dong (AUTO UP hoac TIMED STEP UP) va da co baseline hop le
  if (motorState != MOVING_UP || (currentRunMode != MODE_AUTO && currentRunMode != MODE_TIMED_STEP) || !baselineValid) {
    acsHighDetected = false;
    acsHighSince = 0;
    return;
  }

  unsigned long now = millis();

  // Bo qua dong khoi dong motor ban dau
  if (now - motorStartMs < ACS_IGNORE_START_MS) {
    acsHighDetected = false;
    acsHighSince = 0;
    return;
  }

  int acsMv = readAcsVoltageMv();
  latestAcsMv = acsMv;
  int thresholdMv = int(savedCal.baselineMv) + ANTI_PINCH_MARGIN_MV;

  // In thong so dinh ky de quan sat
  if (now - lastDebugMs >= DEBUG_INTERVAL_MS) {
    lastDebugMs = now;
    logEvent(String("[ACS] UP=") + String(acsMv) + "mV | BASE=" +
             String(savedCal.baselineMv) + "mV | THR=" + String(thresholdMv) + "mV");
  }

  // So sanh voi nguong thu nghiem (baseline + margin)
  if (acsMv >= thresholdMv) {
    if (!acsHighDetected) {
      acsHighDetected = true;
      acsHighSince = now;
    }

    if (now - acsHighSince >= ACS_STALL_CONFIRM_MS) {
      logEvent(String("[CHONG KET] Phat hien ket khi AUTO UP! ACS>=BASE+") +
               String(ANTI_PINCH_MARGIN_MV) + "mV trong " +
               String(ACS_STALL_CONFIRM_MS) + "ms. DUNG MOTOR. (UNVALIDATED)");

      motorStop();
      currentRunMode = MODE_MANUAL;

      // Toi uu nguon dien: KHONG phat loa luc nay de tranh sut ap khi motor khoi dong dao chieu!
      // Cho motor ha kinh 1s xong -> Dung han -> Loa moi phat "Canh bao ket kinh, da ha kinh"

      if (ENABLE_EXPERIMENTAL_REVERSE && !bottomReached()) {
        motorRun(false, MODE_ANTI_PINCH_REVERSE);
        logEvent("[CHONG KET] Bat dau ha kinh toi da 1s.");
      } else {
        // Neu khong dao chieu (hoac da cham day), phat canh bao ngay
        speakerPlay(SOUND_ANTI_PINCH);
      }

      acsHighDetected = false;
      acsHighSince = 0;
    }
  } else {
    // Dien ap tro lai binh thuong
    acsHighDetected = false;
    acsHighSince = 0;
  }
}

void monitorAntiPinchReverse() {
  if (currentRunMode != MODE_ANTI_PINCH_REVERSE || motorState != MOVING_DOWN) {
    return;
  }

  if (millis() - motorStartMs >= ANTI_PINCH_REVERSE_MS) {
    motorStop();
    currentRunMode = MODE_MANUAL;
    logEvent("[CHONG KET] Da ha kinh 1s sau khi ket - DUNG MOTOR.");

    // Motor da dung han, nguon dien on dinh 100% -> Phat loa thong bao:
    speakerPlay(SOUND_ANTI_PINCH);
  }
}
