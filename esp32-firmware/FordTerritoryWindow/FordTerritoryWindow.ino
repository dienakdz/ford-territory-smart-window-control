/*
 * ==============================================================================
 * DỰ ÁN TỐT NGHIỆP: ĐIỀU KHIỂN KÍNH ĐIỆN THÔNG MINH FORD TERRITORY (ESP32-S3)
 * TÍCH HỢP CHỐNG KẸT THÍCH ỨNG (ADAPTIVE ANTI-PINCH) & WI-FI DASHBOARD
 * ==============================================================================
 *
 * TỔ CHỨC CÁC MODULE CHỨC NĂNG (Thư mục con: src/):
 *
 * 1. src/config/
 *    - config.h      : Định nghĩa toàn bộ chân GPIO, hằng số thời gian, ngưỡng bảo vệ.
 *    - types.h       : Định nghĩa các kiểu dữ liệu, enum trạng thái (Motor, Switch, Cal...).
 *
 * 2. src/logger/
 *    - logger.h/.cpp : Ghi nhật ký đồng thời ra Serial và Ring Buffer phục vụ Web Dashboard.
 *
 * 3. src/storage/
 *    - nvs_storage.h/.cpp : Lưu trữ và đọc dữ liệu hiệu chuẩn Baseline vào Flash NVS an toàn.
 *
 * 4. src/motor/
 *    - motor_control.h/.cpp : Điều khiển driver BTS7960, deadtime, giám sát hành trình TOP/BOTTOM.
 *
 * 5. src/switch/
 *    - switch_control.h/.cpp : Đọc ADC công tắc 4 nấc (lọc nhiễu 4 mẫu, debounce chống rung).
 *
 * 6. src/anti_pinch/
 *    - anti_pinch.h/.cpp : Đọc cảm biến dòng ACS712, thuật toán phát hiện chống kẹt khi AUTO UP.
 *
 * 7. src/calibration/
 *    - calibration.h/.cpp : Máy trạng thái hiệu chuẩn (tự hạ đáy -> nâng học baseline -> lưu NVS).
 *
 * 8. src/web/
 *    - web_server.h/.cpp : Trạm phát Wi-Fi AP riêng biệt (192.168.4.1) & Web Dashboard xem realtime.
 * ==============================================================================
 */

#include <Arduino.h>

// Nhúng các module chức năng từ thư mục src/
#include "src/config/config.h"
#include "src/config/types.h"
#include "src/logger/logger.h"
#include "src/storage/nvs_storage.h"
#include "src/motor/motor_control.h"
#include "src/switch/switch_control.h"
#include "src/anti_pinch/anti_pinch.h"
#include "src/calibration/calibration.h"
#include "src/web/web_server.h"
#include "src/voice/voice_control.h"

// Thời điểm cập nhật giá trị dòng điện lên Web Dashboard định kỳ
unsigned long lastStatusSampleMs = 0;

void setup() {
  // [1] Khởi tạo driver BTS7960 ở trạng thái ngắt an toàn ngay lập tức
  motorInit();

  // [2] Khởi tạo Serial giao tiếp máy tính (115200 baud)
  Serial.begin(115200);
  delay(100);

  // [3] Khởi tạo các chân đọc tín hiệu tương tự (ADC) và micro I2S
  switchInit();             // Cấu hình GPIO1 cho công tắc kính 4 nấc
  antiPinchInit();          // Cấu hình GPIO10 cho cảm biến dòng ACS712
  voiceInit();              // Cấu hình GPIO14, 15, 16 cho micro số INMP441
  analogReadResolution(12); // Độ phân giải 12-bit (0 - 4095)

  logEvent("[BOOT] Ford Territory Window Control Starting...");

  // [4] Đọc cấu hình Baseline từ bộ nhớ NVS Flash (nếu đã từng hiệu chuẩn thành công)
  loadCalibration();

  // [5] Khởi động Wi-Fi chế độ kép (WIFI_AP_STA):
  //     - Kết nối iPhone Hotspot với Static IP 172.20.10.50
  //     - Duy trì AP dự phòng 192.168.4.1 (FORD-WINDOW-DEBUG)
  startWiFi();

  // [6] Kiểm tra xem người dùng có giữ tổ hợp phím vào CAL ngay lúc khởi động hay không
  checkBootCal();

  // [7] Đọc dòng điện cảm biến lúc nghỉ ban đầu
  latestAcsMv = readAcsVoltageMv();
  if (calMode) {
    logEvent("[CAL] CALIBRATION ACTIVATED. Motor se tu ha xuong BOTTOM de chuan bi.");
  } else {
    logEvent("[MODE] NORMAL MODE. De vao CAL: Giu AUTO DOWN 2s -> THA -> Giu AUTO UP 2s.");
  }
}

