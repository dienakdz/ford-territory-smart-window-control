#include "calibration.h"
#include "motor_control.h"
#include "switch_control.h"
#include "anti_pinch.h"
#include "nvs_storage.h"
#include "logger.h"

bool calMode = false;
CalPhase calPhase = CAL_INACTIVE;
bool calDownStarted = false;
unsigned long calStartMs = 0;
unsigned long calLastSampleMs = 0;
unsigned long calAbnormalSinceMs = 0;
int calIdleMv = 0;
int calSamples[CAL_MAX_SAMPLES] = {};
int calSampleCount = 0;
String lastCalResult = "CHUA HIEU CHUAN";

CalGestureState calGestureState = GESTURE_IDLE;
unsigned long calGestureTimer = 0;

void trackCalGesture(unsigned long now) {
  if (calMode) {
    calGestureState = GESTURE_IDLE;
    return;
  }

  switch (calGestureState) {
    case GESTURE_IDLE:
      if (stableSwitchMode == SW_AUTO_DOWN) {
        calGestureState = GESTURE_AUTO_DOWN_HOLDING;
        calGestureTimer = now;
      }
      break;

    case GESTURE_AUTO_DOWN_HOLDING:
      if (stableSwitchMode != SW_AUTO_DOWN) {
        calGestureState = GESTURE_IDLE;
      } else if (now - calGestureTimer >= CAL_BOOT_HOLD_MS) {
        calGestureState = GESTURE_WAIT_RELEASE_1;
        calGestureTimer = now;
        logEvent("[CAL] Buoc 1/3 OK: Da giu AUTO DOWN 2s. Hay THA nut trong 3s.");
      }
      break;

    case GESTURE_WAIT_RELEASE_1:
      if (stableSwitchMode == SW_RELEASED) {
        calGestureState = GESTURE_WAIT_AUTO_UP;
        calGestureTimer = now;
        logEvent("[CAL] Buoc 2/3 OK: Da tha nut. Hay giu AUTO UP 2s (trong 3s toi).");
      } else if (now - calGestureTimer >= CAL_BOOT_NEXT_MS) {
        calGestureState = GESTURE_IDLE;
        logEvent("[CAL] Huy: Qua thoi gian cho THA nut.");
      }
      break;

    case GESTURE_WAIT_AUTO_UP:
      if (stableSwitchMode == SW_AUTO_UP) {
        calGestureState = GESTURE_AUTO_UP_HOLDING;
        calGestureTimer = now;
      } else if (stableSwitchMode == SW_UP) {
        // Transitional detent on physical 4-mode switch when pulling to AUTO UP
      } else if (stableSwitchMode != SW_RELEASED) {
        calGestureState = GESTURE_IDLE;
      } else if (now - calGestureTimer >= CAL_BOOT_NEXT_MS) {
        calGestureState = GESTURE_IDLE;
        logEvent("[CAL] Huy: Qua thoi gian cho nhan AUTO UP.");
      }
      break;

    case GESTURE_AUTO_UP_HOLDING:
      if (stableSwitchMode != SW_AUTO_UP && stableSwitchMode != SW_UP) {
        calGestureState = GESTURE_IDLE;
        logEvent("[CAL] Huy: Tha AUTO UP qua som.");
      } else if (now - calGestureTimer >= CAL_BOOT_HOLD_MS) {
        calGestureState = GESTURE_IDLE;
        motorStop();
        calDownStarted = false;
        calMode = true;
        calPhase = CAL_GO_DOWN;
        logEvent("[CAL] KICH HOAT THANH CONG! Tha nut de kinh tu ha xuong BOTTOM.");
      }
      break;
  }
}

void exitCal(const String &result) {
  motorStop();
  calMode = false;
  calPhase = CAL_INACTIVE;
  calDownStarted = false;
  requireReleaseForNormal = true; // Yeu cau tha cong tac truoc khi nhan lenh normal
  lastCalResult = result;
  logEvent(String("[CAL] EXIT -> NORMAL | ") + result + " | Tha nut truoc khi dung NORMAL.");
}

void abortCal(const String &why) {
  motorStop();
  logEvent(String("[CAL] ERROR: ") + why + " | Baseline cu DUOC GIU NGUYEN.");
  exitCal("FAILED");
}

