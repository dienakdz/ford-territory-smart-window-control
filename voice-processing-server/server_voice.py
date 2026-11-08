"""Ford Territory - ESP32-S3 -> Flask -> Google STT (LAN / Hotspot).

Input: POST /recognize, application/octet-stream
       3 seconds of signed little-endian PCM16 mono at 16 kHz (96,000 bytes).
Output: text/plain: AUTO_UP, AUTO_DOWN, STOP, or UNKNOWN.
"""

import re
import socket
import traceback
import unicodedata
import wave
from pathlib import Path

import numpy as np
import speech_recognition as sr
from flask import Flask, Response, request

HOST = "0.0.0.0"    # Lang nghe tren moi card mang cua Laptop
PORT = 5000
SAMPLE_RATE = 16_000
DURATION_SECONDS = 3
SAMPLE_WIDTH = 2  # PCM signed 16-bit
EXPECTED_BYTES = SAMPLE_RATE * DURATION_SECONDS * SAMPLE_WIDTH  # 96,000 bytes
SAVE_LAST_WAV = True
LAST_WAV_PATH = Path(__file__).resolve().parent / "last_voice.wav"

app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = EXPECTED_BYTES

recognizer = sr.Recognizer()
recognizer.operation_timeout = 15  # Phong ngua timeout mang Google

# 3 CHE DO GIONG NOI: AUTO UP, AUTO DOWN, STOP (Chuan xac, an toan tuyet doi)
PHRASES = {
    "AUTO_UP": {
        "nâng kính", "nâng kính ghế phụ", 
        "lên kính", "kéo kính lên", "mở kính lên",
        "nâng kính lên", "auto lên", "auto up"
    },
    "AUTO_DOWN": {
        "hạ kính", "hạ kính ghế phụ", 
        "xuống kính", "kéo kính xuống", "mở kính xuống",
        "hạ kính xuống", "auto xuống", "auto down"
    },
    "STOP": {
        "dừng", "dừng lại", "dừng kính", "ngừng", "ngừng kính", 
        "dừng kính lại", "stop"
    },
}


def get_laptop_ip():
    """Lay dia chi IP cua Laptop trong mang Hotspot/LAN."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("172.20.10.1", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        try:
            return socket.gethostbyname(socket.gethostname())
        except Exception:
            return "172.20.10.2"


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


def reply(command: str, transcription: str = "", status: int = 200) -> Response:
    body = f"{command}:{transcription}" if transcription else command
    return Response(body, status=status, content_type="text/plain; charset=utf-8")


@app.get("/ping")
def ping() -> Response:
    client_ip = request.remote_addr
    print(f"\n[PING] ESP32 da ket noi tu: http://{client_ip}", flush=True)
    print(f"[DASHBOARD] Mo Web Dashboard tai: http://{client_ip}\n", flush=True)
    return reply("PONG")


@app.post("/recognize")
def recognize() -> Response:
    if request.mimetype != "application/octet-stream":
        print(f"[ERROR] Sai content type: {request.mimetype}", flush=True)
        return reply("UNKNOWN", 415)

    pcm = request.get_data(cache=False)
    print(f"\n[AUDIO] Nhan {len(pcm)} bytes tu ESP32 (ky vong {EXPECTED_BYTES})", flush=True)
    if len(pcm) != EXPECTED_BYTES:
        print("[ERROR] Kich thuoc audio khong dung", flush=True)
        return reply("UNKNOWN", 400)

    samples = np.frombuffer(pcm, dtype="<i2")
    peak = int(np.max(np.abs(samples.astype(np.int32))))
    rms = float(np.sqrt(np.mean(samples.astype(np.float64) ** 2)))
    print(f"[AUDIO] Tin hieu am thanh: Peak={peak}, RMS={rms:.1f}", flush=True)

    if SAVE_LAST_WAV:
        try:
            with wave.open(str(LAST_WAV_PATH), "wb") as wav:
                wav.setnchannels(1)
                wav.setsampwidth(SAMPLE_WIDTH)
                wav.setframerate(SAMPLE_RATE)
                wav.writeframes(pcm)
            print(f"[AUDIO] Da ghi file: {LAST_WAV_PATH.name}", flush=True)
        except OSError as error:
            print(f"[WARN] Khong the luu file WAV: {error}", flush=True)

    # Tin hieu qua nho (im lang)
    if peak < 50:
        print("[AUDIO] Am thanh qua nho / im lang -> UNKNOWN", flush=True)
        return reply("UNKNOWN")

    audio_data = sr.AudioData(pcm, SAMPLE_RATE, SAMPLE_WIDTH)
    try:
        print("[GOOGLE] Dang gui Google Speech Recognition (vi-VN)...", flush=True)
        transcription = recognizer.recognize_google(audio_data, language="vi-VN")
        command = to_command(transcription)
        print(f"[GOOGLE] Van ban nhan dang: \"{transcription}\"", flush=True)
        print(f"[RESULT] LENH XAC NHAN: ===> [ {command} ]", flush=True)
        return reply(command, transcription)
    except sr.UnknownValueError:
        print("[RESULT] Google khong nhan dang duoc tu ngu: UNKNOWN", flush=True)
        return reply("UNKNOWN", "(Không rõ từ ngữ)")
    except sr.RequestError as error:
        print(f"[ERROR] Loi ket noi mang Google STT: {error}", flush=True)
        return reply("UNKNOWN", "Lỗi mạng Google", 503)
    except Exception:
        traceback.print_exc()
        return reply("UNKNOWN", 500)


if __name__ == "__main__":
    laptop_ip = get_laptop_ip()
    print("=" * 60)
    print(" FORD TERRITORY VOICE CONTROL SERVER (FLASK)")
    print(" 3 CHE DO: AUTO UP | AUTO DOWN | STOP")
    print("=" * 60)
    print(f"Server dang lang nghe tai: http://0.0.0.0:{PORT}")
    print(f"Dia chi IP Laptop trong mang Hotspot: http://{laptop_ip}:{PORT}")
    print(f"ESP32 se gui am thanh ve: http://{laptop_ip}:{PORT}/recognize")
    print(f"Mo Dashboard ESP32 tai: http://172.20.10.3 hoac http://ford-window.local")
    print("=" * 60)
    app.run(host=HOST, port=PORT, debug=False, use_reloader=False)
