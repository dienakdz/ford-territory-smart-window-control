"""Vietnamese text normalization module."""
import re
import unicodedata


def normalize(text: str) -> str:
    """
    Chuan hoa van ban tieng Viet ve chuan NFC, chu thuong, 
    chuyen doi cac dang phan so thong dung (1/2, 1/3, 2/3, 1 phan 2, 1 phan 3, 2 phan 3...).
    """
    if not text:
        return ""
    text = unicodedata.normalize("NFC", text).lower().strip()

    # Chuan hoa cac cach bieu dien phan so truoc khi xoa dau dac biet
    text = text.replace("1/2", " một nửa ")
    text = text.replace("1 phần 2", " một nửa ")
    text = text.replace("một phần hai", " một nửa ")
    text = text.replace("rưỡi", " một nửa ")

    text = text.replace("1/3", " một phần ba ")
    text = text.replace("1 phần 3", " một phần ba ")

    text = text.replace("2/3", " hai phần ba ")
    text = text.replace("2 phần 3", " hai phần ba ")

    # Thay 'nửa' khong co 'một' phia truoc thanh 'một nửa'
    text = re.sub(r"(?<!một\s)nửa", "một nửa", text)

    text = re.sub(r"[^\w\s]", " ", text, flags=re.UNICODE)
    return " ".join(text.split())
