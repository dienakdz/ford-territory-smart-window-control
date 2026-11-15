#include "motor_control.h"
#include "../logger/logger.h"

MotorState motorState = STOPPED;
RunMode currentRunMode = MODE_MANUAL;
unsigned long motorStartMs = 0;
unsigned long motorTimedTargetMs = 0;
volatile bool isVoiceDriven = false;

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
  analogWrite(RPWM_PIN, 0);
  analogWrite(LPWM_PIN, 0);
  digitalWrite(REN_PIN, LOW);
  digitalWrite(LEN_PIN, LOW);

  motorState = STOPPED;
  motorTimedTargetMs = 0;
  isVoiceDriven = false; // Dong cua so lang nghe ngat giong noi khi motor dung

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

  // Chon chieu quay motor (BTS7960 dieu xung PWM giup kinh chay em ai ~4-5s)
  if (up) {
    analogWrite(LPWM_PIN, 0);
    analogWrite(RPWM_PIN, MOTOR_SPEED_PWM);
  } else {
    analogWrite(RPWM_PIN, 0);
    analogWrite(LPWM_PIN, MOTOR_SPEED_PWM);
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

bool motorRunTimed(bool up, unsigned long durationMs) {
  bool ok = motorRun(up, MODE_TIMED_STEP);
  if (ok) {
    motorTimedTargetMs = durationMs;
  }
  return ok;
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
    unsigned long elapsed = millis() - motorStartMs;
    logEvent(String("[LIMIT] CHAM DINH TOP! Thoi gian chay UP: ") + String(elapsed) + " ms (" + String(elapsed / 1000.0, 2) + "s)");
    motorStop();
    currentRunMode = MODE_MANUAL;
    return;
  }

  // Dang ha ma cham hanh trinh duoi
  if (motorState == MOVING_DOWN && bottomReached()) {
    unsigned long elapsed = millis() - motorStartMs;
    logEvent(String("[LIMIT] CHAM DAY BOTTOM! Thoi gian chay DOWN: ") + String(elapsed) + " ms (" + String(elapsed / 1000.0, 2) + "s)");
    motorStop();
    currentRunMode = MODE_MANUAL;
    return;
  }
}

void monitorRunTimeout() {
  if (motorState == STOPPED) {
    return;
  }

  // Kiem tra hoan tat buoc thoi gian dinh san (1/2, 1/3...)
  if (currentRunMode == MODE_TIMED_STEP && motorTimedTargetMs > 0) {
    if (millis() - motorStartMs >= motorTimedTargetMs) {
      logEvent(String("[TIMED STEP] Hoan tat buoc thoi gian (") + String(motorTimedTargetMs) + "ms). DUNG MOTOR.");
      motorStop();
      currentRunMode = MODE_MANUAL;
      return;
    }
  }

  if (millis() - motorStartMs >= MAX_RUN_MS) {
    logEvent("[SAFE] CANH BAO: Motor chay qua thoi gian cho phep (MAX_RUN_MS). DUNG MOTOR.");
    motorStop();
    currentRunMode = MODE_MANUAL;
  }
}
