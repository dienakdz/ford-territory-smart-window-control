"""
Ford Territory Smart Window Control - Voice Processing Server
Flask LAN Server routing requests from ESP32-S3 to Google STT and NLP Engine.
"""
from flask import Flask, Response, request

import config
from audio import get_audio_signal_metrics, recognize_speech_from_pcm, save_wav_file
from network import get_laptop_ip
from nlp import compute_similarity_command

app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = config.MAX_CONTENT_LENGTH


def make_response(command: str, transcription: str = "", status: int = 200) -> Response:
    """Tao HTTP response tra ve cho ESP32."""
    body = f"{command}:{transcription}" if transcription else command
    return Response(body, status=status, content_type="text/plain; charset=utf-8")


@app.get("/ping")
def handle_ping() -> Response:
    """Kiem tra ket noi mang giua ESP32 va Laptop."""
    client_ip = request.remote_addr
    print(f"\n[PING] ESP32 da ket noi tu: http://{client_ip}", flush=True)
    print(f"[DASHBOARD] Mo Web Dashboard tai: http://{client_ip}\n", flush=True)
    return make_response("PONG")


@app.post("/recognize")
def handle_recognize() -> Response:
    """Nhan stream PCM16 tu ESP32, nhan dang Google STT va tra ve lenh dieu khien."""
    if request.mimetype != "application/octet-stream":
        print(f"[ERROR] Sai Content-Type: {request.mimetype}", flush=True)
        return make_response("UNKNOWN", status=415)

    pcm_data = request.get_data(cache=False)
    print(f"\n[AUDIO] Nhan {len(pcm_data)} bytes tu ESP32", flush=True)

    if len(pcm_data) < config.MIN_AUDIO_BYTES:
        print("[ERROR] Du lieu am thanh qua ngan (< 0.25s)", flush=True)
        return make_response("UNKNOWN", status=400)

    # Tinh toan tin hieu am thanh & ghi file WAV de nghe lai khi can
    peak, rms = get_audio_signal_metrics(pcm_data)
    print(f"[AUDIO] Tin hieu: Peak={peak}, RMS={rms:.1f}", flush=True)
    save_wav_file(pcm_data)

    if peak < 45:
        print("[AUDIO] Am thanh qua nho / im lang -> UNKNOWN", flush=True)
        return make_response("UNKNOWN")

    # Goi Google Speech Recognition
    transcription, error_status = recognize_speech_from_pcm(pcm_data)
    if error_status == "UNKNOWN_WORDS":
        print("[RESULT] Google khong nhan dang duoc tu ngu: UNKNOWN", flush=True)
        return make_response("UNKNOWN", "(Không rõ từ ngữ)")
    elif error_status == "NETWORK_ERROR":
        return make_response("UNKNOWN", "Lỗi mạng Google", status=503)

    # Xu ly NLP do tuong dong de quyet dinh lenh
    command, score, match_info = compute_similarity_command(transcription)
    if command != "UNKNOWN":
        print(f"[NLP MATCH] \"{transcription}\" ==> [{command}] | Do tuong dong: {score*100:.1f}% ({match_info})", flush=True)
        display_text = f"{transcription} ({int(score*100)}%)"
    else:
        print(f"[NLP IGNORE] Bo qua cau tro chuyen: \"{transcription}\" (Do khop: {score*100:.1f}% < {config.SIMILARITY_THRESHOLD*100:.0f}%)", flush=True)
        display_text = f"{transcription} (ngoai luong)"

    print(f"[RESULT] LENH XAC NHAN: ===> [ {command} ]", flush=True)
    return make_response(command, display_text)


if __name__ == "__main__":
    laptop_ip = get_laptop_ip()
    print("=" * 60)
    print(" FORD TERRITORY VOICE CONTROL SERVER (FLASK)")
    print(" 3 CHE DO: AUTO UP | AUTO DOWN | STOP")
    print("=" * 60)
    print(f"Server dang lang nghe tai: http://{config.HOST}:{config.PORT}")
    print(f"Dia chi IP Laptop trong mang Hotspot: http://{laptop_ip}:{config.PORT}")
    print(f"ESP32 se gui am thanh ve: http://{laptop_ip}:{config.PORT}/recognize")
    print(f"Mo Dashboard ESP32 tai: http://172.20.10.3")
    print("=" * 60)
    app.run(host=config.HOST, port=config.PORT, debug=False, use_reloader=False)
