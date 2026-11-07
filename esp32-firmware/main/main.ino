
#include <Arduino.h>

// =====================================================
// 1. KHAI BAO CHAN ESP32-S3
// =====================================================

// Driver BTS7960
#define RPWM_PIN 4
#define LPWM_PIN 5
#define REN_PIN  6
#define LEN_PIN  7

// Cong tac hanh trinh
#define LIMIT_TOP    8
#define LIMIT_BOTTOM 9

// Cong tac kinh 4 che do
#define SWITCH_ADC_PIN 1

// ACS712 OUT sau mach chia ap
// Doi GPIO nay neu day thuc te cua ban noi chan khac
#define ACS_ADC_PIN 10


// =====================================================
// 2. CAU HINH
// =====================================================

// Chong doi cong tac
const unsigned long SWITCH_DEBOUNCE_MS = 40;

// Gioi han chay motor toi da
const unsigned long MAX_RUN_MS = 8000;

// Thoi gian ha kinh khi phat hien ket
const unsigned long ANTI_PINCH_REVERSE_MS = 1000;

// Chong ket ACS712
// Giu nguong thu nghiem cua ban
const int ACS_STALL_THRESHOLD_MV = 1705;

// Bo qua dong khoi dong motor
const unsigned long ACS_IGNORE_START_MS = 350;

// Thoi gian xac nhan dien ap bat thuong
const unsigned long ACS_STALL_CONFIRM_MS = 150;

// Chu ky in thong so
const unsigned long DEBUG_INTERVAL_MS = 250;


// =====================================================
// 3. CAC TRANG THAI
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

  // Trang thai dac biet:
  // motor dang ha kinh sau khi phat hien ket
  MODE_ANTI_PINCH_REVERSE
};

MotorState motorState = STOPPED;
RunMode currentRunMode = MODE_MANUAL;

SwitchMode stableSwitchMode = SW_UNKNOWN;
SwitchMode candidateSwitchMode = SW_UNKNOWN;

unsigned long candidateSince = 0;
unsigned long motorStartMs = 0;

int switchVoltageMv = 0;

// Bien theo doi ACS712
bool acsHighDetected = false;
unsigned long acsHighSince = 0;

unsigned long lastDebugMs = 0;


// =====================================================
// 4. CONG TAC HANH TRINH
// =====================================================

// Gia dinh:
// Cong tac NC duoc dau de binh thuong = LOW,
// khi cong tac mo tai hanh trinh = HIGH.
// Neu cach dau thuc te khac, can doi logic hai ham nay.

bool topReached() {
  return digitalRead(LIMIT_TOP) == HIGH;
}

bool bottomReached() {
  return digitalRead(LIMIT_BOTTOM) == HIGH;
}


// =====================================================
// 5. DUNG MOTOR
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
// 6. DIEU KHIEN MOTOR NANG / HA
// =====================================================

void motorRun(bool up, RunMode requestedMode) {
  // Dung truoc khi thay doi chieu chay
  motorStop();
  delay(150);

  // Bao ve neu ca hai hanh trinh cung kich hoat
  if (topReached() && bottomReached()) {
    Serial.println(
      "LOI: Ca hai cong tac hanh trinh cung kich hoat!"
    );

    currentRunMode = MODE_MANUAL;
    return;
  }

  // Khong cho nang neu da cham gioi han tren
  if (up && topReached()) {
    Serial.println("Da cham hanh trinh tren.");

    currentRunMode = MODE_MANUAL;
    return;
  }

  // Khong cho ha neu da cham gioi han duoi
  if (!up && bottomReached()) {
    Serial.println("Da cham hanh trinh duoi.");

    currentRunMode = MODE_MANUAL;
    return;
  }

  // Chon chieu quay motor
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
    Serial.println("MOTOR: NANG KINH");
  } else {
    motorState = MOVING_DOWN;

    if (requestedMode == MODE_ANTI_PINCH_REVERSE) {
      Serial.println(
        "CHONG KET: HA KINH TRONG TOI DA 1 GIAY."
      );
    } else {
      Serial.println("MOTOR: HA KINH");
    }
  }

  currentRunMode = requestedMode;
  motorStartMs = millis();

  // Reset bo dem ACS712
  acsHighDetected = false;
  acsHighSince = 0;
}


// =====================================================
// 7. DOC CONG TAC KINH 4 CHE DO
// =====================================================

