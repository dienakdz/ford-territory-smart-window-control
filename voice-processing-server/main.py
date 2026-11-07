
import re
import wave
import unicodedata
import time

import sounddevice as sd
import speech_recognition as sr

recognizer = sr.Recognizer()

COMMANDS = {
    "nâng kính": "UP",
    "nâng kính ghế phụ": "UP",
    "hạ kính": "DOWN",
    "hạ kính ghế phụ": "DOWN",
    "dừng kính": "STOP",
    "dừng kính ghế phụ": "STOP",
}


def normalize_text(text):
    text = unicodedata.normalize("NFC", text)
    text = text.lower().strip()
    text = re.sub(r"[^\w\s]", "", text)
    return " ".join(text.split())


def record_audio(seconds=4):
    device = sd.query_devices(kind="input")
    sample_rate = int(device["default_samplerate"])

    print("\nMicro:", device["name"])
    print("Chuan bi thu am...")
    time.sleep(1)

    print("DANG THU AM - HAY NOI!")

    recording = sd.rec(
        int(seconds * sample_rate),
        samplerate=sample_rate,
        channels=1,
        dtype="int16"
    )

    sd.wait()
    print("Thu am xong!")

    # Luu file de nghe lai khi nhan dang sai
    with wave.open("test_voice.wav", "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        wav.writeframes(recording.tobytes())

    print("Da luu: test_voice.wav")

    return sr.AudioData(
        recording.tobytes(),
        sample_rate,
        2
    )


def main():
    print("=== FORD TERRITORY VOICE ===")

    while True:
        input("\nNhan ENTER de thu am...")

        try:
            audio = record_audio()

            print("Dang xu ly voi Google...")

            text = recognizer.recognize_google(
                audio,
                language="vi-VN"
            )

            normalized = normalize_text(text)
            command = COMMANDS.get(
                normalized, "UNKNOWN"
            )

            print("\n----------------------")
            print("Google nhan dang:", text)
            print("COMMAND:", command)
            print("----------------------")

        except sr.UnknownValueError:
            print("Google khong hieu giong noi.")
            print("COMMAND: UNKNOWN")

        except sr.RequestError as e:
            print("Loi ket noi Google:", e)

        except Exception as e:
            print("Loi:", e)


if __name__ == "__main__":
    main()
