#pragma once
#include <Arduino.h>

// Sample rate: 16000 Hz, 16-bit Mono PCM
extern const size_t AUDIO_UP_SAMPLES;
extern const int16_t AUDIO_UP_DATA[] PROGMEM;

extern const size_t AUDIO_DOWN_SAMPLES;
extern const int16_t AUDIO_DOWN_DATA[] PROGMEM;

extern const size_t AUDIO_PINCH_SAMPLES;
extern const int16_t AUDIO_PINCH_DATA[] PROGMEM;

extern const size_t AUDIO_BEEP_SAMPLES;
extern const int16_t AUDIO_BEEP_DATA[] PROGMEM;