SwitchMode readSwitchMode() {
  switchVoltageMv = analogReadMilliVolts(SWITCH_ADC_PIN);

  // Tha cong tac
  if (switchVoltageMv >= 2900) {
    return SW_RELEASED;
  }

  // UP thu cong
  if (switchVoltageMv >= 2300 &&
      switchVoltageMv <= 2750) {
    return SW_UP;
  }

  // AUTO UP
  if (switchVoltageMv >= 1450 &&
      switchVoltageMv <= 2000) {
    return SW_AUTO_UP;
  }

  // DOWN thu cong
  if (switchVoltageMv >= 600 &&
      switchVoltageMv <= 1100) {
    return SW_DOWN;
  }

  // AUTO DOWN
  if (switchVoltageMv <= 250) {
    return SW_AUTO_DOWN;
  }

  return SW_UNKNOWN;
}


// =====================================================
// 8. XU LY CONG TAC 4 CHE DO
// =====================================================

void handleSwitchModeChange(SwitchMode mode) {
  switch (mode) {
    case SW_UP:
      // Nhan va giu de nang kinh
      motorRun(true, MODE_MANUAL);
      break;

    case SW_AUTO_UP:
      // Nhan mot lan de nang tu dong
      motorRun(true, MODE_AUTO);
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
      //
      // Khi dang ha kinh do chong ket, khong dung
      // som chi vi cong tac AUTO UP duoc tha.
      if (currentRunMode == MODE_MANUAL) {
        motorStop();
      }
      break;

    case SW_UNKNOWN:
    default:
      Serial.println(
        "CANH BAO: Dien ap cong tac khong hop le."
      );

      motorStop();
      currentRunMode = MODE_MANUAL;
      break;
  }
}


// =====================================================
// 9. GIAM SAT CONG TAC HANH TRINH
// =====================================================

void monitorLimits() {
  if (motorState == STOPPED) {
    return;
  }

  if (topReached() && bottomReached()) {
    Serial.println(
      "LOI: Hai cong tac hanh trinh cung kich hoat."
    );

    motorStop();
    currentRunMode = MODE_MANUAL;
    return;
  }

  // Dang nang ma cham hanh trinh tren
  if (motorState == MOVING_UP && topReached()) {
    Serial.println(
      "HANH TRINH TREN: DUNG MOTOR."
    );

    motorStop();
    currentRunMode = MODE_MANUAL;
    return;
  }

  // Dang ha ma cham hanh trinh duoi
  if (motorState == MOVING_DOWN && bottomReached()) {
    Serial.println(
      "HANH TRINH DUOI: DUNG MOTOR."
    );

    motorStop();
    currentRunMode = MODE_MANUAL;
    return;
  }
}


// =====================================================
// 10. GIOI HAN THOI GIAN CHAY MOTOR
// =====================================================

void monitorRunTimeout() {
  if (motorState == STOPPED) {
    return;
  }

  if (millis() - motorStartMs >= MAX_RUN_MS) {
    Serial.println(
      "CANH BAO: Motor chay qua thoi gian cho phep."
    );

    motorStop();
    currentRunMode = MODE_MANUAL;
  }
}


// =====================================================
// 11. DOC DIEN AP ACS712
// =====================================================

int readAcsVoltageMv() {
  // Trung binh 10 mau ADC
  long totalMv = 0;

  for (int i = 0; i < 10; i++) {
    totalMv += analogReadMilliVolts(ACS_ADC_PIN);
    delayMicroseconds(200);
  }

  return totalMv / 10;
}


// =====================================================
// 12. CHONG KET CHI KHI AUTO UP
// =====================================================

