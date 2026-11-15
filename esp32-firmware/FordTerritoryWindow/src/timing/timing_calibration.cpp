#include "timing_calibration.h"
#include "../logger/logger.h"
#include "../motor/motor_control.h"
#include "../storage/nvs_storage.h"
#include "../calibration/calibration.h"

MeasurePhase currentMeasurePhase = MEASURE_IDLE;
String lastTimingResult = "CHƯA CÓ KẾT QUẢ";

static unsigned long measureStartTimeMs = 0;
static unsigned long prepWaitSinceMs = 0;
static unsigned long prepStartMs = 0;

void timingInit() {
  currentMeasurePhase = MEASURE_IDLE;
}

bool startMeasureUp() {
  if (calMode) {
    logEvent("[TIMING] Lỗi: Hệ thống đang ở trong chế độ CAL baseline, không thể đo thời gian.");
    return false;
  }
  if (currentMeasurePhase != MEASURE_IDLE) {
    logEvent("[TIMING] Cảnh báo: Đang trong tiến trình đo khác.");
    return false;
  }

  logEvent("[TIMING] BẮT ĐẦU ĐO THỜI GIAN NÂNG KÍNH (UP)...");

  // Neu da o day san
  if (bottomReached()) {
    measureStartTimeMs = millis();
    currentMeasurePhase = MEASURE_UP_RUNNING;
    motorRun(true, MODE_AUTO);
    logEvent("[TIMING] Kính đã ở đáy BOTTOM! Bắt đầu chạy UP lên TOP để đo...");
  } else {
    // Tu dong ha xuong day de chuan bi
    currentMeasurePhase = MEASURE_UP_PREP_DOWN;
    prepStartMs = millis();
    motorRun(false, MODE_AUTO);
    logEvent("[TIMING] Kính chưa ở đáy. Đang tự động hạ xuống BOTTOM để chuẩn bị...");
  }
  return true;
}

bool startMeasureDown() {
  if (calMode) {
    logEvent("[TIMING] Lỗi: Hệ thống đang ở trong chế độ CAL baseline, không thể đo thời gian.");
    return false;
  }
  if (currentMeasurePhase != MEASURE_IDLE) {
    logEvent("[TIMING] Cảnh báo: Đang trong tiến trình đo khác.");
    return false;
  }

  logEvent("[TIMING] BẮT ĐẦU ĐO THỜI GIAN HẠ KÍNH (DOWN)...");

  // Neu da o dinh san
  if (topReached()) {
    measureStartTimeMs = millis();
    currentMeasurePhase = MEASURE_DOWN_RUNNING;
    motorRun(false, MODE_AUTO);
    logEvent("[TIMING] Kính đã ở đỉnh TOP! Bắt đầu chạy DOWN xuống BOTTOM để đo...");
  } else {
    // Tu dong nang len dinh de chuan bi
    currentMeasurePhase = MEASURE_DOWN_PREP_UP;
    prepStartMs = millis();
    motorRun(true, MODE_AUTO);
    logEvent("[TIMING] Kính chưa ở đỉnh. Đang tự động nâng lên TOP để chuẩn bị...");
  }
  return true;
}

void abortMeasure(const String &why) {
  if (currentMeasurePhase == MEASURE_IDLE) {
    return;
  }
  motorStop();
  currentMeasurePhase = MEASURE_IDLE;
  logEvent(String("[TIMING] ĐÃ HỦY ĐO: ") + why);
}

