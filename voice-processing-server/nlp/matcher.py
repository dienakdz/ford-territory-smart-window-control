"""Fuzzy N-gram Similarity Matching Engine."""
import difflib

from config import SIMILARITY_THRESHOLD
from .normalizer import normalize
from .profiles import COMMAND_PROFILES

# Cac tu khoa can loai tru / bo qua hoan toan (khong thuc hien bat ky lenh nao)
IGNORE_KEYWORDS = ["dừng", "ngừng", "ngưng", "stop", "thôi", "đứng lại"]


def compute_similarity_command(spoken_text: str):
    """
    Thuat toan Fuzzy NLP Matching:
    So khop do tuong dong chuoi & N-grams voi tap mau menh lenh (AUTO_UP, AUTO_DOWN).
    Tra ve tuple: (COMMAND, SCORE, MATCH_INFO)
    """
    norm = normalize(spoken_text)
    words = norm.split()
    if not words:
        return "UNKNOWN", 0.0, "Rỗng"

    # [BẢO VỆ]: Nếu câu nói chứa từ khóa dừng/ngưng, bỏ qua ngay (không so khớp nhầm sang nâng/hạ)
    for kw in IGNORE_KEYWORDS:
        if kw in norm:
            return "UNKNOWN", 0.0, f"Bỏ qua lệnh dừng ('{kw}')"

    # Trich xuat N-grams de bat y chinh trong cau noi (vd: "em oi ha kinh xuong di")
    ngrams = set()
    ngrams.add(norm)
    for n in (1, 2, 3):
        for i in range(len(words) - n + 1):
            ngrams.add(" ".join(words[i:i+n]))

    best_cmd = "UNKNOWN"
    best_score = 0.0
    best_match_info = ""

    for cmd, patterns in COMMAND_PROFILES.items():
        for pat in patterns:
            # Khớp chính xác (nằm trọn trong câu hoặc n-grams)
            if pat in ngrams or pat in norm:
                score = 1.0
                if score > best_score:
                    best_score = score
                    best_cmd = cmd
                    best_match_info = f"Khớp chính xác '{pat}'"
                continue

            # So khớp mờ (Fuzzy Similarity)
            for ng in ngrams:
                ratio = difflib.SequenceMatcher(None, ng, pat).ratio()
                if ratio > best_score:
                    best_score = ratio
                    best_cmd = cmd
                    best_match_info = f"'{ng}' ~ '{pat}'"

    if best_score >= SIMILARITY_THRESHOLD:
        return best_cmd, best_score, best_match_info
    else:
        return "UNKNOWN", best_score, best_match_info