void monitorAntiPinch() {
  // Chi kiem tra khi dang nang tu dong
  if (motorState != MOVING_UP ||
      currentRunMode != MODE_AUTO) {
    acsHighDetected = false;
    acsHighSince = 0;
    return;
  }

  unsigned long now = millis();

  // Bo qua dong khoi dong
  if (now - motorStartMs < ACS_IGNORE_START_MS) {
    acsHighDetected = false;
    acsHighSince = 0;
    return;
  }

  int acsMv = readAcsVoltageMv();

  // In dien ap de theo doi
  if (now - lastDebugMs >= DEBUG_INTERVAL_MS) {
    Serial.print("AUTO UP - ACS712: ");
    Serial.print(acsMv);
    Serial.println(" mV");

    lastDebugMs = now;
  }

  // Theo nguong ban dang thu nghiem:
  // Dien ap cao bat thuong lien tuc -> phat hien ket
  if (acsMv >= ACS_STALL_THRESHOLD_MV) {
    if (!acsHighDetected) {
      acsHighDetected = true;
      acsHighSince = now;
    }

    if (now - acsHighSince >= ACS_STALL_CONFIRM_MS) {
      Serial.println(
        "PHAT HIEN KET KHI AUTO UP!"
      );

      Serial.println(
        "Dung chieu nang va chuyen sang ha kinh."
      );

      // Ham nay dung motor nang truoc,
      // sau do chay motor theo chieu ha.
      motorRun(false, MODE_ANTI_PINCH_REVERSE);

      // Neu khong the ha vi da cham hanh trinh duoi
      // hoac co loi hanh trinh, giu motor dung.
      if (motorState != MOVING_DOWN ||
          currentRunMode != MODE_ANTI_PINCH_REVERSE) {
        motorStop();
        currentRunMode = MODE_MANUAL;

        Serial.println(
          "Khong the ha kinh; motor giu trang thai dung."
        );
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


// =====================================================
// 13. DIEU KHIEN DAO CHIEU HA KINH SAU KHI KET
// =====================================================

void monitorAntiPinchReverse() {
  // Chi ap dung cho giai doan ha kinh do chong ket
  if (currentRunMode != MODE_ANTI_PINCH_REVERSE ||
      motorState != MOVING_DOWN) {
    return;
  }

  // Dung sau toi da 1 giay ke tu luc bat dau ha
  if (millis() - motorStartMs >= ANTI_PINCH_REVERSE_MS) {
    motorStop();
    currentRunMode = MODE_MANUAL;

    Serial.println(
      "Da ha kinh 1 giay sau khi ket - DUNG MOTOR."
    );
    Serial.println(
      "Cho lenh tiep theo."
    );
  }
}


// =====================================================
// 14. SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(RPWM_PIN, OUTPUT);
  pinMode(LPWM_PIN, OUTPUT);
  pinMode(REN_PIN, OUTPUT);
  pinMode(LEN_PIN, OUTPUT);

  pinMode(LIMIT_TOP, INPUT_PULLUP);
  pinMode(LIMIT_BOTTOM, INPUT_PULLUP);

  pinMode(SWITCH_ADC_PIN, INPUT);
  pinMode(ACS_ADC_PIN, INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(SWITCH_ADC_PIN, ADC_11db);
  analogSetPinAttenuation(ACS_ADC_PIN, ADC_11db);

  // Dam bao motor dung khi khoi dong
  motorStop();

  Serial.println();
  Serial.println("====================================");
  Serial.println(" ESP32-S3 WINDOW CONTROL");
  Serial.println(" BTS7960 + 4-MODE SWITCH");
  Serial.println(" LIMIT SWITCH + ACS712");
  Serial.println(" AUTO UP ANTI-PINCH REVERSE");
  Serial.println("====================================");

  Serial.println("UP: Nhan va giu de nang.");
  Serial.println("AUTO UP: Nhan mot lan de nang.");
  Serial.println("DOWN: Nhan va giu de ha.");
  Serial.println("AUTO DOWN: Nhan mot lan de ha.");
  Serial.println("Ket khi AUTO UP: Ha kinh toi da 1 giay.");
  Serial.println("Khong tu dong dao chieu khi ket o che do khac.");
}


// =====================================================
// 15. LOOP
// =====================================================

void loop() {
  // Uu tien hanh trinh
  monitorLimits();

  // Gioi han thoi gian chay tong quat
  monitorRunTimeout();

  // Chong ket chi khi AUTO UP
  monitorAntiPinch();

  // Neu vua phat hien ket, ham nay se tiep tuc
  // giam sat thoi gian ha kinh trong cac vong loop sau
  monitorAntiPinchReverse();

  // Doc cong tac kinh
  SwitchMode reading = readSwitchMode();

  // Chong doi cong tac
  if (reading != candidateSwitchMode) {
    candidateSwitchMode = reading;
    candidateSince = millis();
  }

  if (millis() - candidateSince >= SWITCH_DEBOUNCE_MS) {
    if (stableSwitchMode != candidateSwitchMode) {
      stableSwitchMode = candidateSwitchMode;

      Serial.print("Cong tac ADC: ");
      Serial.print(switchVoltageMv);
      Serial.println(" mV");

      handleSwitchModeChange(stableSwitchMode);
    }
  }
}
