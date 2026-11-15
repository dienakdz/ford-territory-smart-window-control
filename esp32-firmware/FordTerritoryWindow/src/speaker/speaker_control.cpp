#include "speaker_control.h"
#include <ESP_I2S.h>
#include "audio_data.h"
#include "../logger/logger.h"

extern bool isVoiceRecording;

// Doi tuong I2S dieu khien Loa (I2S_NUM_AUTO tu dong chon bo dieu khien I2S con ranh)
static I2SClass speaker;

static volatile SoundType currentRequest = SOUND_NONE;
static volatile bool isPlayingAudio = false;
static volatile bool cancelRequested = false;
static TaskHandle_t speakerTaskHandle = NULL;

// Task chay nen phat am thanh tren Core 0
static void speakerTaskLoop(void *pvParameters) {
  const size_t CHUNK_MONO = 128;
  int16_t stereoBuffer[CHUNK_MONO * 2];

  while (true) {
    if (currentRequest == SOUND_NONE) {
      isPlayingAudio = false;
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    SoundType sound = currentRequest;
    currentRequest = SOUND_NONE;
    isPlayingAudio = true;
    cancelRequested = false;

    const int16_t *srcData = nullptr;
    size_t totalSamples = 0;

    switch (sound) {
      case SOUND_UP_DONE:
        srcData = AUDIO_UP_DATA;
        totalSamples = AUDIO_UP_SAMPLES;
        break;
      case SOUND_DOWN_DONE:
        srcData = AUDIO_DOWN_DATA;
        totalSamples = AUDIO_DOWN_SAMPLES;
        break;
      case SOUND_ANTI_PINCH:
        srcData = AUDIO_PINCH_DATA;
        totalSamples = AUDIO_PINCH_SAMPLES;
        break;
      case SOUND_BEEP:
        srcData = AUDIO_BEEP_DATA;
        totalSamples = AUDIO_BEEP_SAMPLES;
        break;
      default:
        break;
    }

    if (srcData != nullptr && totalSamples > 0) {
      size_t samplesPlayed = 0;
      float vol = SPEAKER_VOLUME;
      if (vol < 0.0f) vol = 0.0f;
      if (vol > 1.0f) vol = 1.0f;

      while (samplesPlayed < totalSamples && !cancelRequested) {
        size_t count = min(CHUNK_MONO, totalSamples - samplesPlayed);

        for (size_t i = 0; i < count; i++) {
          int32_t val = static_cast<int32_t>(srcData[samplesPlayed + i] * vol);
          if (val > 32767) val = 32767;
          if (val < -32768) val = -32768;
          int16_t outSample = static_cast<int16_t>(val);

          // Nhan doi stereo: Kenh Trai = Kenh Phai (dam bao MAX98357A phat to ro ca 2 slot)
          stereoBuffer[i * 2]     = outSample;
          stereoBuffer[i * 2 + 1] = outSample;
        }

        speaker.write(reinterpret_cast<const uint8_t *>(stereoBuffer), count * 2 * sizeof(int16_t));
        samplesPlayed += count;

        // Nhuong CPU nhe giua cac chunk
        vTaskDelay(pdMS_TO_TICKS(1));
      }

      // Xa 64 mau 0 de ket thuc am thanh em ai, khong bi tieng pop/click
      memset(stereoBuffer, 0, sizeof(stereoBuffer));
      speaker.write(reinterpret_cast<const uint8_t *>(stereoBuffer), 64 * 2 * sizeof(int16_t));
    }

    isPlayingAudio = false;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void speakerInit() {
  speaker.setPins(PIN_I2S_SPK_BCLK, PIN_I2S_SPK_LRC, PIN_I2S_SPK_DIN, -1);
  if (!speaker.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    logEvent("[SPEAKER] LOI: Khoi tao I2S Loa MAX98357A that bai!");
    return;
  }

  logEvent(String("[SPEAKER] MAX98357A san sang (BCLK:") + String(PIN_I2S_SPK_BCLK) + 
           " | LRC:" + String(PIN_I2S_SPK_LRC) + " | DIN:" + String(PIN_I2S_SPK_DIN) + " | 16kHz Stereo).");

  xTaskCreatePinnedToCore(
      speakerTaskLoop,
      "SpeakerTask",
      6144, // 6KB stack an toan cho ESP-IDF I2S DMA
      NULL,
      1,
      &speakerTaskHandle,
      tskNO_AFFINITY // Tu do giua cac Core, tranh tranh chap co dinh voi VoiceTaskCore0
  );
}

void speakerPlay(SoundType sound) {
  // Neu dang trong phien thu am micro, khong phat am thanh de tranh nhieu am thanh vao micro
  if (isVoiceRecording && sound != SOUND_NONE) {
    return;
  }
  currentRequest = sound;
}

bool isSpeakerPlaying() {
  return isPlayingAudio;
}

void speakerStop() {
  cancelRequested = true;
  currentRequest = SOUND_NONE;
}

void notifyWindowMotionCompleted(bool wasUp, bool byVoice) {
  if (SPEAKER_FEEDBACK_VOICE_ONLY && !byVoice) {
    return;
  }

  if (wasUp) {
    logEvent("[SPEAKER] Hoan tat nang kinh -> Phat tieng: \"Đã nâng kính\"");
    speakerPlay(SOUND_UP_DONE);
  } else {
    logEvent("[SPEAKER] Hoan tat ha kinh -> Phat tieng: \"Đã hạ kính\"");
    speakerPlay(SOUND_DOWN_DONE);
  }
}
