#include "motor_control.h"
#include "../logger/logger.h"

MotorState motorState = STOPPED;
RunMode currentRunMode = MODE_MANUAL;
unsigned long motorStartMs = 0;

// Tham chieu bo dem phat hien ket ACS712
extern bool acsHighDetected;
extern unsigned long acsHighSince;

void motorInit() {
  pinMode(RPWM_PIN, OUTPUT);
  pinMode(LPWM_PIN, OUTPUT);
  pinMode(REN_PIN, OUTPUT);
  pinMode(LEN_PIN, OUTPUT);

  pinMode(LIMIT_TOP, INPUT_PULLUP);
  pinMode(LIMIT_BOTTOM, INPUT_PULLUP);

  motorStop();
}

bool topReached() {
  return digitalRead(LIMIT_TOP) == HIGH;
}

bool bottomReached() {
  return digitalRead(LIMIT_BOTTOM) == HIGH;
}

bool limitsConflict() {
  return topReached() && bottomReached();
}

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

bool motorRun(bool up, RunMode requestedMode) {
  // Dung truoc khi thay doi chieu chay de dam bao an toan
  motorStop();
  delay(MOTOR_DIRECTION_DEADTIME_MS);

  // Bao ve neu ca hai hanh trinh cung kich hoat (loi phan cung)
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
