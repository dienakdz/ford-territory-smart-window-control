#include "switch_control.h"
#include "../motor/motor_control.h"
#include "../storage/nvs_storage.h"
#include "../logger/logger.h"

int switchVoltageMv = 0;
SwitchMode stableSwitchMode = SW_UNKNOWN;
SwitchMode candidateSwitchMode = SW_UNKNOWN;
unsigned long candidateSince = 0;
bool requireReleaseForNormal = false;

void switchInit() {
  pinMode(SWITCH_ADC_PIN, INPUT);
  analogSetPinAttenuation(SWITCH_ADC_PIN, ADC_11db);
}

SwitchMode readSwitchMode() {
  // Lay trung binh 4 mau ngan (~200us) de triet tieu nhieu ADC esp32
  long totalMv = 0;
  for (int i = 0; i < 4; i++) {
    totalMv += analogReadMilliVolts(SWITCH_ADC_PIN);
    delayMicroseconds(50);
  }
  switchVoltageMv = int(totalMv / 4);

  // Tha cong tac (nominal ~3.13V)
  if (switchVoltageMv >= 2850) {
    return SW_RELEASED;
  }

  // UP thu cong (nominal ~2.52V)
  if (switchVoltageMv >= 2250 && switchVoltageMv <= 2800) {
    return SW_UP;
  }

  // AUTO UP (nominal ~1.7V)
  if (switchVoltageMv >= 1400 && switchVoltageMv <= 2050) {
    return SW_AUTO_UP;
  }

  // DOWN thu cong (nominal ~0.85V)
  if (switchVoltageMv >= 550 && switchVoltageMv <= 1150) {
    return SW_DOWN;
  }

  // AUTO DOWN (nominal ~0V)
  if (switchVoltageMv <= 280) {
    return SW_AUTO_DOWN;
  }

  return SW_UNKNOWN;
}

void handleSwitchModeChange(SwitchMode mode) {
  if (currentRunMode == MODE_ANTI_PINCH_REVERSE) {
    // Neu dang dao chieu ma co thao tac cong tac moi, huy dao chieu de an toan
    if (mode != SW_RELEASED) {
      motorStop();
      currentRunMode = MODE_MANUAL;
      logEvent("[SAFE] Dao chieu bi huy boi thao tac cong tac.");
    }
    return;
  }

  switch (mode) {
    case SW_UP:
      // Nhan va giu de nang kinh
      motorRun(true, MODE_MANUAL);
      break;

    case SW_AUTO_UP:
      // Nhan mot lan de nang tu dong - YEU CAU BASELINE HOP LE
      if (!baselineValid) {
        logEvent("[SAFE] AUTO UP bi khoa: Chua co baseline hop le. Hay thuc hien CAL truoc.");
        motorStop();
        currentRunMode = MODE_MANUAL;
      } else {
        motorRun(true, MODE_AUTO);
      }
      break;

    case SW_DOWN:
      // Nhan va giu de ha kinh
      motorRun(false, MODE_MANUAL);
      break;

    case SW_AUTO_DOWN:
      // Nhan mot lan de ha tu dong
      motorRun(false, MODE_AUTO);
      break;

    case SW_RELEASED:
      // Thu cong: tha cong tac thi dung.
      // Tu dong: tiep tuc chay sau khi tha.
      if (currentRunMode == MODE_MANUAL) {
        motorStop();
      }
      break;

    case SW_UNKNOWN:
    default:
      logEvent("[SWITCH] Canh bao: Dien ap cong tac khong hop le. DUNG MOTOR.");
      motorStop();
      currentRunMode = MODE_MANUAL;
      break;
  }
}
