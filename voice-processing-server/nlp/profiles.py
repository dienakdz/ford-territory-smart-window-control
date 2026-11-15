"""Command profiles and dictionary of recognized phrases."""

# Tap mau lenh day du cho cac che do dieu khien kinh
# Chi tap trung vao 2 lenh chinh: AUTO_UP (nang het kinh) va AUTO_DOWN (ha het kinh)
COMMAND_PROFILES = {
    "AUTO_DOWN": [
        "hạ kính", "hạ cánh", "hạ kín", "xuống kính", "kéo kính xuống", 
        "hạ xuống", "xuống", "mở kính", "auto down", "kéo xuống", "hạ", 
        "hạ kính xuống", "kính xuống", "mở kính xuống", "hạ kén", "hạ kình",
        "hạ hết", "hạ hết kính", "xuống hết", "xuống hết kính", "mở hết kính"
    ],
    "AUTO_UP": [
        "nâng kính", "nâng cánh", "lên kính", "lên cánh", "lên kín", 
        "kéo kính lên", "nâng lên", "lên", "đóng kính", "auto up", "kéo lên", 
        "nâng kính lên", "kính lên", "đóng cửa kính", "lên kình", "nâng kình",
        "nâng hết", "nâng hết kính", "lên hết", "lên hết kính", "đóng hết kính"
    ],
}
