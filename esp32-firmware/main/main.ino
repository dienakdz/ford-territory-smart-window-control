/*
 * FORD TERRITORY - WINDOW CONTROL FIRMWARE (ESP32-S3)
 * Modular Architecture
 * 
 * Modules:
 * - config.h: Pin definitions & operating thresholds
 * - types.h: Enums & data structures
 * - logger.h / .cpp: Serial & web log ring buffer
 * - motor_control.h / .cpp: BTS7960 driver & limit switch monitors
 * - switch_control.h / .cpp: 4-position window switch ADC reading & debounce
 * - anti_pinch.h / .cpp: ACS712 current sensing & anti-pinch logic
 * - nvs_storage.h / .cpp: Non-volatile storage of baseline record
 * - calibration.h / .cpp: Gesture & calibration state machine
 * - web_server.h / .cpp: Standalone AP & real-time debug dashboard
 */

#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "logger.h"
#include "nvs_storage.h"
#include "motor_control.h"
#include "switch_control.h"
#include "anti_pinch.h"
#include "calibration.h"
#include "web_server.h"

unsigned long lastStatusSampleMs = 0;

void setup() {
  // Khoi tao BTS7960 ve muc ngat an toan ngay tu dau
  motorInit();

  Serial.begin(115200);
  delay(100);

  // Khoi tao cac chan ADC
  switchInit();
  antiPinchInit();
  analogReadResolution(12);

  logEvent("[BOOT] Ford Territory Window Control Starting...");
  loadCalibration();
  startAP();
  checkBootCal();

  latestAcsMv = readAcsVoltageMv();
  if (calMode) {
    logEvent("[CAL] CALIBRATION ACTIVATED. Clear pinch zone, motor will auto DOWN.");
  } else {
    logEvent("[MODE] NORMAL MODE. De vao CAL: Giu AUTO DOWN 2s -> THA -> Giu AUTO UP 2s.");
  }
}

void loop() {
  unsigned long now = millis();

  // Luon doc va chong doi cong tac o MOI che do (ca NORMAL va CAL)
  SwitchMode reading = readSwitchMode();

  // Chong doi cong tac goc
  if (reading != candidateSwitchMode) {
    candidateSwitchMode = reading;
    candidateSince = now;
  }

  if (now - candidateSince >= SWITCH_DEBOUNCE_MS) {
    if (stableSwitchMode != candidateSwitchMode) {
      stableSwitchMode = candidateSwitchMode;
      logEvent(String("[SWITCH] ") + switchName(stableSwitchMode) + " | " + String(switchVoltageMv) + "mV");

      if (!calMode) {
        if (requireReleaseForNormal) {
          if (stableSwitchMode == SW_RELEASED) {
            requireReleaseForNormal = false;
            logEvent("[MODE] NORMAL ready for commands.");
          }
        } else {
          handleSwitchModeChange(stableSwitchMode);
        }
      }
    }
  }

  if (calMode) {
    handleCal(now);
  } else {
    // --- ORIGINAL WORKING NORMAL MOTOR CONTROL FLOW ---
    monitorLimits();
    monitorRunTimeout();
    monitorAntiPinch();
    monitorAntiPinchReverse();

    // Theo doi to hop CAL gesture bat ky luc nao khi o NORMAL
    trackCalGesture(now);
  }

  // Cap nhat telemetry doc cam bien phuc vu dashboard (non-blocking)
  if (now - lastStatusSampleMs >= 200) {
    lastStatusSampleMs = now;
    latestAcsMv = readAcsVoltageMv();
  }

  // Web server xu ly realtime ngay ca khi motor dang chay
  handleWebClient();

  delay(5);
}
