/*
 * FORD TERRITORY - WINDOW CONTROL FIRMWARE
 * Hardware: ESP32-S3 N16R8, BTS7960 Motor Driver, ACS712 Current Sensor, 
 *           4-position Analog Window Switch, NC Top/Bottom Limit Switches.
 *
 * Core Architecture:
 * - Restored original motor-control state machine and debounced switch decoding.
 * - Integrated Standalone Wi-Fi AP (FORD-WINDOW-DEBUG / 192.168.4.1) & Read-only Dashboard.
 * - Integrated Calibration Sequence (Boot AUTO DOWN 2s -> RELEASE -> AUTO UP 2s).
 * - Integrated NVS Baseline Storage & Experimental Adaptive Anti-Pinch (baseline + 90mV).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <stddef.h>
#include <stdint.h>

// =====================================================
// 1. HARDWARE PIN DEFINITIONS (ESP32-S3)
// =====================================================

// Driver BTS7960
#define RPWM_PIN 4
#define LPWM_PIN 5
#define REN_PIN  6
#define LEN_PIN  7

// Cong tac hanh trinh (NC: binh thuong LOW, cham/ho mach HIGH)
#define LIMIT_TOP    8
#define LIMIT_BOTTOM 9

// Cong tac kinh 4 che do (ADC GPIO1)
#define SWITCH_ADC_PIN 1

// ACS712 OUT sau mach chia ap (ADC GPIO10)
#define ACS_ADC_PIN 10


// =====================================================
// 2. CAU HINH & THAM SO HOAT DONG
// =====================================================

// Chong doi cong tac goc (ms)
const unsigned long SWITCH_DEBOUNCE_MS = 40;

// Gioi han chay motor toi da (ms)
const unsigned long MAX_RUN_MS = 8000;

// Thoi gian ha kinh khi dao chieu chong ket (ms)
const unsigned long ANTI_PINCH_REVERSE_MS = 1000;

// Thoi gian chet bao ve H-bridge khi bat/dao chieu motor (ms)
const unsigned long MOTOR_DIRECTION_DEADTIME_MS = 150;

// Bo qua dong khoi dong motor (ms)
const unsigned long ACS_IGNORE_START_MS = 350;

// Thoi gian xac nhan dien ap bat thuong de kich hoat chong ket (ms)
const unsigned long ACS_STALL_CONFIRM_MS = 150;

// Chu ky in thong so debug len serial/log (ms)
const unsigned long DEBUG_INTERVAL_MS = 250;

// --- NGUONG THU NGHIEM CHONG KET ---
const int ANTI_PINCH_MARGIN_MV = 50;            // Nguong = baseline + 90mV
const bool ENABLE_EXPERIMENTAL_REVERSE = false; // false: STOP + LOG; true: dao chieu 1s (chi bat sau khi kiem chung an toan)

// --- THAM SO HIEU CHUAN (CALIBRATION) ---
const unsigned long CAL_BOOT_HOLD_MS = 2000;
const unsigned long CAL_BOOT_NEXT_MS = 3000;
const unsigned long CAL_SAMPLE_INTERVAL_MS = 40;
const unsigned long CAL_IGNORE_SAMPLE_START_MS = 550;
const unsigned long CAL_HARD_TIMEOUT_MS = 8000;
const int CAL_MIN_SAMPLES = 20;
const int CAL_MAX_SAMPLES = 210;
const int CAL_MIN_SIGNAL_ABOVE_IDLE_MV = 40;
const int CAL_MAX_P10_P90_SPREAD_MV = 250;
const int CAL_TOP_STALL_ABOVE_IDLE_MV = 550;
const unsigned long CAL_TOP_STALL_CONFIRM_MS = 150;

// --- WI-FI AP & DASHBOARD ---
const char *AP_SSID = "FORD-WINDOW-DEBUG";
const char *AP_PASSWORD = "FordWindow!26";
const size_t LOG_CAPACITY = 90;
const size_t LOG_LINE_LEN = 148;


// =====================================================
// 3. CAC TRANG THAI & BIEN TOAN CUC
// =====================================================

enum MotorState {
  STOPPED,
  MOVING_UP,
  MOVING_DOWN
};

enum SwitchMode {
  SW_UNKNOWN,
  SW_RELEASED,
  SW_UP,
  SW_AUTO_UP,
  SW_DOWN,
  SW_AUTO_DOWN
};

enum RunMode {
  MODE_MANUAL,
  MODE_AUTO,
  MODE_ANTI_PINCH_REVERSE,
  MODE_CAL_DOWN,
  MODE_CAL_UP
};

enum CalPhase {
  CAL_INACTIVE,
  CAL_GO_DOWN,
  CAL_WAIT_UP,
  CAL_COLLECT_UP,
  CAL_FAILED
};

// Bien dieu khien motor & cong tac goc
MotorState motorState = STOPPED;
RunMode currentRunMode = MODE_MANUAL;
SwitchMode stableSwitchMode = SW_UNKNOWN;
SwitchMode candidateSwitchMode = SW_UNKNOWN;
unsigned long candidateSince = 0;
unsigned long motorStartMs = 0;
int switchVoltageMv = 0;
int latestAcsMv = 0;

// Bien theo doi chong ket ACS712
bool acsHighDetected = false;
unsigned long acsHighSince = 0;
unsigned long lastDebugMs = 0;
unsigned long lastStatusSampleMs = 0;

// Bien che do CAL
bool calMode = false;
CalPhase calPhase = CAL_INACTIVE;
bool calDownStarted = false;
bool requireReleaseForNormal = false;
unsigned long calStartMs = 0;
unsigned long calLastSampleMs = 0;
unsigned long calAbnormalSinceMs = 0;
int calIdleMv = 0;
int calSamples[CAL_MAX_SAMPLES] = {};
int calSampleCount = 0;
String lastCalResult = "CHUA HIEU CHUAN";

// Cau truc luu tru NVS
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
const uint32_t CAL_MAGIC = 0x464F5244u;
const uint32_t CAL_VERSION = 1u;
CalibrationRecord savedCal = {};
bool baselineValid = false;

// Web server & log buffer
WebServer webServer(80);
bool apReady = false;
char logRing[LOG_CAPACITY][LOG_LINE_LEN] = {};
size_t logNext = 0;
size_t logCount = 0;


// =====================================================
// 4. LOGGING & TIEN ICH
// =====================================================

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

const char *modeName() { return calMode ? "CALIBRATION" : "NORMAL"; }

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


// =====================================================
// 5. CONG TAC HANH TRINH (LIMIT SWITCHES)
// =====================================================

// NC limit switch: binh thuong LOW, ho mach / cham hanh trinh = HIGH.
bool topReached() {
  return digitalRead(LIMIT_TOP) == HIGH;
}

bool bottomReached() {
  return digitalRead(LIMIT_BOTTOM) == HIGH;
}

bool limitsConflict() {
  return topReached() && bottomReached();
}


// =====================================================
// 6. DUNG MOTOR (GOC)
// =====================================================

void motorStop() {
  digitalWrite(RPWM_PIN, LOW);
  digitalWrite(LPWM_PIN, LOW);
  digitalWrite(REN_PIN, LOW);
  digitalWrite(LEN_PIN, LOW);

  motorState = STOPPED;

  // Xoa bo dem phat hien ket
  acsHighDetected = false;
  acsHighSince = 0;
}


// =====================================================
// 7. DIEU KHIEN MOTOR NANG / HA (GOC)
// =====================================================

bool motorRun(bool up, RunMode requestedMode) {
  // Dung truoc khi thay doi chieu chay
  motorStop();
  delay(MOTOR_DIRECTION_DEADTIME_MS);

  // Bao ve neu ca hai hanh trinh cung kich hoat
  if (limitsConflict()) {
    logEvent("[SAFE] LOI: Ca hai cong tac hanh trinh cung kich hoat! Motor locked.");
    currentRunMode = MODE_MANUAL;
    return false;
  }

  // Khong cho nang neu da cham gioi han tren
  if (up && topReached()) {
    logEvent("[LIMIT] Da cham hanh trinh tren. UP blocked.");
    currentRunMode = MODE_MANUAL;
    return false;
  }

  // Khong cho ha neu da cham gioi han duoi
  if (!up && bottomReached()) {
    logEvent("[LIMIT] Da cham hanh trinh duoi. DOWN blocked.");
    currentRunMode = MODE_MANUAL;
    return false;
  }

  // Chon chieu quay motor (BTS7960)
  if (up) {
    digitalWrite(LPWM_PIN, LOW);
    digitalWrite(RPWM_PIN, HIGH);
  } else {
    digitalWrite(RPWM_PIN, LOW);
    digitalWrite(LPWM_PIN, HIGH);
  }

  // Enable BTS7960
  digitalWrite(REN_PIN, HIGH);
  digitalWrite(LEN_PIN, HIGH);

  if (up) {
    motorState = MOVING_UP;
    logEvent("[MOTOR] NANG KINH (UP) started.");
  } else {
    motorState = MOVING_DOWN;
    if (requestedMode == MODE_ANTI_PINCH_REVERSE) {
      logEvent("[CHONG KET] HA KINH TRONG TOI DA 1 GIAY.");
    } else {
      logEvent("[MOTOR] HA KINH (DOWN) started.");
    }
  }

  currentRunMode = requestedMode;
  motorStartMs = millis();

  // Reset bo dem ACS712
  acsHighDetected = false;
  acsHighSince = 0;
  return true;
}


// =====================================================
// 8. DOC CONG TAC KINH 4 CHE DO (GOC + LOC NHIEU)
// =====================================================

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


// =====================================================
// 9. XU LY CONG TAC 4 CHE DO (GOC + AN TOAN BASELINE)
// =====================================================

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


// =====================================================
// 10. GIAM SAT HANH TRINH & TIMEOUT (GOC)
// =====================================================

void monitorLimits() {
  if (motorState == STOPPED) {
    return;
  }

  if (limitsConflict()) {
    logEvent("[SAFE] LOI: Hai cong tac hanh trinh cung kich hoat! DUNG MOTOR.");
    motorStop();
    currentRunMode = MODE_MANUAL;
    return;
  }

  // Dang nang ma cham hanh trinh tren
  if (motorState == MOVING_UP && topReached()) {
    logEvent("[LIMIT] HANH TRINH TREN: DUNG MOTOR.");
    motorStop();
    currentRunMode = MODE_MANUAL;
    return;
  }

  // Dang ha ma cham hanh trinh duoi
  if (motorState == MOVING_DOWN && bottomReached()) {
    logEvent("[LIMIT] HANH TRINH DUOI: DUNG MOTOR.");
    motorStop();
    currentRunMode = MODE_MANUAL;
    return;
  }
}

void monitorRunTimeout() {
  if (motorState == STOPPED) {
    return;
  }

  if (millis() - motorStartMs >= MAX_RUN_MS) {
    logEvent("[SAFE] CANH BAO: Motor chay qua thoi gian cho phep (MAX_RUN_MS). DUNG MOTOR.");
    motorStop();
    currentRunMode = MODE_MANUAL;
  }
}


// =====================================================
// 11. DOC DIEN AP ACS712 (GOC)
// =====================================================

int readAcsVoltageMv() {
  // Trung binh 10 mau ADC
  long totalMv = 0;
  for (int i = 0; i < 10; i++) {
    totalMv += analogReadMilliVolts(ACS_ADC_PIN);
    delayMicroseconds(200);
  }
  return int(totalMv / 10);
}


// =====================================================
// 12. CHONG KET CHI KHI AUTO UP (BASELINE + 90mV)
// =====================================================

void monitorAntiPinch() {
  // Chi kiem tra khi dang nang tu dong va da co baseline
  if (motorState != MOVING_UP || currentRunMode != MODE_AUTO || !baselineValid) {
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

  // So sanh voi nguong thu nghiem (baseline + 90mV)
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

      if (ENABLE_EXPERIMENTAL_REVERSE && !bottomReached()) {
        motorRun(false, MODE_ANTI_PINCH_REVERSE);
        logEvent("[CHONG KET] Bat dau ha kinh toi da 1s.");
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
  }
}


// =====================================================
// 13. NVS PREFERENCES (LUU & DOC BASELINE)
// =====================================================

uint32_t checksumCal(const CalibrationRecord &r) {
  const uint8_t *bytes = (const uint8_t*)&r;
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < offsetof(CalibrationRecord, checksum); i++) {
    h ^= bytes[i];
    h *= 16777619u;
  }
  return h;
}

void loadCalibration() {
  baselineValid = false;
  Preferences prefs;
  if (!prefs.begin("fordwin", false)) {
    logEvent("[NVS] Cannot open preferences.");
    return;
  }

  CalibrationRecord rec = {};
  size_t n = 0;
  if (prefs.getBytesLength("cal_v1") == sizeof(rec)) {
    n = prefs.getBytes("cal_v1", &rec, sizeof(rec));
  }
  prefs.end();

  if (n == sizeof(rec) && rec.magic == CAL_MAGIC && rec.version == CAL_VERSION &&
      rec.checksum == checksumCal(rec) && rec.baselineMv >= 100 && rec.baselineMv <= 3100 &&
      rec.samples >= CAL_MIN_SAMPLES) {
    savedCal = rec;
    baselineValid = true;
    logEvent(String("[NVS] Loaded BASE=") + String(savedCal.baselineMv) + "mV | TEST LIMIT=" +
             String(savedCal.baselineMv + ANTI_PINCH_MARGIN_MV) + "mV");
  } else {
    logEvent("[NVS] No baseline. AUTO UP disabled until CAL success.");
  }
}

bool saveCalibration(int baseline, int idle, int p10, int p90, int count) {
  CalibrationRecord r = {};
  r.magic = CAL_MAGIC;
  r.version = CAL_VERSION;
  r.idleMv = idle;
  r.baselineMv = baseline;
  r.marginMvAtSave = ANTI_PINCH_MARGIN_MV;
  r.p10Mv = p10;
  r.p90Mv = p90;
  r.samples = count;
  r.checksum = checksumCal(r);

  Preferences prefs;
  if (!prefs.begin("fordwin", false)) return false;
  size_t n = prefs.putBytes("cal_v1", &r, sizeof(r));
  prefs.end();

  if (n != sizeof(r)) return false;
  savedCal = r;
  baselineValid = true;
  return true;
}


// =====================================================
// 14. CALIBRATION STATE MACHINE & GESTURE TRACKER
// =====================================================

enum CalGestureState {
  GESTURE_IDLE,
  GESTURE_AUTO_DOWN_HOLDING,
  GESTURE_WAIT_RELEASE_1,
  GESTURE_WAIT_AUTO_UP,
  GESTURE_AUTO_UP_HOLDING
};
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
  // Sap xepusable mau dau tien de loc ngoai lai va tinh p10, p90
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


// =====================================================
// 15. WIFI AP & READ-ONLY DASHBOARD
// =====================================================

String escapeJson(const String &str) {
  String s;
  s.reserve(str.length() + 8);
  for (size_t i = 0; i < str.length(); i++) {
    char c = str[i];
    if (c == '"') s += "\\\"";
    else if (c == '\\') s += "\\\\";
    else if (c == '\n') s += "\\n";
    else if (c == '\r') s += "\\r";
    else if ((unsigned char)c < 0x20) s += ' ';
    else s += c;
  }
  return s;
}

static const char WEB_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="vi"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ford Window | ESP32-S3</title>
<style>
:root{font-family:system-ui,-apple-system,sans-serif;color-scheme:dark}body{margin:0;background:#0d1728;color:#f0f4fc}
main{max-width:960px;margin:auto;padding:18px 14px}h1{font-size:1.45rem;margin:0 0 9px}
.notice{color:#ffcf8c;line-height:1.45}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(155px,1fr));gap:10px}
.card{background:#1a2941;border:1px solid #314562;border-radius:11px;padding:12px}
.label{font-size:.78rem;color:#a7bbd6}.value{font-weight:750;font-size:1.17rem;margin-top:7px;overflow-wrap:anywhere}
pre{white-space:pre-wrap;min-height:220px;max-height:450px;overflow:auto;background:#07101e;padding:12px;border:1px solid #314562;border-radius:10px;font-size:.82rem;line-height:1.45}
.small{font-size:.85rem;color:#b7c7da;line-height:1.55}h2{font-size:1.05rem;margin-top:18px}
</style></head><body><main>
<h1>Ford Territory — Window Control</h1>
<p class="notice">BẢN THỬ NGHIỆM. Ngưỡng ACS712 +90mV chưa chứng minh an toàn chống kẹt. Chỉ thử với đồ gá an toàn, không dùng tay.</p>
<div class="grid">
<div class="card"><div class="label">Mode</div><div class="value" id="mode">—</div></div>
<div class="card"><div class="label">Calibration</div><div class="value" id="phase">—</div></div>
<div class="card"><div class="label">Motor</div><div class="value" id="motor">—</div></div>
<div class="card"><div class="label">Công tắc</div><div class="value" id="sw">—</div></div>
<div class="card"><div class="label">Công tắc ADC GPIO1</div><div class="value" id="swmv">—</div></div>
<div class="card"><div class="label">ACS712 ADC GPIO10</div><div class="value" id="acsmv">—</div></div>
<div class="card"><div class="label">Baseline đã lưu</div><div class="value" id="base">—</div></div>
<div class="card"><div class="label">Ngưỡng thử (BASE + 90)</div><div class="value" id="thresh">—</div></div>
<div class="card"><div class="label">Mẫu CAL / kết quả</div><div class="value" id="samples">—</div></div>
<div class="card"><div class="label">Hành trình</div><div class="value" id="limits">—</div></div>
<div class="card"><div class="label">Thiết bị kết nối</div><div class="value" id="clients">—</div></div>
</div>
<h2>Cách hiệu chuẩn (không cần USB)</h2>
<p class="small">Lúc khởi động hoặc bình thường: <b>AUTO DOWN giữ 2 giây → THẢ → AUTO UP giữ 2 giây → THẢ.</b><br>
ESP32 tự hạ tới BOTTOM. Sau đó <b>ấn và giữ UP</b> để kính chạy lên cho tới công tắc hành trình TOP.<br>
ESP32 tự tính BASELINE, tự lưu Flash và tự về NORMAL.</p>
<h2>Log</h2><pre id="log">Đang kết nối...</pre><p id="conn" class="small"></p>
<script>
const e=id=>document.getElementById(id);
let inFlight=false;
async function tick(){
 if(inFlight)return;
 inFlight=true;
 try{
  const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);const s=await r.json();
  e('mode').textContent=s.mode;e('phase').textContent=s.phase;e('motor').textContent=s.motor;
  e('sw').textContent=s.switch;e('swmv').textContent=s.switch_mv+' mV';e('acsmv').textContent=s.acs_mv+' mV';
  e('base').textContent=s.saved_valid?s.baseline_mv+' mV':'CHƯA CÓ';
  e('thresh').textContent=s.saved_valid?s.threshold_mv+' mV (thử nghiệm)':'CHƯA CÓ';
  e('samples').textContent=s.samples+' mẫu / '+s.result;
  e('limits').textContent=(s.top?'TOP CHẠM':'TOP OK')+' / '+(s.bottom?'BOTTOM CHẠM':'BOTTOM OK');
  e('clients').textContent=s.clients;e('conn').textContent='ESP32 AP: '+s.ip+' | Realtime Live';
  if(s.lines){
    const el=e('log'),follow=el.scrollHeight-el.scrollTop-el.clientHeight<60;
    el.textContent=s.lines.join('\n')||'(trống)';
    if(follow) el.scrollTop=el.scrollHeight;
  }
 }catch(err){e('conn').textContent='Đang cập nhật... '+err.message;}
 finally{inFlight=false;setTimeout(tick,350);}
}tick();
</script></main></body></html>
)HTML";

void sendStatus() {
  String s;
  s.reserve(4500);
  s += "{\"mode\":\"" + String(modeName()) + "\",";
  s += "\"phase\":\"" + String(phaseName()) + "\",";
  s += "\"motor\":\"" + String(motorName()) + "\",";
  s += "\"switch\":\"" + String(switchName(stableSwitchMode)) + "\",";
  s += "\"switch_mv\":" + String(switchVoltageMv) + ",";
  s += "\"acs_mv\":" + String(latestAcsMv) + ",";
  s += "\"baseline_mv\":" + String(baselineValid ? savedCal.baselineMv : 0) + ",";
  s += "\"margin_mv\":" + String(ANTI_PINCH_MARGIN_MV) + ",";
  s += "\"threshold_mv\":" + String(baselineValid ? savedCal.baselineMv + ANTI_PINCH_MARGIN_MV : 0) + ",";
  s += "\"saved_valid\":" + String(baselineValid ? "true" : "false") + ",";
  s += "\"samples\":" + String(calSampleCount) + ",";
  s += "\"result\":\"" + escapeJson(lastCalResult) + "\",";
  s += "\"top\":" + String(topReached() ? "true" : "false") + ",";
  s += "\"bottom\":" + String(bottomReached() ? "true" : "false") + ",";
  s += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
  s += "\"clients\":" + String(WiFi.softAPgetStationNum()) + ",";
  s += "\"lines\":[";
  size_t count = logCount > 35 ? 35 : logCount;
  size_t first = (logNext + LOG_CAPACITY - count) % LOG_CAPACITY;
  for (size_t i = 0; i < count; i++) {
    if (i) s += ',';
    s += '"';
    s += escapeJson(logRing[(first + i) % LOG_CAPACITY]);
    s += '"';
  }
  s += "]}";
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(200, "application/json; charset=utf-8", s);
}

void sendLogs() {
  String s = "{\"lines\":[";
  s.reserve(13000);
  size_t first = (logNext + LOG_CAPACITY - logCount) % LOG_CAPACITY;
  for (size_t i = 0; i < logCount; i++) {
    if (i) s += ',';
    s += '"';
    s += escapeJson(logRing[(first + i) % LOG_CAPACITY]);
    s += '"';
  }
  s += "]}";
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(200, "application/json; charset=utf-8", s);
}

void startAP() {
  WiFi.mode(WIFI_AP);
  IPAddress ip(192, 168, 4, 1), mask(255, 255, 255, 0);
  WiFi.softAPConfig(ip, ip, mask);
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    logEvent("[WIFI] AP start failed.");
    return;
  }
  apReady = true;
  webServer.on("/", HTTP_GET, []() {
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
  });
  webServer.on("/api/status", HTTP_GET, sendStatus);
  webServer.on("/api/logs", HTTP_GET, sendLogs);
  webServer.onNotFound([]() { webServer.send(404, "text/plain", "Not found"); });
  webServer.begin();
  logEvent(String("[WIFI] AP ready: ") + AP_SSID + " | http://192.168.4.1");
}


// =====================================================
// 16. SETUP
// =====================================================

void setup() {
  // De-energize BTS7960 as early as possible
  pinMode(RPWM_PIN, OUTPUT);
  pinMode(LPWM_PIN, OUTPUT);
  pinMode(REN_PIN, OUTPUT);
  pinMode(LEN_PIN, OUTPUT);
  motorStop();

  Serial.begin(115200);
  delay(100);

  pinMode(LIMIT_TOP, INPUT_PULLUP);
  pinMode(LIMIT_BOTTOM, INPUT_PULLUP);

  pinMode(SWITCH_ADC_PIN, INPUT);
  pinMode(ACS_ADC_PIN, INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(SWITCH_ADC_PIN, ADC_11db);
  analogSetPinAttenuation(ACS_ADC_PIN, ADC_11db);

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


// =====================================================
// 17. LOOP
// =====================================================

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
  if (apReady) {
    webServer.handleClient();
  }

  delay(5);
}
