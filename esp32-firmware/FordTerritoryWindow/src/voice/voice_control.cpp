#include "voice_control.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP_I2S.h>
#include <math.h>
#include "../logger/logger.h"
#include "../motor/motor_control.h"
#include "../storage/nvs_storage.h"

String lastVoiceResult = "CHƯA CÓ LỆNH";
bool isVoiceRecording = false;

volatile VoiceCommandReq voicePendingReq = VOICE_REQ_NONE;
volatile unsigned long voiceActiveUntilMs = 0;
static volatile bool voiceTriggerRequested = false;

static TaskHandle_t voiceTaskHandle = NULL;
static I2SClass mic;
static const size_t BLOCK_SAMPLES = 256;
static int32_t rawSamples[BLOCK_SAMPLES];

// Prototype ham loop tren Core 0
static void voiceTaskLoop(void *pvParameters);

void voiceInit() {
  pinMode(VOICE_BUTTON_PIN, INPUT_PULLUP);
  mic.setPins(PIN_I2S_BCLK, PIN_I2S_WS, -1, PIN_I2S_DIN);
  if (!mic.begin(I2S_MODE_STD, VOICE_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
                 I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    logEvent("[VOICE] LOI: Khoi tao I2S micro INMP441 that bai!");
    return;
  }

  String psramStatus = psramFound() ? "BAT (8MB PSRAM OPI)" : "TAT (Dung On-Demand SRAM)";
  logEvent(String("[VOICE] Micro INMP441 san sang (16kHz | 3s | PSRAM: ") + psramStatus + ").");
  logEvent(String("[SYSTEM] Free Heap khoi dong: ") + String(ESP.getFreeHeap()) + " bytes");

  // Khoi tao Task chay doc lap tren Core 0 (Bộ não thứ 2)
  xTaskCreatePinnedToCore(
      voiceTaskLoop,
      "VoiceTaskCore0",
      10240,            // 10KB stack cho HTTPClient & I2S
      NULL,
      1,                // Do uu tien 1
      &voiceTaskHandle,
      0                 // Ghim co dinh vao CORE 0!
  );
}

void triggerVoiceListen() {
  voiceTriggerRequested = true;
  logEvent("[VOICE] Bat dau thu am 3 giay... Hay noi: het, 1/2, 1/3 hoac 2/3 kinh...");
}

bool recordAndRecognize() {
  triggerVoiceListen();
  return true;
}

bool testVoiceServerPing() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  WiFiClient client;
  HTTPClient http;
  String url = String(FLASK_SERVER_BASE) + "/ping";
  if (!http.begin(client, url)) {
    return false;
  }

  http.setConnectTimeout(800);
  http.setTimeout(1200);
  int code = http.GET();
  bool success = false;

  if (code > 0) {
    String body = http.getString();
    body.trim();
    if (code == 200 && body == "PONG") {
      logEvent(String("[VOICE] Da ket noi Flask Server tren Laptop! (") + FLASK_SERVER_BASE + ")");
      success = true;
    }
  } else {
    logEvent(String("[VOICE] Chua thay Flask Server tai ") + FLASK_SERVER_BASE + ". Hay bat server_voice.py");
  }

  http.end();
  client.stop();
  return success;
}

