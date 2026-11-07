/* Ford Territory - ESP32-S3 N16R8 / INMP441 / Flask LAN test
 * Arduino-ESP32 core 3.x (ESP_I2S.h)
 * No motor pins are configured. P => ping. R => record and recognize.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP_I2S.h>
#include <math.h>

// Replace both values with the actual Wi-Fi SSID and password.
const char *WIFI_SSID = "Diệnn";
const char *WIFI_PASSWORD = "39393939";
const char *SERVER_BASE = "http://172.20.10.2:5000";

constexpr int PIN_I2S_BCLK = 14;  // INMP441 SCK
constexpr int PIN_I2S_WS   = 15;  // INMP441 WS
constexpr int PIN_I2S_DIN  = 16;  // INMP441 SD

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t RECORD_SECONDS = 3;
constexpr size_t SAMPLE_COUNT = SAMPLE_RATE * RECORD_SECONDS;  // 48000 samples
constexpr size_t PCM_BYTES = SAMPLE_COUNT * sizeof(int16_t);  // 96000 bytes
constexpr int MIC_GAIN = 8;  // Software gain; adjust only after reviewing WAV/peak
constexpr size_t BLOCK_SAMPLES = 256;

I2SClass mic;
int32_t rawSamples[BLOCK_SAMPLES];  // DMA -> internal RAM; never PSRAM

bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.print("Connecting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERROR] WiFi not connected");
    return false;
  }
  Serial.print("[WIFI] ESP32 IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("[WIFI] Flask server: ");
  Serial.println(SERVER_BASE);
  return true;
}

void testPing() {
  if (!ensureWiFi()) return;

  WiFiClient client;
  HTTPClient http;
  String url = String(SERVER_BASE) + "/ping";
  if (!http.begin(client, url)) {
    Serial.println("[PING] http.begin failed");
    return;
  }
  http.setConnectTimeout(5000);
  http.setTimeout(10000);

  int code = http.GET();
  if (code > 0) {
    String body = http.getString();
    body.trim();
    Serial.printf("[PING] HTTP %d | Body: %s\n", code, body.c_str());
    if (code == 200 && body == "PONG") {
      Serial.println("[OK] ESP32 <-> Flask LAN connected");
    }
  } else {
    Serial.printf("[PING] HTTP error %d: %s\n", code, http.errorToString(code).c_str());
  }
  http.end();
}

// Store 3 seconds as mono, signed little-endian PCM16 at 16 kHz.
// INMP441 uses 24-bit samples in 32-bit I2S slots.
bool recordPcm(int16_t *pcm) {
  if (pcm == nullptr) return false;
  mic.setTimeout(1000);

  // Discard one old I2S block before a new recording.
  mic.readBytes(reinterpret_cast<char *>(rawSamples), sizeof(rawSamples));

  size_t recorded = 0;
  int emptyReads = 0;
  uint64_t sumSquares = 0;
  int peak = 0;

  Serial.println("[REC] Recording for 3 seconds - speak now!");
  while (recorded < SAMPLE_COUNT) {
    const size_t want = min(BLOCK_SAMPLES, SAMPLE_COUNT - recorded);
    const size_t gotBytes = mic.readBytes(
        reinterpret_cast<char *>(rawSamples), want * sizeof(int32_t));
    const size_t gotSamples = gotBytes / sizeof(int32_t);
    if (gotSamples == 0) {
      emptyReads++;
      if (emptyReads >= 5) {
        Serial.println("[ERROR] I2S mic did not return audio");
        return false;
      }
      continue;
    }
    emptyReads = 0;

    for (size_t i = 0; i < gotSamples && recorded < SAMPLE_COUNT; i++) {
      // Convert the high 16 bits to PCM16, then apply a limited gain.
      int32_t v = (rawSamples[i] >> 16) * MIC_GAIN;
      if (v > 32767) v = 32767;
      if (v < -32768) v = -32768;
      pcm[recorded++] = static_cast<int16_t>(v);
      const int magnitude = v >= 0 ? v : -v;
      if (magnitude > peak) peak = magnitude;
      sumSquares += static_cast<uint64_t>(v * v);
    }
  }

  const double rms = sqrt(static_cast<double>(sumSquares) / SAMPLE_COUNT);
  Serial.printf("[REC] %u bytes | peak=%d | RMS=%.1f\n",
                static_cast<unsigned>(PCM_BYTES), peak, rms);
  if (peak < 50) {
    Serial.println("[WARN] Audio is almost silent; check mic wiring/slot");
  }
  if (peak >= 32767) {
    Serial.println("[WARN] Audio clipped; lower MIC_GAIN if frequent");
  }
  return true;
}

void recordAndRecognize() {
  if (!ensureWiFi()) return;

  int16_t *pcm = nullptr;
  if (psramFound()) {
    pcm = static_cast<int16_t *>(ps_malloc(PCM_BYTES));
  }
  if (pcm == nullptr) {
    pcm = static_cast<int16_t *>(malloc(PCM_BYTES));
  }
  if (pcm == nullptr) {
    Serial.println("[ERROR] Could not allocate 96000-byte PCM buffer");
    Serial.println("[TIP] Enable PSRAM (OPI PSRAM) in Arduino IDE Tools");
    return;
  }

  if (!recordPcm(pcm)) {
    free(pcm);
    return;
  }

  WiFiClient client;
  HTTPClient http;
  const String url = String(SERVER_BASE) + "/recognize";
  if (!http.begin(client, url)) {
    Serial.println("[ERROR] HTTP setup failed");
    free(pcm);
    return;
  }
  http.setConnectTimeout(5000);
  http.setTimeout(30000);  // Google STT is not instantaneous
  http.addHeader("Content-Type", "application/octet-stream");

  Serial.printf("[HTTP] POST %s (%u bytes)\n", url.c_str(),
                static_cast<unsigned>(PCM_BYTES));
  int code = http.POST(reinterpret_cast<uint8_t *>(pcm), PCM_BYTES);
  if (code > 0) {
    String command = http.getString();
    command.trim();
    Serial.printf("[HTTP] Status: %d\n", code);
    if (code == 200 &&
        (command == "UP" || command == "DOWN" ||
         command == "STOP" || command == "UNKNOWN")) {
      Serial.print("[RESULT] COMMAND: ");
      Serial.println(command);
    } else {
      Serial.printf("[ERROR] Unexpected server status/response: %s\n",
                    command.c_str());
    }
  } else {
    Serial.printf("[ERROR] POST failed %d: %s\n", code,
                  http.errorToString(code).c_str());
  }

  http.end();
  free(pcm);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== FORD TERRITORY VOICE LAN TEST ===");
  Serial.println("Motor outputs are NOT controlled by this firmware.");

  mic.setPins(PIN_I2S_BCLK, PIN_I2S_WS, -1, PIN_I2S_DIN);
  if (!mic.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
                 I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("[ERROR] I2S microphone initialization failed");
    while (true) delay(1000);
  }
  Serial.println("[OK] I2S mic initialized at 16000 Hz / 32-bit LEFT");
  ensureWiFi();
  Serial.println("Press P + Enter to ping; R + Enter to record");
}

void loop() {
  if (!Serial.available()) {
    delay(20);
    return;
  }
  const char c = Serial.read();
  if (c == 'P' || c == 'p') {
    testPing();
  } else if (c == 'R' || c == 'r') {
    recordAndRecognize();
  }
  // Newline/CR from Serial Monitor is ignored.
}
