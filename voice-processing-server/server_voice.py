"""Ford Territory - ESP32-S3 -> Flask -> Google STT (LAN test only).

Input: POST /recognize, application/octet-stream
       3 seconds of signed little-endian PCM16 mono at 16 kHz (96,000 bytes).
Output: text/plain: UP, DOWN, STOP, or UNKNOWN.
"""

import re
import traceback
import unicodedata
import wave
from pathlib import Path

import numpy as np
import speech_recognition as sr
from flask import Flask, Response, request

HOST = "0.0.0.0"    # Listen on laptop network interfaces
PORT = 5000
SAMPLE_RATE = 16_000
DURATION_SECONDS = 3
SAMPLE_WIDTH = 2  # PCM signed 16-bit
EXPECTED_BYTES = SAMPLE_RATE * DURATION_SECONDS * SAMPLE_WIDTH  # 96000
SAVE_LAST_WAV = True  # Set False after debugging
LAST_WAV_PATH = Path(__file__).resolve().parent / "last_voice.wav"

app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = EXPECTED_BYTES

recognizer = sr.Recognizer()
recognizer.operation_timeout = 15  # Prevent an indefinite Google network wait

# Exact phrases only. Do NOT guess or autocorrect dangerous motor commands.
PHRASES = {
    "UP": {"nâng kính", "nâng kính ghế phụ"},
    "DOWN": {"hạ kính", "hạ kính ghế phụ"},
    "STOP": {"dừng", "dừng lại", "dừng kính", "ngừng", "ngừng kính"},
}


def normalize(text: str) -> str:
    text = unicodedata.normalize("NFC", text).lower().strip()
    text = re.sub(r"[^\w\s]", " ", text, flags=re.UNICODE)
    return " ".join(text.split())


def to_command(text: str) -> str:
    phrase = normalize(text)
    for command, accepted_phrases in PHRASES.items():
        if phrase in accepted_phrases:
            return command
    return "UNKNOWN"


def reply(command: str, status: int = 200) -> Response:
    return Response(command, status=status, content_type="text/plain; charset=utf-8")


@app.get("/ping")
def ping() -> Response:
    print("[PING] ESP32/client connected", flush=True)
    return reply("PONG")


@app.post("/recognize")
def recognize() -> Response:
    if request.mimetype != "application/octet-stream":
        print(f"[ERROR] Wrong content type: {request.mimetype}", flush=True)
        return reply("UNKNOWN", 415)

    pcm = request.get_data(cache=False)
    print(f"[AUDIO] Received {len(pcm)} bytes (expected {EXPECTED_BYTES})", flush=True)
    if len(pcm) != EXPECTED_BYTES:
        print("[ERROR] Invalid audio length", flush=True)
        return reply("UNKNOWN", 400)

    samples = np.frombuffer(pcm, dtype="<i2")
    peak = int(np.max(np.abs(samples.astype(np.int32))))
    rms = float(np.sqrt(np.mean(samples.astype(np.float64) ** 2)))
    print(f"[AUDIO] Peak={peak}, RMS={rms:.1f}", flush=True)

    if SAVE_LAST_WAV:
        try:
            with wave.open(str(LAST_WAV_PATH), "wb") as wav:
                wav.setnchannels(1)
                wav.setsampwidth(SAMPLE_WIDTH)
                wav.setframerate(SAMPLE_RATE)
                wav.writeframes(pcm)
            print(f"[AUDIO] WAV saved: {LAST_WAV_PATH}", flush=True)
        except OSError as error:
            print(f"[WARN] Could not save WAV: {error}", flush=True)

    # A fully silent or near-silent recording should never generate a command.
    if peak < 50:
        print("[AUDIO] Very low signal; returning UNKNOWN", flush=True)
        return reply("UNKNOWN")

    audio_data = sr.AudioData(pcm, SAMPLE_RATE, SAMPLE_WIDTH)
    try:
        print("[GOOGLE] Recognizing Vietnamese...", flush=True)
        transcription = recognizer.recognize_google(audio_data, language="vi-VN")
        command = to_command(transcription)
        print(f"[GOOGLE] Text: {transcription}", flush=True)
        print(f"[RESULT] COMMAND: {command}", flush=True)
        return reply(command)
    except sr.UnknownValueError:
        print("[RESULT] Google could not recognize speech: UNKNOWN", flush=True)
        return reply("UNKNOWN")
    except sr.RequestError as error:
        print(f"[ERROR] Google request failed: {error}", flush=True)
        return reply("UNKNOWN", 503)
    except Exception:
        traceback.print_exc()
        return reply("UNKNOWN", 500)


if __name__ == "__main__":
    print(f"Ford Territory Voice Server - listening at 0.0.0.0:{PORT}")
    print("LAN address (if unchanged): http://172.20.10.2:5000/ping")
    app.run(host=HOST, port=PORT, debug=False, use_reloader=False)