// ==============================================================================
// VÒNG LẶP CHÍNH CỦA BỘ NÃO THỨ 2 (CORE 0 - DUAL CORE TASK)
// ==============================================================================
static void voiceTaskLoop(void *pvParameters) {
  logEvent("[DUAL-CORE] Bo nao lang nghe Core 0 da khoi dong thanh cong!");

  // Bien trang thai bo loc DC-Blocker IIR (fc ~ 35Hz)
  float dcPrevX = 0.0f;
  float dcPrevY = 0.0f;
  const float dcAlpha = 0.985f;

  while (true) {
    // -------------------------------------------------------------------------
    // KIỂM TRA ĐIỀU KIỆN THU ÂM (CHỈ THU ÂM KHI CLICK BẤM NÚT):
    // Chỉ kích hoạt khi người dùng nhấn nút vật lý PTT (GPIO2) hoặc click trên Web Dashboard.
    // -------------------------------------------------------------------------
    bool isUserTriggered = false;
    if (voiceTriggerRequested) {
      voiceTriggerRequested = false;
      isUserTriggered = true;
    }

    // Nếu không ai bấm nút kích hoạt: Nghỉ 50ms để nhường CPU
    if (!isUserTriggered) {
      isVoiceRecording = false;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    // Nếu chưa kết nối Wi-Fi: Báo nhắc người dùng
    if (WiFi.status() != WL_CONNECTED) {
      logEvent("[VOICE] Chua the thu am: Wi-Fi chua ket noi voi iPhone Hotspot!");
      isVoiceRecording = false;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    // --- CẤP PHÁT BỘ ĐỆM ĐỘNG THEO YÊU CẦU (ON-DEMAND BUFFER 96KB) ---
    // Chỉ cấp phát RAM khi bấm nút nói. Thu âm & gửi xong sẽ giải phóng ngay!
    // Giúp hệ thống luôn dư dả 150KB - 200KB SRAM nội bộ, Wi-Fi và Web Server không bao giờ đơ/crash.
    int16_t *pcmBuffer = nullptr;
    if (psramFound()) {
      pcmBuffer = static_cast<int16_t *>(ps_malloc(VOICE_PCM_BYTES));
    }
    if (pcmBuffer == nullptr) {
      pcmBuffer = static_cast<int16_t *>(malloc(VOICE_PCM_BYTES));
    }

    if (pcmBuffer == nullptr) {
      logEvent(String("[VOICE] LOI: Khong du RAM thu am! Free Heap hien tai: ") + String(ESP.getFreeHeap()) + " bytes");
      isVoiceRecording = false;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    // --- TIẾN HÀNH THU ÂM 3 GIÂY ---
    isVoiceRecording = true;

    // Xoa sach du lieu cu trong bo dem I2S
    mic.readBytes(reinterpret_cast<char *>(rawSamples), sizeof(rawSamples));

    size_t recorded = 0;
    int emptyReads = 0;
    int peak = 0;
    uint64_t sumSquares = 0;

    while (recorded < VOICE_SAMPLE_COUNT) {
      size_t want = min(BLOCK_SAMPLES, VOICE_SAMPLE_COUNT - recorded);
      size_t gotBytes = mic.readBytes(
          reinterpret_cast<char *>(rawSamples), want * sizeof(int32_t));
      size_t gotSamples = gotBytes / sizeof(int32_t);

      if (gotSamples == 0) {
        emptyReads++;
        if (emptyReads >= 10) {
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        continue;
      }
      emptyReads = 0;

      for (size_t i = 0; i < gotSamples && recorded < VOICE_SAMPLE_COUNT; i++) {
        // Lay 16-bit cao va nhan do khuech dai
        float raw = static_cast<float>((rawSamples[i] >> 16) * VOICE_MIC_GAIN);

        // BỘ LỌC DC BLOCKER: Triet tieu hoan toan troi DC va tieng un 0 - 20Hz
        float filtered = raw - dcPrevX + dcAlpha * dcPrevY;
        dcPrevX = raw;
        dcPrevY = filtered;

        // Bo qua 200 mau dau tien luc khoi dong bo loc
        int32_t val = (recorded < 200) ? 0 : static_cast<int32_t>(filtered);
        if (val > 32767) val = 32767;
        if (val < -32768) val = -32768;

        pcmBuffer[recorded++] = static_cast<int16_t>(val);

        int mag = val >= 0 ? val : -val;
        if (mag > peak) peak = mag;
        sumSquares += static_cast<uint64_t>(val * val);
      }
    }

    // Kiểm tra ngưỡng biên độ giọng nói (VAD) khi bấm nút
    if (peak < VOICE_MIN_SPEECH_PEAK) {
      logEvent("[VOICE] Am thanh qua nho hoac khong co tieng noi.");
      if (pcmBuffer != nullptr) {
        free(pcmBuffer);
        pcmBuffer = nullptr;
      }
      isVoiceRecording = false;
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    // --- GỬI SANG FLASK SERVER (LAPTOP) QUA HTTP POST ---
    WiFiClient client;
    HTTPClient http;
    String url = String(FLASK_SERVER_BASE) + "/recognize";
    String command = "UNKNOWN";

    if (http.begin(client, url)) {
      http.setConnectTimeout(2500);
      http.setTimeout(8000);
      http.addHeader("Content-Type", "application/octet-stream");

      int code = http.POST(reinterpret_cast<uint8_t *>(pcmBuffer), recorded * sizeof(int16_t));

      if (code == 200) {
        String resp = http.getString();
        resp.trim();
        int sepIdx = resp.indexOf(':');
        String heardText = "";
        if (sepIdx >= 0) {
          command = resp.substring(0, sepIdx);
          heardText = resp.substring(sepIdx + 1);
          logEvent(String("[VOICE] Google nghe: \"") + heardText + "\" -> LENH: " + command);
        } else {
          command = resp;
          logEvent(String("[VOICE] LENH: [ ") + command + " ]");
        }
      } else {
        voiceActiveUntilMs = 0;
        voiceTriggerRequested = false;
        isVoiceDriven = false;
        lastVoiceResult = "SERVER_ERROR";
        logEvent(String("[VOICE] Loi ket noi Flask Server (Code ") + String(code) + ").");
      }

      http.end();
      client.stop();
    } else {
      voiceActiveUntilMs = 0;
      voiceTriggerRequested = false;
      isVoiceDriven = false;
      logEvent("[VOICE] Khong the khoi tao HTTP POST sang Flask Server.");
    }

    // --- GIẢI PHÓNG BỘ ĐỆM 96KB NGAY LẬP TỨC ĐỂ TRẢ RAM CHO HỆ THỐNG ---
    if (pcmBuffer != nullptr) {
      free(pcmBuffer);
      pcmBuffer = nullptr;
    }

    // --- PHÂN LOẠI VÀ THỰC THI LỆNH GIỌNG NÓI ---
    if (command == "AUTO_DOWN" || command == "DOWN") {
      lastVoiceResult = "HẠ HẾT";
      voicePendingReq = VOICE_REQ_AUTO_DOWN;
    } else if (command == "AUTO_UP" || command == "UP") {
      lastVoiceResult = "NÂNG HẾT";
      voicePendingReq = VOICE_REQ_AUTO_UP;
    } else if (command == "UP_HALF") {
      lastVoiceResult = "NÂNG 1/2";
      voicePendingReq = VOICE_REQ_UP_HALF;
    } else if (command == "UP_THIRD") {
      lastVoiceResult = "NÂNG 1/3";
      voicePendingReq = VOICE_REQ_UP_THIRD;
    } else if (command == "UP_TWO_THIRDS") {
      lastVoiceResult = "NÂNG 2/3";
      voicePendingReq = VOICE_REQ_UP_TWO_THIRDS;
    } else if (command == "DOWN_HALF") {
      lastVoiceResult = "HẠ 1/2";
      voicePendingReq = VOICE_REQ_DOWN_HALF;
    } else if (command == "DOWN_THIRD") {
      lastVoiceResult = "HẠ 1/3";
      voicePendingReq = VOICE_REQ_DOWN_THIRD;
    } else if (command == "DOWN_TWO_THIRDS") {
      lastVoiceResult = "HẠ 2/3";
      voicePendingReq = VOICE_REQ_DOWN_TWO_THIRDS;
    } else if (command != "UNKNOWN") {
      lastVoiceResult = command;
    }

    isVoiceRecording = false;

    // Nghỉ nhẹ 20ms
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ==============================================================================
// GIÁM SÁT NÚT BẤM VẬT LÝ VOICE PTT (GPIO2) TRONG LOOP
// ==============================================================================
void monitorVoiceButton() {
  static int lastReading = HIGH;
  static int stableState = HIGH;
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50;

  int reading = digitalRead(VOICE_BUTTON_PIN);

  if (reading != lastReading) {
    lastDebounceTime = millis();
    lastReading = reading;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != stableState) {
      stableState = reading;
      // Khi nhấn nút: chuyển từ HIGH (kéo lên) -> LOW (nối GND)
      if (stableState == LOW) {
        if (!isVoiceRecording) {
          logEvent("[VOICE PTT] Nút nhấn vật lý (GPIO2) kích hoạt -> Bắt đầu thu âm giọng nói!");
          triggerVoiceListen();
        } else {
          logEvent("[VOICE PTT] Đang trong phiên thu âm, bỏ qua lượt nhấn lặp lại.");
        }
      }
    }
  }
}

// ==============================================================================
// HÀM KIỂM TRA VÀ THỰC THI LỆNH TRÊN CORE 1 (Gọi trong loop)
// ==============================================================================
void checkVoicePendingReq() {
  if (voicePendingReq == VOICE_REQ_NONE) {
    return;
  }

  VoiceCommandReq req = voicePendingReq;
  voicePendingReq = VOICE_REQ_NONE;

  // Tinh toan thoi gian buoc nhay thuc te tu NVS Flash
  unsigned long timeUp = (travelTimeUpMs >= 1500 && travelTimeUpMs <= 12000) ? travelTimeUpMs : FULL_TRAVEL_TIME_UP_MS;
  unsigned long timeDown = (travelTimeDownMs >= 1500 && travelTimeDownMs <= 12000) ? travelTimeDownMs : FULL_TRAVEL_TIME_DOWN_MS;

  unsigned long stepUpHalf = timeUp / 2;
  unsigned long stepUpThird = timeUp / 3;
  unsigned long stepUpTwoThirds = (timeUp * 2) / 3;

  unsigned long stepDownHalf = timeDown / 2;
  unsigned long stepDownThird = timeDown / 3;
  unsigned long stepDownTwoThirds = (timeDown * 2) / 3;

  if (req == VOICE_REQ_AUTO_UP) {
    if (!baselineValid) {
      logEvent("[SAFE] AUTO UP bi khoa: Chua co baseline hop le. Hay CAL truoc.");
      motorStop();
      currentRunMode = MODE_MANUAL;
    } else {
      motorRun(true, MODE_AUTO);
      logEvent("[VOICE -> MOTOR] AUTO UP (Nang het kinh) kich hoat!");
    }
  } else if (req == VOICE_REQ_AUTO_DOWN) {
    motorRun(false, MODE_AUTO);
    logEvent("[VOICE -> MOTOR] AUTO DOWN (Ha het kinh) kich hoat!");
  } else if (req == VOICE_REQ_UP_HALF) {
    if (!baselineValid) {
      logEvent("[SAFE] NANG 1/2 bi khoa: Chua co baseline hop le. Hay CAL truoc.");
      motorStop();
      currentRunMode = MODE_MANUAL;
    } else {
      motorRunTimed(true, stepUpHalf);
      logEvent(String("[VOICE -> MOTOR] NANG 1/2 KINH (") + String(stepUpHalf) + "ms) kich hoat!");
    }
  } else if (req == VOICE_REQ_UP_THIRD) {
    if (!baselineValid) {
      logEvent("[SAFE] NANG 1/3 bi khoa: Chua co baseline hop le. Hay CAL truoc.");
      motorStop();
      currentRunMode = MODE_MANUAL;
    } else {
      motorRunTimed(true, stepUpThird);
      logEvent(String("[VOICE -> MOTOR] NANG 1/3 KINH (") + String(stepUpThird) + "ms) kich hoat!");
    }
  } else if (req == VOICE_REQ_UP_TWO_THIRDS) {
    if (!baselineValid) {
      logEvent("[SAFE] NANG 2/3 bi khoa: Chua co baseline hop le. Hay CAL truoc.");
      motorStop();
      currentRunMode = MODE_MANUAL;
    } else {
      motorRunTimed(true, stepUpTwoThirds);
      logEvent(String("[VOICE -> MOTOR] NANG 2/3 KINH (") + String(stepUpTwoThirds) + "ms) kich hoat!");
    }
  } else if (req == VOICE_REQ_DOWN_HALF) {
    motorRunTimed(false, stepDownHalf);
    logEvent(String("[VOICE -> MOTOR] HA 1/2 KINH (") + String(stepDownHalf) + "ms) kich hoat!");
  } else if (req == VOICE_REQ_DOWN_THIRD) {
    motorRunTimed(false, stepDownThird);
    logEvent(String("[VOICE -> MOTOR] HA 1/3 KINH (") + String(stepDownThird) + "ms) kich hoat!");
  } else if (req == VOICE_REQ_DOWN_TWO_THIRDS) {
    motorRunTimed(false, stepDownTwoThirds);
    logEvent(String("[VOICE -> MOTOR] HA 2/3 KINH (") + String(stepDownTwoThirds) + "ms) kich hoat!");
  }
}
