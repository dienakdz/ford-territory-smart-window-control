"""Hybrid Semantic & Fuzzy N-gram Matching Engine."""
import difflib

from config import SIMILARITY_THRESHOLD
from .normalizer import normalize
from .profiles import COMMAND_PROFILES

# Cac tu khoa ngat / huy
IGNORE_KEYWORDS = ["dừng", "ngừng", "ngưng", "stop", "thôi", "đứng lại"]


def compute_similarity_command(spoken_text: str):
    """
    Thuat toan Nhan dien Ngon ngu Tu nhien ket hop Semantic Rule & Fuzzy Matching:
    1. Bo qua hoan toan neu cau noi co chua tu khoa dung/ngung.
    2. Phan tich y dinh ngu nghia: Ty le (2/3, 1/3, 1/2) + Huong chuyen dong (NANG / HA).
       -> Dam bao 100% khong bao gio bi nham giua lenh ty le va lenh AUTO het hanh trinh!
    3. Fallback sang Fuzzy SequenceMatcher neu cau noi chua khop.
    """
    norm = normalize(spoken_text)
    words = norm.split()
    if not words:
        return "UNKNOWN", 0.0, "Rỗng"

    # [BƯỚC 1]: Bỏ qua ngay nếu có từ khóa dừng
    for kw in IGNORE_KEYWORDS:
        if kw in norm:
            return "UNKNOWN", 0.0, f"Bỏ qua lệnh dừng ('{kw}')"

    # [BƯỚC 2]: Xác định hướng chuyển động (Direction)
    is_up = any(w in norm for w in ["nâng", "lên", "đóng", "kéo lên", "auto up"])
    is_down = any(w in norm for w in ["hạ", "xuống", "mở", "kéo xuống", "auto down"])

    # [BƯỚC 3]: Xác định tỷ lệ hành trình (Fraction / Step)
    has_2_3 = any(w in norm for w in ["hai phần ba", "2 3", "2 phần 3", "2/3"])
    has_1_3 = any(w in norm for w in ["một phần ba", "1 3", "1 phần 3", "1/3", "hé"])
    has_half = any(w in norm for w in ["một nửa", "1 2", "1 phần 2", "1/2", "nửa"])

    # Ưu tiên tuyệt đối cho lệnh theo tỷ lệ cụ thể
    if has_2_3:
        if is_up:
            return "UP_TWO_THIRDS", 1.0, "Ngữ nghĩa 2/3 (chiều NÂNG)"
        if is_down:
            return "DOWN_TWO_THIRDS", 1.0, "Ngữ nghĩa 2/3 (chiều HẠ)"

    if has_1_3:
        if is_up:
            return "UP_THIRD", 1.0, "Ngữ nghĩa 1/3 (chiều NÂNG)"
        if is_down:
            return "DOWN_THIRD", 1.0, "Ngữ nghĩa 1/3 (chiều HẠ)"

    if has_half:
        if is_up:
            return "UP_HALF", 1.0, "Ngữ nghĩa 1/2 (chiều NÂNG)"
        if is_down:
            return "DOWN_HALF", 1.0, "Ngữ nghĩa 1/2 (chiều HẠ)"

    # Nếu KHÔNG CÓ từ chỉ tỷ lệ -> Mới xét lệnh nâng/hạ hết (AUTO)
    if is_up and not is_down:
        return "AUTO_UP", 1.0, "Ngữ nghĩa NÂNG HẾT (AUTO UP)"
    if is_down and not is_up:
        return "AUTO_DOWN", 1.0, "Ngữ nghĩa HẠ HẾT (AUTO DOWN)"

    # [BƯỚC 4]: Fallback Fuzzy SequenceMatcher
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
            if pat in ngrams or pat in norm:
                score = 1.0
                if score > best_score:
                    best_score = score
                    best_cmd = cmd
                    best_match_info = f"Khớp chính xác '{pat}'"
                continue

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
