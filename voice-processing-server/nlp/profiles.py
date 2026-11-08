"""Command profiles and dictionary of recognized phrases."""

# Danh sach tu khoa ngat dung khan cap (Uu tien tuyet doi)
STOP_URGENT_KEYWORDS = [
    "dừng", "ngừng", "ngưng", "stop", "thôi", "đứng lại", "dừng lại"
]

# Tap mau lenh day du, bao gom cac tu chech am / ngo nguoi noi tieng Viet pho bien
COMMAND_PROFILES = {
    "STOP": [
        "dừng", "dừng lại", "dừng kính", "ngừng", "ngưng", "stop", "thôi", 
        "đứng lại", "đứng", "ngừng kính", "thôi dừng", "dừng xe", "dừng ngay"
    ],
    "AUTO_DOWN": [
        "hạ kính", "hạ cánh", "hạ kín", "xuống kính", "kéo kính xuống", 
        "hạ xuống", "xuống", "mở kính", "auto down", "kéo xuống", "hạ", 
        "hạ kính xuống", "kính xuống", "mở kính xuống", "hạ kén", "hạ kình"
    ],
    "AUTO_UP": [
        "nâng kính", "nâng cánh", "lên kính", "lên cánh", "lên kín", 
        "kéo kính lên", "nâng lên", "lên", "đóng kính", "auto up", "kéo lên", 
        "nâng kính lên", "kính lên", "đóng cửa kính", "lên kình", "nâng kình"
    ],
}