void computeAndSaveCal() {
  motorStop();
  if (calSampleCount < CAL_MIN_SAMPLES) {
    abortCal("Too few samples.");
    return;
  }

  int usable = (calSampleCount >= CAL_MIN_SAMPLES + 5) ? calSampleCount - 5 : calSampleCount;
  // Sap xep usable mau dau tien de loc ngoai lai va tinh p10, p90
  for (int i = 1; i < usable; i++) {
    int v = calSamples[i], j = i - 1;
    while (j >= 0 && calSamples[j] > v) {
      calSamples[j + 1] = calSamples[j];
      j--;
    }
    calSamples[j + 1] = v;
  }

  int trim = usable / 10;
  int kept = usable - 2 * trim;
  if (kept < CAL_MIN_SAMPLES / 2) {
    abortCal("Not enough filtered samples.");
    return;
  }

  int64_t sum = 0;
  for (int i = trim; i < usable - trim; i++) sum += calSamples[i];
  int baseline = int((sum + kept / 2) / kept);
  int p10 = calSamples[usable / 10];
  int p90 = calSamples[usable * 9 / 10];

  logEvent(String("[CAL] DATA: idle=") + String(calIdleMv) + " BASE=" + String(baseline) +
           "mV | p10=" + String(p10) + " p90=" + String(p90) + " | n=" + String(calSampleCount));

  if (baseline <= calIdleMv + CAL_MIN_SIGNAL_ABOVE_IDLE_MV) {
    abortCal("ACS signal too close to idle.");
    return;
  }
  if (p90 - p10 > CAL_MAX_P10_P90_SPREAD_MV) {
    abortCal("ACS noise/load variation too high.");
    return;
  }
  if (baseline < 100 || baseline + ANTI_PINCH_MARGIN_MV >= 3200) {
    abortCal("ADC range invalid.");
    return;
  }

  if (!saveCalibration(baseline, calIdleMv, p10, p90, calSampleCount)) {
    abortCal("NVS write FAILED.");
    return;
  }

  logEvent(String("[CAL] SAVED: BASE=") + String(baseline) + "mV + MARGIN=" +
           String(ANTI_PINCH_MARGIN_MV) + "mV => THRESHOLD=" +
           String(baseline + ANTI_PINCH_MARGIN_MV) + "mV");
  exitCal("SAVED");
}

void handleCal(unsigned long now) {
  if (limitsConflict()) {
    abortCal("TOP and BOTTOM simultaneously active.");
    return;
  }

  switch (calPhase) {
    case CAL_GO_DOWN: {
      // 1. Neu cham cong tac hanh trinh duoi
      if (bottomReached()) {
        motorStop();
        calPhase = CAL_WAIT_UP;
        logEvent("[CAL] BOTTOM reached. An va giu UP de nang len TOP hoc baseline.");
        break;
      }

      if (!calDownStarted) {
        if (stableSwitchMode != SW_RELEASED) return;
        if (!motorRun(false, MODE_CAL_DOWN)) {
          abortCal("Cannot start auto DOWN.");
          return;
        }
        calDownStarted = true;
        logEvent("[CAL] Auto DOWN in progress. Cho kinh ha toi BOTTOM.");
      } else {
        // 2. Neu nguoi dung an UP khi kinh da xuong day (hoac chu dong bat dau nang)
        if (stableSwitchMode == SW_UP || stableSwitchMode == SW_AUTO_UP) {
          motorStop();
          calPhase = CAL_WAIT_UP;
          logEvent("[CAL] Nhan lenh UP: Chuyen sang WAIT_UP de hoc baseline.");
          break;
        }

        // 3. Neu chay het thoi gian ha toi da (cho truong hop khong co cong tac duoi)
        if (now - motorStartMs >= CAL_HARD_TIMEOUT_MS) {
          motorStop();
          calPhase = CAL_WAIT_UP;
          logEvent("[CAL] Da ha het hanh trinh (BOTTOM). An va giu UP de nang len TOP hoc baseline.");
          break;
        }
      }
      break;
    }

    case CAL_WAIT_UP: {
      if (stableSwitchMode == SW_UP || stableSwitchMode == SW_AUTO_UP) {
        if (topReached()) {
          abortCal("TOP already active. Cannot learn.");
          return;
        }
        delay(50);
        calIdleMv = readAcsVoltageMv();
        calSampleCount = 0;
        calAbnormalSinceMs = 0;
        if (!motorRun(true, MODE_CAL_UP)) {
          abortCal("Cannot start UP.");
          return;
        }
        calStartMs = motorStartMs;
        calLastSampleMs = calStartMs;
        calPhase = CAL_COLLECT_UP;
        logEvent(String("[CAL] UP capture START. idle=") + String(calIdleMv) +
                 "mV | Chay len den hanh trinh TOP de hoc baseline.");
      } else if (stableSwitchMode == SW_DOWN || stableSwitchMode == SW_AUTO_DOWN) {
        abortCal("Nguoi dung huy CAL bang cong tac DOWN.");
      }
      break;
    }

    case CAL_COLLECT_UP: {
      // 1. Cong tac hanh trinh tren LIMIT_TOP (neu co)
      if (topReached()) {
        motorStop();
        logEvent("[CAL] TOP reached (Cong tac hanh trinh). Compute & auto SAVE.");
        computeAndSaveCal();
        return;
      }

      // 2. An toan: Nhan DOWN se huy CAL lap tuc
      if (stableSwitchMode == SW_DOWN || stableSwitchMode == SW_AUTO_DOWN) {
        abortCal("Nguoi dung huy CAL bang cong tac DOWN.");
        return;
      }

      // 3. Nguoi dung tha nut UP: Neu da du hanh trinh (>= CAL_MIN_SAMPLES), tu dong luu
      if (stableSwitchMode == SW_RELEASED) {
        if (calSampleCount >= CAL_MIN_SAMPLES) {
          motorStop();
          logEvent("[CAL] Nguoi dung tha nut UP tai TOP. Compute & auto SAVE.");
          computeAndSaveCal();
          return;
        } else {
          abortCal("Tha nut UP qua som (chua du mau hoc baseline).");
          return;
        }
      }

      // 4. Phat hien cham dinh co hoc (Top Stall) qua ACS712: dong tang >= idle + 550mV
      int sample = readAcsVoltageMv();
      if (now - calStartMs >= CAL_IGNORE_SAMPLE_START_MS) {
        if (sample - calIdleMv >= CAL_TOP_STALL_ABOVE_IDLE_MV) {
          if (!calAbnormalSinceMs) calAbnormalSinceMs = now;
          if (now - calAbnormalSinceMs >= CAL_TOP_STALL_CONFIRM_MS) {
            if (calSampleCount >= CAL_MIN_SAMPLES) {
              motorStop();
              logEvent("[CAL] Phat hien cham dinh co hoc tai TOP (Stall). Compute & auto SAVE.");
              computeAndSaveCal();
              return;
            } else {
              abortCal("Ket dong bat thuong khi vua bat dau CAL.");
              return;
            }
          }
        } else {
          calAbnormalSinceMs = 0;
        }
      }

      // 5. Gioi han thoi gian toi da (8s)
      if (now - calStartMs >= CAL_HARD_TIMEOUT_MS) {
        if (calSampleCount >= CAL_MIN_SAMPLES) {
          motorStop();
          logEvent("[CAL] Het thoi gian chay toi da (TOP). Compute & auto SAVE.");
          computeAndSaveCal();
          return;
        } else {
          abortCal("UP timeout before TOP.");
          return;
        }
      }

      // 6. Thu thap mau hanh trinh chay tu do (chi luu khi chua stall)
      if (now - calStartMs >= CAL_IGNORE_SAMPLE_START_MS &&
          now - calLastSampleMs >= CAL_SAMPLE_INTERVAL_MS) {
        calLastSampleMs = now;
        if (sample - calIdleMv < CAL_TOP_STALL_ABOVE_IDLE_MV) {
          if (calSampleCount < CAL_MAX_SAMPLES) {
            calSamples[calSampleCount++] = sample;
            if (calSampleCount % 25 == 0) {
              logEvent(String("[CAL] collected ") + String(calSampleCount) +
                       " samples | latest " + String(sample) + "mV");
            }
          }
        }
      }
      break;
    }

    default:
      break;
  }
}

