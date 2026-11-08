"""Vietnamese text normalization module."""
import re
import unicodedata


def normalize(text: str) -> str:
    """Chuan hoa van ban tieng Viet ve chuan NFC, chu thuong, xoa ky tu la."""
    if not text:
        return ""
    text = unicodedata.normalize("NFC", text).lower().strip()
    text = re.sub(r"[^\w\s]", " ", text, flags=re.UNICODE)
    return " ".join(text.split())
