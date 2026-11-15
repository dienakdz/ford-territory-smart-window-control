"""Command profiles and dictionary of recognized phrases."""

# Tap mau lenh day du cho cac che do dieu khien kinh
# 1. AUTO_UP: Nang het kinh
# 2. AUTO_DOWN: Ha het kinh
# 3. UP_HALF: Nang 1/2 hanh trinh
# 4. UP_THIRD: Nang 1/3 hanh trinh
# 5. UP_TWO_THIRDS: Nang 2/3 hanh trinh
# 6. DOWN_HALF: Ha 1/2 hanh trinh
# 7. DOWN_THIRD: Ha 1/3 hanh trinh (he kinh)
# 8. DOWN_TWO_THIRDS: Ha 2/3 hanh trinh
COMMAND_PROFILES = {
    "UP_TWO_THIRDS": [
        "nâng hai phần ba", "nâng 2 phần 3", "lên hai phần ba", "lên 2 phần 3",
        "nâng hai phần ba kính", "lên hai phần ba kính", "kéo lên hai phần ba",
        "đóng hai phần ba", "nâng 2 3", "lên 2 3", "up two thirds"
    ],
    "DOWN_TWO_THIRDS": [
        "hạ hai phần ba", "hạ 2 phần 3", "xuống hai phần ba", "xuống 2 phần 3",
        "hạ hai phần ba kính", "xuống hai phần ba kính", "kéo xuống hai phần ba",
        "mở hai phần ba", "hạ 2 3", "xuống 2 3", "down two thirds"
    ],
    "UP_HALF": [
        "nâng một nửa", "nâng 1 2", "lên một nửa", "lên 1 2",
        "nâng một nửa kính", "lên một nửa kính", "nâng 1 2 kính", "lên 1 2 kính",
        "kéo lên một nửa", "đóng một nửa", "đóng một nửa kính", "up half"
    ],
    "UP_THIRD": [
        "nâng một phần ba", "nâng 1 3", "lên một phần ba", "lên 1 3",
        "nâng một phần ba kính", "lên một phần ba kính", "nâng 1 3 kính", "lên 1 3 kính",
        "kéo lên một phần ba", "đóng một phần ba", "up third"
    ],
    "DOWN_HALF": [
        "hạ một nửa", "hạ 1 2", "xuống một nửa", "xuống 1 2",
        "hạ một nửa kính", "xuống một nửa kính", "hạ 1 2 kính", "xuống 1 2 kính",
        "kéo xuống một nửa", "mở một nửa", "mở một nửa kính", "down half"
    ],
    "DOWN_THIRD": [
        "hạ một phần ba", "hạ 1 3", "xuống một phần ba", "xuống 1 3",
        "hạ một phần ba kính", "xuống một phần ba kính", "hạ 1 3 kính", "xuống 1 3 kính",
        "kéo xuống một phần ba", "mở một phần ba", "hé kính", "hé một phần ba", "down third"
    ],
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
