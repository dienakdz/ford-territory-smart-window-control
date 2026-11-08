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
  logEvent("[VOICE] Micro INMP441 san sang (GPIO14/15/16 | 16kHz | 2s).");

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
  voiceActiveUntilMs = millis() + VOICE_ACTIVE_WINDOW_MS;
  logEvent("[VOICE] Bat dau thu am 2 giay...\n[VOICE] Hay noi: \"nang kinh\" / \"ha kinh\" / \"dung lai\"...");
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

  http.setConnectTimeout(400);
  http.setTimeout(600);
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

  while (true) {
    unsigned long now = millis();
    bool shouldListen = false;
    bool explicitButton = false;

    // Kiem tra neu co yeu cau kich hoat tu Web / Serial
    if (voiceTriggerRequested) {
      voiceTriggerRequested = false;
      explicitButton = true;
      voiceActiveUntilMs = now + VOICE_ACTIVE_WINDOW_MS;
      shouldListen = true;
    }

    // Neu kinh dang chay (UP hoac DOWN): Luon tu dong lang nghe de bat lenh STOP / DAO CHIEU!
    if (motorState != STOPPED) {
      shouldListen = true;
    }

    // Neu con trong cua so 10 giay sau khi ra lenh
    if (now < voiceActiveUntilMs) {
      shouldListen = true;
    }

    // Neu khong can nghe, hoac chua co Wi-Fi / chua co RAM: tam nghi 50ms
    if (!shouldListen || WiFi.status() != WL_CONNECTED || voicePcmBuffer == nullptr) {
      isVoiceRecording = false;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    // --- TIEN HANH THU AM 2 GIAY DONG BO ---
    isVoiceRecording = true;

    // Xoa sach mau ton dong trong bo dem I2S
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
        int32_t v = (rawSamples[i] >> 16) * VOICE_MIC_GAIN;
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        voicePcmBuffer[recorded++] = static_cast<int16_t>(v);
        int mag = v >= 0 ? v : -v;
        if (mag > peak) peak = mag;
        sumSquares += static_cast<uint64_t>(v * v);
      }
    }

    // [BỘ LỌC VAD - VOICE ACTIVITY DETECTION]:
    // Nếu người dùng không ấn nút và âm thanh chỉ là tiếng ồn phòng (peak < 220),
    // bỏ qua và không gửi sang Google để tránh nghẽn mạng và spam "Không rõ từ ngữ"!
    if (!explicitButton && peak < VOICE_MIN_SPEECH_PEAK) {
      isVoiceRecording = false;
      vTaskDelay(pdMS_TO_TICKS(40));
      continue;
    }

    // --- GUI SANG FLASK SERVER (LAPTOP) QUA HTTP POST ---
    WiFiClient client;
    HTTPClient http;
    String url = String(FLASK_SERVER_BASE) + "/recognize";

    if (http.begin(client, url)) {
      http.setConnectTimeout(500);
      http.setTimeout(2500);
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
        // Neu khong ket noi duoc Flask Server, tat cua so lang nghe de khong treo Core 0
        voiceActiveUntilMs = 0;
      }

      http.end();

      // --- PHAN LOAI VA THUC THI LENH GIONG NOI ---
      if (command == "STOP") {
        // [KHAN CAP]: Dung motor ngay lap tuc tren Core 0 (0ms latency)!
        motorStop();
        lastVoiceResult = "STOP";
        voicePendingReq = VOICE_REQ_STOP;
        logEvent("[VOICE INTERRUPT] DUNG KINH THANH CONG BANG GIONG NOI!");
      } else if (command == "AUTO_DOWN" || command == "DOWN") {
        lastVoiceResult = "AUTO DOWN";
        voicePendingReq = VOICE_REQ_AUTO_DOWN;
        voiceActiveUntilMs = millis() + VOICE_ACTIVE_WINDOW_MS;
      } else if (command == "AUTO_UP" || command == "UP") {
        lastVoiceResult = "AUTO UP";
        voicePendingReq = VOICE_REQ_AUTO_UP;
        voiceActiveUntilMs = millis() + VOICE_ACTIVE_WINDOW_MS;
      }
    }

    isVoiceRecording = false;

    // Nghi nhe 20ms giua cac block
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ==============================================================================
// HAM KIEM TRA VA CAP NHAT TRANG THAI TREN CORE 1 (Goi trong loop)
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