// Chuoi nhan dien CAL luc boot: AUTO DOWN 2s -> RELEASE -> AUTO UP 2s -> RELEASE
bool bootHoldMode(SwitchMode expected, unsigned long holdMs, const char *label) {
  unsigned long since = millis();
  unsigned int reportedSec = 0;
  while (millis() - since < holdMs) {
    if (readSwitchMode() != expected) {
      logEvent(String("[CAL] Canceled hold ") + label);
      return false;
    }
    unsigned int sec = (millis() - since) / 1000UL;
    if (sec > reportedSec) {
      reportedSec = sec;
      logEvent(String("[CAL] Holding ") + label + " " + String(sec) + "s");
    }
    delay(20);
  }
  logEvent(String("[CAL] Hold ") + label + " OK");
  return true;
}

bool bootWaitMode(SwitchMode expected, unsigned long timeoutMs, const char *label) {
  unsigned long since = millis();
  unsigned long stableSince = 0;
  while (millis() - since < timeoutMs) {
    if (readSwitchMode() == expected) {
      if (!stableSince) stableSince = millis();
      if (millis() - stableSince >= 80) {
        logEvent(String("[CAL] ") + label + " detected");
        return true;
      }
    } else {
      stableSince = 0;
    }
    delay(20);
  }
  logEvent(String("[CAL] Timeout waiting for ") + label);
  return false;
}

void checkBootCal() {
  logEvent("[BOOT] Checking CAL gesture: AUTO DOWN 2s -> RELEASE -> AUTO UP 2s");
  if (readSwitchMode() != SW_AUTO_DOWN) {
    logEvent("[BOOT] Normal boot selected.");
    return;
  }
  delay(50);
  if (readSwitchMode() != SW_AUTO_DOWN) return;

  logEvent("[CAL] Step 1: AUTO DOWN detected at boot.");
  if (!bootHoldMode(SW_AUTO_DOWN, CAL_BOOT_HOLD_MS, "AUTO DOWN")) return;
  if (!bootWaitMode(SW_RELEASED, CAL_BOOT_NEXT_MS, "RELEASE")) return;
  if (!bootWaitMode(SW_AUTO_UP, CAL_BOOT_NEXT_MS, "AUTO UP")) return;
  if (!bootHoldMode(SW_AUTO_UP, CAL_BOOT_HOLD_MS, "AUTO UP")) return;

  calDownStarted = false;
  calMode = true;
  calPhase = CAL_GO_DOWN;
  logEvent("[CAL] ACTIVATED! Release switch; window will auto DOWN to BOTTOM.");
}
