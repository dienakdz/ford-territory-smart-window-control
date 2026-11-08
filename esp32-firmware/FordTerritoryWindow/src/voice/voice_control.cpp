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
static int16_t *voicePcmBuffer = nullptr;

// Prototype ham loop tren Core 0
static void voiceTaskLoop(void *pvParameters);

void voiceInit() {
  mic.setPins(PIN_I2S_BCLK, PIN_I2S_WS, -1, PIN_I2S_DIN);
  if (!mic.begin(I2S_MODE_STD, VOICE_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
                 I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    logEvent("[VOICE] LOI: Khoi tao I2S micro INMP441 that bai!");
    return;
  }
  logEvent("[VOICE] Micro INMP441 san sang (GPIO14/15/16 | 16kHz | 2s | DC-Blocker).");

  // Cap phat bo dem co dinh 64KB (2 giay)
  if (psramFound()) {
    voicePcmBuffer = static_cast<int16_t *>(ps_malloc(VOICE_PCM_BYTES));
  }
  if (voicePcmBuffer == nullptr) {
    voicePcmBuffer = static_cast<int16_t *>(malloc(VOICE_PCM_BYTES));
  }
  if (voicePcmBuffer == nullptr) {
    logEvent("[VOICE] LOI: Khong the cap phat bo dem 64KB cho am thanh!");
    return;
  }

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
  logEvent("[VOICE] Bat dau thu am 2 giay... Hay noi: \"nang kinh\" / \"ha kinh\" / \"dung lai\"...");
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

  http.setConnectTimeout(2500);
  http.setTimeout(3000);
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
    // KIỂM TRA ĐIỀU KIỆN THU ÂM:
    // 1. isUserTriggered: Người dùng bấm nút trên Web Dashboard hoặc gửi Serial 'R'
    //    -> Chỉ thu âm ĐÚNG 1 LẦN (2s), không lặp lại!
    // 2. isMotorMoving: Kính đang thực sự chạy (motorState != STOPPED)
    //    -> Thu âm để rình bắt lệnh ngắt STOP/ĐẢO CHIỀU. Khi kính dừng -> DỪNG NGAY!
    // -------------------------------------------------------------------------
    bool isUserTriggered = false;
    if (voiceTriggerRequested) {
      voiceTriggerRequested = false;
      isUserTriggered = true;
    }

    bool isMotorMoving = (motorState != STOPPED);

    // Neu khong ai an nut VA kinh khong chay: Nghi 50ms, tuyet doi khong thu am
    if (!isUserTriggered && !isMotorMoving) {
      isVoiceRecording = false;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    // Neu chua ket noi Wi-Fi hoac khong co bo dem RAM
    if (WiFi.status() != WL_CONNECTED || voicePcmBuffer == nullptr) {
      isVoiceRecording = false;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    // --- TIẾN HÀNH THU ÂM 2 GIÂY ---
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

        voicePcmBuffer[recorded++] = static_cast<int16_t>(val);

        int mag = val >= 0 ? val : -val;
        if (mag > peak) peak = mag;
        sumSquares += static_cast<uint64_t>(val * val);
      }
    }

    // [BỘ LỌC VAD CHỐNG SPAM]:
    // Khi đang rình bắt lệnh ngắt lúc motor chạy:
    // Nếu peak < VOICE_MIN_SPEECH_PEAK: Chi la tieng on co khi / tieng gio / im lang
    // -> Bo qua, KHONG gui sang Google de tranh nghien mang va spam "(Khong ro tu ngu)"!
    if (!isUserTriggered && peak < VOICE_MIN_SPEECH_PEAK) {
      isVoiceRecording = false;
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    // --- GỬI SANG FLASK SERVER (LAPTOP) QUA HTTP POST ---
    WiFiClient client;
    HTTPClient http;
    String url = String(FLASK_SERVER_BASE) + "/recognize";

    if (http.begin(client, url)) {
      http.setConnectTimeout(2500);
      http.setTimeout(8000);
      http.addHeader("Content-Type", "application/octet-stream");

      int code = http.POST(reinterpret_cast<uint8_t *>(voicePcmBuffer), recorded * sizeof(int16_t));
      String command = "UNKNOWN";

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
        // [BẢO VỆ TOÀN DIỆN]: Nếu không kết nối được Flask Server (chưa bật server hoặc mất mạng),
        // tắt ngay trạng thái lắng nghe để Core 0 không bị kẹt trong các chu kỳ timeout 2.5s liên tục,
        // giúp giữ băng thông Wi-Fi và đảm bảo nút Calibration (CAL) trên Web Dashboard phản hồi tức thì!
        voiceActiveUntilMs = 0;
        voiceTriggerRequested = false;
        lastVoiceResult = "SERVER_ERROR";
        logEvent(String("[VOICE] Loi ket noi Flask Server (Code ") + String(code) + "). Huy lang nghe de bao ve Web/Calibration.");
      }

      http.end();

      // --- PHÂN LOẠI VÀ THỰC THI LỆNH GIỌNG NÓI ---
      if (command == "STOP") {
        // [KHẨN CẤP]: Cắt điện motor ngay lập tức trên Core 0 (0ms latency)!
        motorStop();
        lastVoiceResult = "STOP";
        voicePendingReq = VOICE_REQ_STOP;
        logEvent("[VOICE INTERRUPT] DUNG KINH THANH CONG BANG GIONG NOI!");
      } else if (command == "AUTO_DOWN" || command == "DOWN") {
        lastVoiceResult = "AUTO DOWN";
        voicePendingReq = VOICE_REQ_AUTO_DOWN;
      } else if (command == "AUTO_UP" || command == "UP") {
        lastVoiceResult = "AUTO UP";
        voicePendingReq = VOICE_REQ_AUTO_UP;
      }
    } else {
      voiceActiveUntilMs = 0;
      voiceTriggerRequested = false;
      logEvent("[VOICE] Khong the ket noi Flask Server. Da huy lang nghe.");
    }

    isVoiceRecording = false;

    // Nghỉ nhẹ 20ms
    vTaskDelay(pdMS_TO_TICKS(20));
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

  if (req == VOICE_REQ_STOP) {
    motorStop();
    currentRunMode = MODE_MANUAL;
  } else if (req == VOICE_REQ_AUTO_UP) {
    if (!baselineValid) {
      logEvent("[SAFE] AUTO UP bi khoa: Chua co baseline hop le. Hay CAL truoc.");
      motorStop();
      currentRunMode = MODE_MANUAL;
    } else {
      motorRun(true, MODE_AUTO);
      logEvent("[VOICE -> MOTOR] AUTO UP kich hoat thanh cong!");
    }
  } else if (req == VOICE_REQ_AUTO_DOWN) {
    motorRun(false, MODE_AUTO);
    logEvent("[VOICE -> MOTOR] AUTO DOWN kich hoat thanh cong!");
  }
}
