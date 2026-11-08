"""Configuration settings for Ford Territory Voice Processing Server."""
from pathlib import Path

# --- Network Configuration ---
HOST = "0.0.0.0"
PORT = 5000

# --- Audio PCM Specifications ---
SAMPLE_RATE = 16_000
DURATION_SECONDS = 2
SAMPLE_WIDTH = 2  # 16-bit PCM signed mono
EXPECTED_BYTES = SAMPLE_RATE * DURATION_SECONDS * SAMPLE_WIDTH  # 64,000 bytes
MIN_AUDIO_BYTES = 8_000  # Bo qua cac packet qua ngan (< 0.25s)
MAX_CONTENT_LENGTH = 128 * 1024  # 128 KB buffer limit

# --- Audio Storage ---
SAVE_LAST_WAV = True
LAST_WAV_PATH = Path(__file__).resolve().parent / "last_voice.wav"

# --- Google STT Configuration ---
GOOGLE_STT_TIMEOUT = 10  # seconds
GOOGLE_STT_LANGUAGE = "vi-VN"

# --- NLP Similarity Matching ---
SIMILARITY_THRESHOLD = 0.72  # 72% minimum similarity score
