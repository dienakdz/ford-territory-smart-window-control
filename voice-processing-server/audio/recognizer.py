"""Audio capture, metrics, and Google Speech Recognition wrapper."""
import wave
import numpy as np
import speech_recognition as sr

from config import (
    GOOGLE_STT_LANGUAGE,
    GOOGLE_STT_TIMEOUT,
    LAST_WAV_PATH,
    SAMPLE_RATE,
    SAMPLE_WIDTH,
    SAVE_LAST_WAV,
)

recognizer = sr.Recognizer()
recognizer.operation_timeout = GOOGLE_STT_TIMEOUT


def save_wav_file(pcm_bytes: bytes) -> None:
    """Luu buffer PCM16 ra file WAV de co the mo nghe lai va debug am thanh."""
    if not SAVE_LAST_WAV:
        return
    try:
        with wave.open(str(LAST_WAV_PATH), "wb") as wav:
            wav.setnchannels(1)
            wav.setsampwidth(SAMPLE_WIDTH)
            wav.setframerate(SAMPLE_RATE)
            wav.writeframes(pcm_bytes)
        print(f"[AUDIO] Da ghi file am thanh: {LAST_WAV_PATH.name}", flush=True)
    except OSError as err:
        print(f"[WARN] Khong the ghi file WAV: {err}", flush=True)


def get_audio_signal_metrics(pcm_bytes: bytes):
    """Tinh toan Peak va RMS cua tin hieu am thanh."""
    samples = np.frombuffer(pcm_bytes, dtype="<i2")
    peak = int(np.max(np.abs(samples.astype(np.int32))))
    rms = float(np.sqrt(np.mean(samples.astype(np.float64) ** 2)))
    return peak, rms


def recognize_speech_from_pcm(pcm_bytes: bytes) -> tuple[str, str]:
    """
    Gui buffer PCM16 sang Google Speech Recognition.
    Tu dong dem 400ms im lang (zero-padding) o cuoi de Google bat tron ven am duoi
    ma khong bi loi UnknownValueError do cat dot ngot vien file.
    Tra ve tuple: (transcription, error_status)
    """
    # Dem 400ms (0.4s) im lang vao duoi am thanh
    trailing_padding = b"\x00" * int(SAMPLE_RATE * 0.4 * SAMPLE_WIDTH)
    padded_pcm = pcm_bytes + trailing_padding

    audio_data = sr.AudioData(padded_pcm, SAMPLE_RATE, SAMPLE_WIDTH)
    try:
        print("[GOOGLE] Dang gui Google Speech Recognition (vi-VN)...", flush=True)
        text = recognizer.recognize_google(audio_data, language=GOOGLE_STT_LANGUAGE)
        return text, ""
    except sr.UnknownValueError:
        return "", "UNKNOWN_WORDS"
    except sr.RequestError as err:
        print(f"[ERROR] Loi ket noi mang Google STT: {err}", flush=True)
        return "", "NETWORK_ERROR"
