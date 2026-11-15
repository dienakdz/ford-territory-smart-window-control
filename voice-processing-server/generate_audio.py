import urllib.request
import urllib.parse
import miniaudio
import numpy as np
import os

def fetch_and_process(text):
    print(f"Downloading TTS for: {text}...")
    url = f"https://translate.google.com/translate_tts?ie=UTF-8&q={urllib.parse.quote(text)}&tl=vi&client=tw-ob"
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req) as resp:
        mp3 = resp.read()
    dec = miniaudio.decode(mp3, nchannels=1, sample_rate=16000, output_format=miniaudio.SampleFormat.SIGNED16)
    samples = np.frombuffer(dec.samples, dtype=np.int16)
    idx = np.where(np.abs(samples) > 250)[0]
    if len(idx):
        start = max(0, idx[0] - 600)
        end = min(len(samples), idx[-1] + 600)
        samples = samples[start:end]
    peak = np.max(np.abs(samples))
    if peak > 0:
        samples = (samples.astype(np.float32) * (26000.0 / peak)).astype(np.int16)
    return samples

def make_beep(freq=1000, duration_s=0.08, sample_rate=16000):
    t = np.linspace(0, duration_s, int(sample_rate * duration_s), endpoint=False)
    envelope = np.sin(np.pi * np.linspace(0, 1, len(t)))
    wave = np.sin(2 * np.pi * freq * t) * envelope * 20000
    return wave.astype(np.int16)

def array_to_c(name, data):
    lines = [f"const size_t {name}_SAMPLES = {len(data)};", f"const int16_t {name}_DATA[] PROGMEM = {{"]
    chunk_size = 16
    for i in range(0, len(data), chunk_size):
        chunk = data[i:i+chunk_size]
        line = "  " + ", ".join(str(int(x)) for x in chunk) + ","
        lines.append(line)
    lines.append("};\n")
    return "\n".join(lines)

def main():
    up = fetch_and_process("Đã nâng kính")
    down = fetch_and_process("Đã hạ kính")
    pinch = fetch_and_process("Cảnh báo kẹt kính")
    beep = make_beep()

    speaker_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "esp32-firmware", "FordTerritoryWindow", "src", "speaker"))
    os.makedirs(speaker_dir, exist_ok=True)

    h_path = os.path.join(speaker_dir, "audio_data.h")
    cpp_path = os.path.join(speaker_dir, "audio_data.cpp")

    h_content = """#pragma once
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
"""
    with open(h_path, "w", encoding="utf-8") as f:
        f.write(h_content)

    print("Writing audio_data.cpp...")
    with open(cpp_path, "w", encoding="utf-8") as f:
        f.write('#include "audio_data.h"\n\n')
        f.write(array_to_c("AUDIO_UP", up))
        f.write(array_to_c("AUDIO_DOWN", down))
        f.write(array_to_c("AUDIO_PINCH", pinch))
        f.write(array_to_c("AUDIO_BEEP", beep))

    print(f"Generated successfully: {h_path} and {cpp_path}")
    print(f"Size of audio_data.cpp: {os.path.getsize(cpp_path)} bytes")

if __name__ == "__main__":
    main()
