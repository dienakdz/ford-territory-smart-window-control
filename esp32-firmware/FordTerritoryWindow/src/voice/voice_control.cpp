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

static I2SClass mic;
static const size_t BLOCK_SAMPLES = 256;
static int32_t rawSamples[BLOCK_SAMPLES];

void voiceInit() {
  mic.setPins(PIN_I2S_BCLK, PIN_I2S_WS, -1, PIN_I2S_DIN);
  if (!mic.begin(I2S_MODE_STD, VOICE_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
                 I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    logEvent("[VOICE] LOI: Khoi tao I2S micro INMP441 that bai!");
  } else {
    logEvent("[VOICE] Micro INMP441 san sang (GPIO14/15/16 | 16kHz).");
  }
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

  http.setConnectTimeout(3000);
  http.setTimeout(4000);
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

bool recordAndRecognize() {
  if (isVoiceRecording) {
    return false;
  }
  isVoiceRecording = true;

  int16_t *pcm = nullptr;
  if (psramFound()) {
    pcm = static_cast<int16_t *>(ps_malloc(VOICE_PCM_BYTES));
  }
  if (pcm == nullptr) {
    pcm = static_cast<int16_t *>(malloc(VOICE_PCM_BYTES));
  }
  if (pcm == nullptr) {
    logEvent("[VOICE] LOI: Khong du RAM de cap phat bo dem am thanh 96KB.");
    isVoiceRecording = false;
    return false;
  }

  // Xoa bo dem doc cu cua I2S
  mic.readBytes(reinterpret_cast<char *>(rawSamples), sizeof(rawSamples));

  size_t recorded = 0;
  int emptyReads = 0;
  int peak = 0;
  uint64_t sumSquares = 0;

  logEvent("[VOICE] BAT DAU THU AM (2s) - HAY NOI (nang kinh / ha kinh / dung kinh)...");

  while (recorded < VOICE_SAMPLE_COUNT) {
    size_t want = min(BLOCK_SAMPLES, VOICE_SAMPLE_COUNT - recorded);
    size_t gotBytes = mic.readBytes(
        reinterpret_cast<char *>(rawSamples), want * sizeof(int32_t));
    size_t gotSamples = gotBytes / sizeof(int32_t);

    if (gotSamples == 0) {
      emptyReads++;
      if (emptyReads >= 10) {
        logEvent("[VOICE] LOI: Micro khong tra ve du lieu am thanh.");
        free(pcm);
        isVoiceRecording = false;
        return false;
      }
      delay(1);
      continue;
    }
    emptyReads = 0;

    for (size_t i = 0; i < gotSamples && recorded < VOICE_SAMPLE_COUNT; i++) {
      int32_t v = (rawSamples[i] >> 16) * VOICE_MIC_GAIN;
      if (v > 32767) v = 32767;
      if (v < -32768) v = -32768;
      pcm[recorded++] = static_cast<int16_t>(v);
      int mag = v >= 0 ? v : -v;
      if (mag > peak) peak = mag;
      sumSquares += static_cast<uint64_t>(v * v);
    }
  }

  double rms = sqrt(static_cast<double>(sumSquares) / VOICE_SAMPLE_COUNT);
  logEvent(String("[VOICE] Thu am xong 2s | Peak=") + String(peak) + " | RMS=" + String(rms, 1));

  if (peak < 50) {
    logEvent("[VOICE] Canh bao: Am thanh qua nho hoac micro bi ngat.");
  }

  if (WiFi.status() != WL_CONNECTED) {
    logEvent("[VOICE] LOI: Mat ket noi Wi-Fi, khong the gui am thanh len server.");
    free(pcm);
    isVoiceRecording = false;
    return false;
  }

  logEvent(String("[VOICE] Dang gui sang Flask Server xu ly Google STT..."));
  WiFiClient client;
  HTTPClient http;
  String url = String(FLASK_SERVER_BASE) + "/recognize";

  if (!http.begin(client, url)) {
    logEvent("[VOICE] LOI: Ket noi HTTP toi Flask server that bai.");
    free(pcm);
    isVoiceRecording = false;
    return false;
  }

  http.setConnectTimeout(4000);
  http.setTimeout(15000); // Cho Google Speech API xu ly
  http.addHeader("Content-Type", "application/octet-stream");

  int code = http.POST(reinterpret_cast<uint8_t *>(pcm), VOICE_PCM_BYTES);
  String command = "UNKNOWN";

  if (code == 200) {
    String resp = http.getString();
    resp.trim();
    int sepIdx = resp.indexOf(':');
    String heardText = "";
    if (sepIdx >= 0) {
      command = resp.substring(0, sepIdx);
      heardText = resp.substring(sepIdx + 1);
      logEvent(String("[VOICE] Google nghe duoc: \"") + heardText + "\" -> LENH: " + command);
    } else {
      command = resp;
      logEvent(String("[VOICE] KET QUA NHAN DANG: [ ") + command + " ]");
    }
  } else {
    logEvent(String("[VOICE] HTTP Loi ") + String(code) + " khi gui toi Flask server.");
    lastVoiceResult = "HTTP_ERROR";
  }

  http.end();
  free(pcm);
  isVoiceRecording = false;

  // Thuc thi 3 che do giong noi: AUTO UP, AUTO DOWN, STOP (An toan tuyet doi)
  if (command == "AUTO_UP" || command == "UP") {
    lastVoiceResult = "AUTO UP";
    logEvent("[VOICE -> MOTOR] Kich hoat: AUTO UP (Tu dong nang kinh + Chong ket)");
    if (!baselineValid) {
      logEvent("[SAFE] AUTO UP bi khoa: Chua co baseline hop le. Hay CAL truoc.");
      motorStop();
      currentRunMode = MODE_MANUAL;
      return false;
    } else {
      return motorRun(true, MODE_AUTO);
    }
  } else if (command == "AUTO_DOWN" || command == "DOWN") {
    lastVoiceResult = "AUTO DOWN";
    logEvent("[VOICE -> MOTOR] Kich hoat: AUTO DOWN (Tu dong ha kinh toi day)");
    return motorRun(false, MODE_AUTO);
  } else if (command == "STOP") {
    lastVoiceResult = "STOP";
    logEvent("[VOICE -> MOTOR] Kich hoat: STOP (Dung motor tai vi tri hien tai)");
    motorStop();
    currentRunMode = MODE_MANUAL;
    return true;
  } else {
    lastVoiceResult = "UNKNOWN";
    logEvent("[VOICE] Lenh khong hop le (UNKNOWN). Giu nguyen trang thai motor de dam bao an toan.");
    return false;
  }
}