void loop() {
  unsigned long now = millis();

  // ---------------------------------------------------------------------------
  // [BƯỚC 1]: ĐỌC VÀ CHỐNG RUNG (DEBOUNCE) CÔNG TẮC KÍNH
  // ---------------------------------------------------------------------------
  // Luôn đọc công tắc ở mọi chế độ để hệ thống phản hồi tức thì
  SwitchMode reading = readSwitchMode();

  if (reading != candidateSwitchMode) {
    candidateSwitchMode = reading;
    candidateSince = now;
  }

  // Khi tín hiệu công tắc đã ổn định vượt thời gian debounce (40ms)
  if (now - candidateSince >= SWITCH_DEBOUNCE_MS) {
    if (stableSwitchMode != candidateSwitchMode) {
      stableSwitchMode = candidateSwitchMode;
      logEvent(String("[SWITCH] ") + switchName(stableSwitchMode) + " | " + String(switchVoltageMv) + "mV");

      // Trong chế độ NORMAL, xử lý lệnh điều khiển của người dùng
      if (!calMode) {
        if (requireReleaseForNormal) {
          // Bảo vệ: Yêu cầu người dùng thả tay khỏi nút sau khi vừa thoát CAL
          if (stableSwitchMode == SW_RELEASED) {
            requireReleaseForNormal = false;
            logEvent("[MODE] NORMAL ready for commands.");
          }
        } else {
          // Xử lý các nấc UP, DOWN, AUTO UP, AUTO DOWN
          handleSwitchModeChange(stableSwitchMode);
        }
      }
    }
  }

  // ---------------------------------------------------------------------------
  // [BƯỚC 2]: XỬ LÝ THEO CHẾ ĐỘ HIỆN TẠI (CALIBRATION HOẶC NORMAL)
  // ---------------------------------------------------------------------------
  if (calMode) {
    // Nếu đang ở chế độ hiệu chuẩn: chạy máy trạng thái Calibration
    // (Tự hạ đáy -> Chờ người dùng nhấn UP -> Nâng lên TOP học baseline -> Tự lưu NVS)
    handleCal(now);
  } else {
    // Nếu đang ở chế độ NORMAL: Luồng điều khiển gốc được bảo toàn 100%
    monitorLimits();            // Giám sát chạm công tắc hành trình đỉnh/đáy
    monitorRunTimeout();         // Bảo vệ motor không chạy quá thời gian tối đa (8s)
    monitorAntiPinch();          // Giám sát dòng ACS712 khi AUTO UP (ngưỡng Baseline + Margin)
    monitorAntiPinchReverse();   // Đảo chiều hạ kính 1s nếu phát hiện kẹt (khi kích hoạt)

    // Luôn theo dõi tổ hợp phím (Gesture) để cho phép người dùng vào CAL bất kỳ lúc nào
    trackCalGesture(now);
  }

  // ---------------------------------------------------------------------------
  // [BƯỚC 3]: CẬP NHẬT DỮ LIỆU TELEMETRY & WEB DASHBOARD
  // ---------------------------------------------------------------------------
  // Giám sát kết nối Wi-Fi với iPhone Hotspot và tự động ping Flask server
  checkWiFiStatus();

  // Nhận lệnh thu âm / ping qua Serial nếu có cắm cáp debug
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'R' || c == 'r') {
      recordAndRecognize();
    } else if (c == 'P' || c == 'p') {
      testVoiceServerPing();
    }
  }

  // Cập nhật giá trị đo ACS712 siêu mượt mỗi 50ms phục vụ hiển thị Realtime lên Web Dashboard
  if (now - lastStatusSampleMs >= 50) {
    lastStatusSampleMs = now;
    latestAcsMv = readAcsVoltageMv();
  }

  // Xử lý các yêu cầu HTTP từ điện thoại/máy tính kết nối Wi-Fi (Non-blocking)
  handleWebClient();

  // Nhường CPU cho các tác vụ nền của ESP32 (Wi-Fi stack, RTOS idle)
  delay(5);
}