void handleTimingMeasure(unsigned long now) {
  if (currentMeasurePhase == MEASURE_IDLE) {
    return;
  }

  // --- TIẾN TRÌNH ĐO CHIỀU NÂNG (UP) ---
  if (currentMeasurePhase == MEASURE_UP_PREP_DOWN) {
    if (bottomReached()) {
      motorStop();
      prepWaitSinceMs = now;
      currentMeasurePhase = MEASURE_UP_WAIT_REVERSE;
      logEvent("[TIMING] Đã chạm đáy BOTTOM! Nghỉ 0.5s rồi tự động chạy UP để đo thời gian...");
    } else if (now - prepStartMs >= MAX_RUN_MS) {
      abortMeasure("Timeout khi tự hạ đáy chuẩn bị");
    }
  } else if (currentMeasurePhase == MEASURE_UP_WAIT_REVERSE) {
    if (now - prepWaitSinceMs >= 500) {
      measureStartTimeMs = millis();
      currentMeasurePhase = MEASURE_UP_RUNNING;
      motorRun(true, MODE_AUTO);
      logEvent("[TIMING] ⏱️ BẮT ĐẦU BẤM GIỜ: Kính đang chạy UP lên đỉnh TOP...");
    }
  } else if (currentMeasurePhase == MEASURE_UP_RUNNING) {
    if (topReached()) {
      unsigned long elapsed = millis() - measureStartTimeMs;
      motorStop();
      currentMeasurePhase = MEASURE_IDLE;
      saveTravelTimeUp(elapsed);
      lastTimingResult = String("UP: ") + String(elapsed) + "ms";
      logEvent(String("[TIMING] ✅ ĐO THÀNH CÔNG THỜI GIAN NÂNG: ") + String(elapsed) + " ms (" + String(elapsed / 1000.0, 2) + "s). Đã lưu vào NVS Flash!");
    } else if (now - measureStartTimeMs >= MAX_RUN_MS) {
      abortMeasure("Timeout khi chạy UP đo thời gian");
    }
  }

  // --- TIẾN TRÌNH ĐO CHIỀU HẠ (DOWN) ---
  else if (currentMeasurePhase == MEASURE_DOWN_PREP_UP) {
    if (topReached()) {
      motorStop();
      prepWaitSinceMs = now;
      currentMeasurePhase = MEASURE_DOWN_WAIT_REVERSE;
      logEvent("[TIMING] Đã chạm đỉnh TOP! Nghỉ 0.5s rồi tự động chạy DOWN để đo thời gian...");
    } else if (now - prepStartMs >= MAX_RUN_MS) {
      abortMeasure("Timeout khi tự nâng đỉnh chuẩn bị");
    }
  } else if (currentMeasurePhase == MEASURE_DOWN_WAIT_REVERSE) {
    if (now - prepWaitSinceMs >= 500) {
      measureStartTimeMs = millis();
      currentMeasurePhase = MEASURE_DOWN_RUNNING;
      motorRun(false, MODE_AUTO);
      logEvent("[TIMING] ⏱️ BẮT ĐẦU BẤM GIỜ: Kính đang chạy DOWN xuống đáy BOTTOM...");
    }
  } else if (currentMeasurePhase == MEASURE_DOWN_RUNNING) {
    if (bottomReached()) {
      unsigned long elapsed = millis() - measureStartTimeMs;
      motorStop();
      currentMeasurePhase = MEASURE_IDLE;
      saveTravelTimeDown(elapsed);
      lastTimingResult = String("DOWN: ") + String(elapsed) + "ms";
      logEvent(String("[TIMING] ✅ ĐO THÀNH CÔNG THỜI GIAN HẠ: ") + String(elapsed) + " ms (" + String(elapsed / 1000.0, 2) + "s). Đã lưu vào NVS Flash!");
    } else if (now - measureStartTimeMs >= MAX_RUN_MS) {
      abortMeasure("Timeout khi chạy DOWN đo thời gian");
    }
  }
}

const char *measurePhaseName() {
  switch (currentMeasurePhase) {
    case MEASURE_UP_PREP_DOWN: return "ĐANG HẠ ĐÁY ĐO UP";
    case MEASURE_UP_WAIT_REVERSE: return "CHỜ ĐỔI CHIỀU UP";
    case MEASURE_UP_RUNNING: return "ĐANG BẤM GIỜ UP";
    case MEASURE_DOWN_PREP_UP: return "ĐANG LÊN ĐỈNH ĐO DOWN";
    case MEASURE_DOWN_WAIT_REVERSE: return "CHỜ ĐỔI CHIỀU DOWN";
    case MEASURE_DOWN_RUNNING: return "ĐANG BẤM GIỜ DOWN";
    default: return "IDLE";
  }
}
