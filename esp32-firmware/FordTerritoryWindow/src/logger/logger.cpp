#include "logger.h"

// Ring buffer toan cuc
char logRing[LOG_CAPACITY][LOG_LINE_LEN] = {};
size_t logNext = 0;
size_t logCount = 0;

// Tham chieu cac bien trang thai he thong
extern MotorState motorState;
extern bool calMode;
extern CalPhase calPhase;

void logEvent(const String &message) {
  char *dest = logRing[logNext];
  snprintf(dest, LOG_LINE_LEN, "[%lus] %s", millis() / 1000UL, message.c_str());
  Serial.println(dest);
  logNext = (logNext + 1) % LOG_CAPACITY;
  if (logCount < LOG_CAPACITY) ++logCount;
}

const char *switchName(SwitchMode m) {
  switch (m) {
    case SW_RELEASED: return "RELEASED";
    case SW_UP: return "UP MANUAL";
    case SW_AUTO_UP: return "AUTO UP";
    case SW_DOWN: return "DOWN MANUAL";
    case SW_AUTO_DOWN: return "AUTO DOWN";
    default: return "UNKNOWN";
  }
}

const char *motorName() {
  switch (motorState) {
    case MOVING_UP: return "UP";
    case MOVING_DOWN: return "DOWN";
    default: return "STOPPED";
  }
}

const char *modeName() {
  return calMode ? "CALIBRATION" : "NORMAL";
}

const char *phaseName() {
  if (!calMode) return "NORMAL";
  switch (calPhase) {
    case CAL_GO_DOWN: return "CAL: AUTO DOWN";
    case CAL_WAIT_UP: return "CAL: HOLD MANUAL UP";
    case CAL_COLLECT_UP: return "CAL: RECORDING UP";
    case CAL_FAILED: return "CAL: ERROR";
    default: return "CAL: START";
  }
}
