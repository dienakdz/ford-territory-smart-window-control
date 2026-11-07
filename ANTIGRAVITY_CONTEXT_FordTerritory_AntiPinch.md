# ANTIGRAVITY HANDOFF — Ford Territory Window Control / Adaptive Anti-Pinch

> **Ngôn ngữ trao đổi:** Tiếng Việt. **Ngày tổng hợp:** 07/11/2026.  
> **Mục đích:** Dán/gửi file này cho Antigravity và để trong thư mục dự án, để agent đọc source thực tế, tự kiểm tra/biên dịch/chạy test phần mềm và tiếp tục phát triển.  
> **Quan trọng:** Các kết quả về điện áp bên dưới là **người dùng đo bằng VOM**; các bản firmware mới chỉ được kiểm tra bằng **mô phỏng/stub C++ trong phiên ChatGPT**, **chưa được xác thực trên ESP32-S3 và cơ cấu kính thật**. Đừng coi mọi tính năng mô tả là đã chạy thành công trên phần cứng.

## 1. Mục tiêu đồ án và phạm vi công việc

Đề tài: điều khiển kính điện **ghế phụ Ford Territory**, gồm:
- Nâng/hạ **thủ công** (giữ UP/DOWN), **AUTO UP/AUTO DOWN**.
- **Chống kẹt (anti-pinch)** khi AUTO UP, dựa trên dòng motor đo bằng ACS712.
- Chức năng hiệu chuẩn tải bình thường (**CALIBRATION / baseline**) để tránh phụ thuộc ngưỡng điện áp tuyệt đối khi đổi motor/cơ cấu/dây/nguồn.
- Wi-Fi AP và dashboard để xem trạng thái/log từ điện thoại mà **không cắm USB khi ESP32 đang dùng nguồn ngoài**.
- Phần điều khiển giọng nói tiếng Việt qua INMP441 → ESP32 → Wi-Fi LAN → Python Flask/STT là **nhánh tích hợp khác**, chưa ghép vào firmware kính hiện tại; **không làm phát sinh thêm chức năng voice khi chỉ được yêu cầu sửa anti-pinch**.

**Ưu tiên của người dùng:** KISS / không over-engineer; mỗi bước ít thay đổi, dễ kiểm chứng, giữ nguyên chức năng chạy ổn định. Người dùng có agent Antigravity trên máy Windows; muốn giao cho agent truy cập source trong workspace, sửa code, build, kiểm tra log/test, so sánh phiên bản nhanh hơn việc copy code qua chat.

## 2. Phần cứng và wiring đã dùng trong sketch

| Thiết bị | ESP32-S3 / ghi chú |
|---|---|
| ESP32-S3 | N16R8 — 16 MB Flash, 8 MB PSRAM |
| Driver motor BTS7960 | RPWM GPIO4; LPWM GPIO5; R_EN GPIO6; L_EN GPIO7 |
| Hành trình trên | GPIO8 (`LIMIT_TOP`) |
| Hành trình dưới | GPIO9 (`LIMIT_BOTTOM`) |
| Công tắc kính 4 mức analog | GPIO1 (`SWITCH_ADC_PIN`) |
| Cảm biến dòng ACS712 | Ngõ OUT **qua mạch chia áp** → GPIO10 (`ACS_ADC_PIN`) |
| Thiết bị bổ sung cho voice | Mic INMP441 I²S (SCK GPIO14, WS GPIO15, SD GPIO16, L/R GND), MAX98357A, loa 4Ω 3W |

- Logic limit switch hiện tại **giả định NC, bình thường LOW, khi chạm/mạch mở HIGH** (`topReached()/bottomReached()`). **Phải xác nhận trên phần cứng**. Không suy ra trạng thái an toàn chỉ từ tên pin.
- Công tắc analog trong source gốc có mức: RELEASED >=2900 mV; UP 2300–2750 mV; AUTO UP 1450–2000 mV; DOWN 600–1100 mV; AUTO DOWN <=250 mV. Đây là ngưỡng của **ADC GPIO1**, cần kiểm tra theo wiring thực tế.
- ACS712 đã từng được đảo **chiều qua hai đầu đo dòng IP+/IP−**, khiến điện áp OUT chuyển từ giảm theo tải sang **tăng theo tải**. Không đảo VCC/GND cảm biến, không đảo nguồn motor; cần xác nhận vị trí cảm biến ở đường cấp BTS7960 hay đường motor.
- Model/độ nhạy ACS712 (5A/20A/30A), giá trị mạch chia áp, điện áp nguồn cảm biến, sơ đồ nguồn cấp ESP32 **chưa được xác nhận**. Không tự suy ra dòng (A) chỉ từ điện áp VOM.

## 3. Số liệu thực tế người dùng đo bằng VOM

### 3.1. Trước khi đảo chiều dòng qua ACS712

| Trạng thái | Điện áp VOM |
|---|---:|
| Motor dừng | 1,67 V |
| Motor nâng/hạ bình thường | 1,57–1,58 V |
| Motor bị cản khi AUTO UP | 1,53–1,55 V |

Vì điện áp giảm khi tải tăng, logic phát hiện *dạng này* sẽ so sánh điện áp giảm (chỉ khi đo đúng chân và đã hiệu chuẩn). Đây là dữ liệu lịch sử, **không còn dùng cho hướng hiện tại**.

### 3.2. Sau khi đảo chiều dòng qua ACS712 — hướng đang dùng

| Trạng thái | Điện áp VOM |
|---|---:|
| Motor dừng | khoảng **1,67 V** |
| AUTO UP lúc khởi động | **1,93–1,94 V** |
| AUTO UP chạy ổn định | **1,87–1,89 V** |
| Bị cản/kẹt trong thử nghiệm AUTO UP | **1,97–1,98 V** |

- Điện áp VOM **chưa chắc bằng** `analogReadMilliVolts(GPIO10)` nếu VOM đo ở OUT ACS712 **trước** chia áp còn ESP32 đọc **sau** chia áp. Hãy hỏi/đối chiếu vị trí đo trước khi kết luận ngưỡng.
- VOM có thể lấy trung bình; ESP32 ADC thấy nhiễu/gợn dòng PWM nhanh hơn. Chưa có dữ liệu log ADC thực nghiệm dùng để chọn ngưỡng.
- Baseline ước tính theo số liệu VOM = **1,88 V = 1880 mV**.
- Chênh lệch giữa mức ổn định ~1,88 V và mức bị cản ~1,975 V ≈ **95 mV**. **Không đồng nghĩa +90 mV đã là ngưỡng anti-pinch an toàn**, do rất gần mức tăng khi kẹt, có thể phát hiện trễ hoặc sai.

## 4. Thuật toán đã thống nhất: baseline + margin

Khái niệm:

```text
BASELINE_MV = điện áp ADC đại diện khi nâng kính bình thường, được đo trong CAL hợp lệ
ANTI_PINCH_MARGIN_MV = 90   // thông số thử nghiệm dễ thay đổi
THRESHOLD_MV = BASELINE_MV + ANTI_PINCH_MARGIN_MV

Ví dụ minh họa (KHÔNG phải kết quả đã đo bằng ESP32):
BASELINE_MV = 1880 → THRESHOLD_MV = 1970
```

- Không dùng ngưỡng tuyệt đối `1705 mV` từ sketch gốc: sau khi đảo chiều, mức nâng bình thường đã là khoảng 1870–1890 mV (theo VOM).
- **Không cập nhật baseline liên tục trong khi nghi ngờ kẹt**: nếu điện áp tăng vì vật cản mà baseline cũng tăng, ngưỡng sẽ chạy theo và bỏ sót.
- Hướng được chọn: người dùng **chủ động chạy CAL khi lắp lần đầu/đổi phần cứng/cần hiệu chuẩn lại**; sau khi hiệu chuẩn hợp lệ, lưu baseline NVS/Flash; những lần bình thường sử dụng lại, không học mỗi lần AUTO UP.
- `+90 mV` là **biên độ hiện người dùng muốn cố định để kiểm thử thuật toán**, dễ chỉnh tại **một hằng số**. Agent có thể đề xuất hiệu chuẩn margin về sau nhưng **đừng âm thầm đổi yêu cầu hoặc tự động bật đảo chiều**.
- Baseline một số cho cả hành trình chỉ là mô hình đơn giản. Ma sát thay đổi theo vị trí, nguồn, nhiệt độ, tải, PWM. Để kết luận an toàn cần đo nhiều chu kỳ + kiểm tra lực và thời gian phản ứng bằng đồ gá.

## 5. UX mong muốn — ngắn gọn, không cần USB hay dashboard thao tác

### NORMAL MODE
- Bình thường cấp nguồn và thả công tắc → **NORMAL**.
- Nhấn giữ UP/DOWN thủ công → motor chạy đến khi nhả hoặc chạm limit; AUTO DOWN bình thường.
- AUTO UP chỉ được cho phép sau khi baseline hợp lệ được lưu (theo phiên bản code mới).
- Trong quá trình AUTO UP, so sánh ACS712 với `baseline + 90 mV`; **phiên bản hiện tại chỉ DỪNG MOTOR + LOG khi nghi ngờ vượt ngưỡng**, **KHÔNG tự đảo chiều** vì chưa chứng minh an toàn.

### Kích hoạt CAL không xung đột với DOWN thủ công
- **Chỉ nhận chuỗi lúc khởi động/reset**: giữ **AUTO DOWN 2 giây → THẢ → giữ AUTO UP 2 giây → THẢ**.
- Tuyệt đối **không dùng giữ DOWN thủ công 5 giây** làm phím tắt CAL vì sẽ xung đột với thao tác hạ kính thường ngày.

### CAL MODE — trải nghiệm người dùng mong muốn
1. Vào CAL bằng chuỗi trên (log từng bước).
2. Firmware **tự hạ kính đến BOTTOM** (công tắc hành trình dưới), trong điều kiện thử nghiệm đã được xác nhận an toàn; ưu tiên bảo vệ giới hạn và emergency cut độc lập.
3. Khi đã ở BOTTOM, hiện trạng thái **`CAL: HOLD MANUAL UP`**.
4. Người dùng **GIỮ UP thủ công liên tục** đến hành trình TOP. **Không phải nhấp một cái**; nếu nhả trước TOP thì dừng/hủy CAL và **không lưu** kết quả mới.
5. Thu ACS712 khi nâng bình thường, loại đoạn khởi động khỏi việc học baseline, tính baseline, kiểm tra hợp lệ.
6. Chạm TOP → motor dừng → **tự lưu NVS/Flash nếu hợp lệ** (không yêu cầu REVIEW/SAVE thủ công) → **tự thoát CAL và về NORMAL**, không cần reset/nạp lại firmware. Yêu cầu thả mọi nút trước khi chấp nhận lệnh NORMAL mới.
7. Nếu lỗi hành trình, timeout, đọc cảm biến bất thường, thiếu mẫu, nhả UP sớm: dừng motor, báo lỗi, giữ nguyên baseline cũ, về NORMAL theo điều kiện an toàn. Khi đổi motor hay muốn học lại, chủ động kích hoạt lại CAL.

**Điểm quan trọng:** Trong CAL, *khóa điều khiển AUTO/thủ công thông thường* nhưng vẫn cho phép hành trình CAL đã định nghĩa; KHÔNG có nghĩa mọi output motor đều bị khóa trong CAL. NORMAL không nên bị khóa toàn bộ như Stage 1/2 chẩn đoán.

## 6. Firmware hiện có và trạng thái thực sự

**Nguồn chân lý là file firmware thực sự nằm trong workspace máy người dùng, không phải một đoạn mô tả trong chat.** Nếu workspace chưa có file, dùng bản `.ino` đính kèm trong gói handoff.

### Phiên bản chính gần nhất được tạo trong ChatGPT

`FordWindow_CAL_Normal_WiFi.ino` — 704 dòng, được tạo 07/11/2026. SHA-256: `13e3f2fac080442cab5c294038fc2683ba62099366fa9e951b881eebd3d99f3b`.

Các file lịch sử để đối chiếu:
- `sketch_oct19b.ino`: sketch gốc, motor manual/auto, chống kẹt ngưỡng **cố định 1705 mV**, khi vượt thì dừng và đảo chiều xuống tối đa 1 s; 560 dòng. **KHÔNG phù hợp điện áp mới nếu chưa sửa**.
- `FordWindow_Calibration_Stage2.ino`: bản trung gian CAL + Wi-Fi, vận hành bình thường còn khóa; **không dùng làm final**.
- `FordWindow_CAL_Normal_WiFi.ino`: bản hợp nhất NORMAL + CAL + Wi-Fi, **bản đang cần agent review/kiểm thử**.

### Đối chiếu code thực tế của bản gần nhất

**Các vị trí cần đọc** (số dòng có thể lệch nếu file local đã chỉnh):
- `ANTI_PINCH_MARGIN_MV = 90` gần dòng 48.
- `ENABLE_EXPERIMENTAL_REVERSE = false` gần dòng 49. **Không tự bật `true`**.
- GPIO pin constants gần dòng 38–45.
- `loadCalibration()` dòng ~229, `saveCalibration()` ~247: NVS `Preferences`, namespace `fordwin`, key `cal_v1`; record gồm magic, version, idle, baseline, margin khi lưu, p10/p90, số mẫu, checksum.
- `checkBootCal()` ~298: chuỗi AUTO DOWN → RELEASE → AUTO UP lúc boot.
- `computeAndSaveCal()` ~343: kiểm tra mẫu, trimmed mean 10%, p10/p90, lưu baseline; không review thủ công.
- `handleCal()` ~373: `CAL_GO_DOWN`, `CAL_WAIT_UP`, `CAL_COLLECT_UP`; khi chạm TOP sẽ gọi tính/lưu, sau đó `exitCal()`.
- `onNormalSwitch()` ~444: UP/DOWN/AUTO; AUTO UP đòi baseline valid.
- `monitorNormal()` ~474: giám sát limits, timeout, manual release, ngưỡng thử AUTO UP.
- `startAP()` ~632; `setup()` ~649; `loop()` ~671.

### Tham số cụ thể hiện trong code gần nhất

```cpp
const int ANTI_PINCH_MARGIN_MV = 90;
const bool ENABLE_EXPERIMENTAL_REVERSE = false;
const unsigned long MAX_RUN_MS = 8000;
const unsigned long REVERSE_MS = 1000;
const unsigned long ACS_IGNORE_START_MS = 350;
const unsigned long ACS_STALL_CONFIRM_MS = 150;
const unsigned long CAL_BOOT_HOLD_MS = 2000;
const unsigned long CAL_BOOT_NEXT_MS = 3000;
const unsigned long CAL_SAMPLE_INTERVAL_MS = 40;
const unsigned long CAL_IGNORE_SAMPLE_START_MS = 550;
const unsigned long CAL_HARD_TIMEOUT_MS = 8000;
const int CAL_MIN_SAMPLES = 25;
const int CAL_MAX_SAMPLES = 210;
const int CAL_MIN_SIGNAL_ABOVE_IDLE_MV = 65;
const int CAL_MAX_P10_P90_SPREAD_MV = 65;
const int CAL_ABORT_ABOVE_IDLE_MV = 285;
const unsigned long CAL_ABORT_CONFIRM_MS = 60;
```

- **Lưu ý:** các điều kiện `CAL_*` và thời gian hiện tại là heuristic thử nghiệm trong code, **không phải các thông số an toàn đã hiệu chuẩn**. Đặc biệt `CAL_ABORT_ABOVE_IDLE_MV` không thay thế chống kẹt độc lập; chu kỳ học CAL UP chưa có anti-pinch được chứng minh.
- Khi AUTO UP nhận `ACS >= saved_baseline + 90 mV` liên tục ~150 ms, sau giai đoạn bỏ qua ~350 ms đầu, firmware gọi `motorStop()` và in log. Tự reverse khi kẹt đang **tắt** dù tên bài đồ án có chống kẹt đảo chiều.
- Web dashboard đọc dữ liệu bằng HTTP GET, không gửi lệnh motor.
- Một số nhánh dùng `delay(150)` và WebServer đồng bộ; kiểm tra độ trễ dừng, ưu tiên limit khi thực tế tích hợp.

## 7. Wi-Fi AP và xem log không USB

- ESP32 phát AP: **SSID** `FORD-WINDOW-DEBUG`, **password hiện tại** `FordWindow!26` (chỉ mật khẩu demo; nên đổi trước khi công khai).
- Điện thoại/laptop kết nối Wi-Fi này và mở **`http://192.168.4.1`**.
- API đọc: `GET /api/status`, `GET /api/logs`.
- Dashboard: trạng thái NORMAL/CAL, motor, switch, điện áp GPIO1/GPIO10, limit TOP/BOTTOM, baseline NVS, margin, threshold, số mẫu, log.
- Log vòng trong RAM 90 dòng; mất sau reset, có thể xem lại các log boot nếu chưa bị ghi đè.
- Với source hiện tại, web server **chỉ xử lý request khi motor STOPPED**, nên **dashboard có thể không cập nhật trong khi motor đang chạy**. Đây là lựa chọn hiện tại để tránh HTTP đồng bộ làm chậm kiểm tra motor; agent nên giữ ưu tiên an toàn/latency, không tùy tiện thay bằng handler chặn vòng lặp.
- AP này **không phải** Wi-Fi client kết nối Python Flask cũ; không cần laptop, router hoặc Internet để xem log.
- Python STT trước đây chạy trên laptop IP `172.20.10.2` (IP có thể đổi). **Hiện firmware anti-pinch không liên lạc với Flask**.

## 8. Nguồn điện, môi trường build và an toàn thử nghiệm

- ESP32 ở trong hộp/mạch và đang được cấp nguồn ngoài. Người dùng ngại cắm USB laptop để xem Serial Monitor vì có thể xung đột đường nguồn 5V. **Không khẳng định hai nguồn chắc chắn gây cháy**, nhưng phải xem đúng sơ đồ board và cách cấp nguồn trước khi nối đồng thời.
- Khi nạp code qua USB: ngắt/kiểm tra nguồn ngoài và nguồn động lực motor theo sơ đồ mạch; không nối trực tiếp 12V vào 5V/3V3; không nối các đường cấp nguồn ngược nhau.
- Phần mềm không thay thế ngắt khẩn cấp độc lập, cầu chì, công tắc hành trình, giới hạn lực/hành trình. Không dùng tay/bộ phận cơ thể làm vật cản để kiểm thử chống kẹt. Đặc biệt **CAL tự hạ và CAL UP khi học baseline chưa được chứng minh có chống kẹt an toàn**; chỉ thực hiện trên đồ gá có che chắn và giám sát.
- Môi trường: Arduino IDE, ESP32-S3 N16R8, Arduino-ESP32 core (cần xác định phiên bản trên máy). `WiFi.h`, `WebServer.h`, `Preferences.h`, `Arduino.h` là dependencies dự kiến; không cần Python cho AP-dashboard.
- Các test đã nêu trong chat là mô phỏng C++ stub; **không có chứng cứ firmware build bằng toolchain thật / nạp board thật / đi đủ chu trình CAL+NORMAL thành công**. Agent phải xây dựng và ghi lại bằng chứng của mình.

## 9. Những vấn đề/giả định agent cần ưu tiên kiểm tra (không được âm thầm suy diễn)

1. **Nguồn code hiện hành**: tìm file .ino thực tế trong repo, kiểm tra diff với `FordWindow_CAL_Normal_WiFi.ino`; đừng ghi đè bản người dùng đang sửa mà không xem diff.
2. **Build thật**: xác định board/core version, compile bằng Arduino CLI/IDE nếu máy đã có; báo rõ nếu môi trường thiếu. Lỗi biên dịch cần sửa ít nhất có thể.
3. **GPIO & công tắc**: xác minh giới hạn switch analog, trạng thái release; điện áp ACS đo tại đúng GPIO10 (sau chia áp); hạn chế giả định VOM=ADC.
4. **Boot gesture**: chỉ nhận CAL tại boot, không bắt nhầm DOWN thủ công; khi boot không kích motor; trường hợp giữ nhầm/nhả sớm không vào CAL; web hiển thị log đúng.
5. **CAL state machine**: hạ tới BOTTOM, chỉ nhận UP thủ công để nâng, nhả UP thì dừng/hủy; chạm TOP thì dừng, tính mẫu và SAVE; không lưu khi lỗi/timeout/thiếu mẫu; giữ baseline trước đó nếu thất bại.
6. **NORMAL behavior**: nút manual phải nhả là dừng; AUTO DOWN vẫn chạy bình thường theo limit; AUTO UP bị chặn nếu chưa có baseline; sau CAL và sau reset đọc baseline NVS và thao tác hoạt động đúng.
7. **Lỗi có thể xảy ra do logic**: biến `lastHandled` static, `requireReleaseForNormal`, resume sau khi CAL, các cạnh chuyển trạng thái, switch ADC nhiễu; kiểm tra dừng motor khi mất/đứt limit, khi công tắc về UNKNOWN, khi boot/reset, timeout.
8. **Anti-pinch**: ngưỡng `baseline + 90 mV` hiện tại chỉ là bài test; xác minh 350 ms đầu bỏ qua, yêu cầu 150 ms liên tục, chu kỳ ADC/loop, hiệu ứng PWM, nhiễu và độ trễ stop. Không gắn nhãn đã đáp ứng tiêu chuẩn an toàn.
9. **Tính hợp lệ thống kê**: yêu cầu tối thiểu 25 mẫu sau bỏ qua 550 ms, tương đương tối thiểu hơn ~1 giây dữ liệu; với hành trình quá ngắn có thể không đủ mẫu. Kiểm tra tải cao gần TOP, số mẫu được trim, p10–p90 tối đa 65 mV; tránh học từ chu kỳ bị vật cản.
10. **Chế độ CAL chưa an toàn đầy đủ**: code có giới hạn timeout, ACS guard dựa idle (285 mV) và limit; đây không phải hệ thống bảo vệ độc lập. Không tự kiểm thử kẹt bằng người; không tự bật reverse.
11. **Web dashboard**: kiểm tra AP, endpoint JSON valid, log còn đủ, không lộ điều khiển motor qua HTTP; không để mạng làm trì hoãn dừng motor.
12. **Không over-engineer**: ưu tiên sửa bug và thực nghiệm có log; không tự thêm framework, database, cloud, WebSocket hay voice vào bản anti-pinch nếu chưa được yêu cầu.

## 10. Tiêu chí hoàn thành / hành vi mong đợi để agent tự đánh giá

| Test | Kết quả mong đợi |
|---|---|
| Boot không thao tác CAL | vào NORMAL; motor không tự chạy |
| Giữ DOWN thủ công 5+ giây trong NORMAL | chỉ hạ khi đang giữ; **không vào CAL** |
| AUTO DOWN → RELEASE → AUTO UP đúng lúc boot | CAL được kích hoạt, log rõ |
| CAL tự hạ chạm BOTTOM | dừng ở BOTTOM, chuyển WAIT_UP |
| CAL giữ UP đến TOP | dừng tại TOP, tính và lưu baseline nếu mẫu hợp lệ |
| CAL thả UP giữa chừng | dừng ngay, hủy CAL, **không ghi đè baseline cũ** |
| CAL fail timeout/mất mẫu/lỗi limit | STOP, log lý do, không ghi dữ liệu sai |
| CAL thành công rồi nhả switch | trở lại NORMAL, không yêu cầu reset |
| Sau power cycle | baseline NVS còn, không phải CAL lại |
| Thay `ANTI_PINCH_MARGIN_MV` từ 90 | threshold hiện bằng baseline đã lưu + margin mới (nếu NVS chưa xóa) |
| Chưa có baseline hợp lệ | AUTO UP bị chặn |
| Điện áp bình thường / vượt threshold (test mô phỏng) | tính đúng ngưỡng; khi vượt đủ thời gian thì STOP + LOG; **reverse = OFF** |
| Kết nối Wi-Fi AP, web | xem `mode`, `motor`, `acs_mv`, `baseline_mv`, `threshold_mv`, logs; không cần cắm USB |

## 11. Cách giao việc cho Antigravity ngay sau khi thêm file vào repo

Copy câu lệnh này vào Antigravity:

> Hãy đọc `ANTIGRAVITY_CONTEXT_FordTerritory_AntiPinch.md` trong workspace trước. Sau đó tìm firmware `.ino` hiện hành của đồ án Ford Territory, **đọc source thực tế, không giả định các tính năng đã chạy trên board**. Ưu tiên kiểm tra `FordWindow_CAL_Normal_WiFi.ino`, đối chiếu với sketch gốc nếu có; lập danh sách bug/risk có dẫn vị trí code. Nếu máy có Arduino IDE hoặc `arduino-cli`, hãy **compile thực tế** và chạy các unit/simulation test phần state machine mà không cần kích motor. Hãy sửa lỗi phần mềm thực sự tìm được một cách tối thiểu, giải thích patch/diff. **Không tự nạp firmware, cấp điện, đảo dây, chạy motor, kích CAL tự động, kiểm thử lực kẹp hoặc bật tính năng đảo chiều** nếu chưa được tôi đồng ý và điều kiện phần cứng chưa an toàn. Cuối cùng báo lại trạng thái: đã kiểm tra gì, pass/fail, log lỗi, file đã sửa và bước thử thủ công tiếp theo. Giải thích ngắn gọn bằng tiếng Việt, không over-engineer.

### Cách bố trí workspace gợi ý

```text
<your-repo>/
├── ANTIGRAVITY_CONTEXT_FordTerritory_AntiPinch.md
├── firmware/                           # tùy cấu trúc repo hiện tại
│   └── FordWindow_CAL_Normal_WiFi.ino
└── ...                                 # phần Python/voice trong folder riêng nếu có
```

Không bắt buộc đổi cấu trúc repo nếu người dùng đã có sẵn. Nếu gói handoff có source đi kèm, **so sánh source trước khi copy đè**.

## 12. Những câu hỏi còn bỏ ngỏ, chỉ hỏi khi thực sự cần quyết định/test phần cứng

- Mạch nguồn cụ thể: ESP32 nhận 5V từ buck vào pin 5V, 3.3V, hay USB? Board ESP32-S3 N16R8 chính xác hãng/PCB nào? Có mạch OR-ing/cách ly nguồn không?
- ACS712 loại 5A/20A/30A? Nối ở dây nguồn BTS7960 hay nối tiếp dây motor? Điện trở chia áp bao nhiêu? VOM đo OUT cảm biến hay chân GPIO10?
- Công tắc hành trình NC/NO thực tế có đúng giả định LOW/HIGH không? Dữ liệu ADC GPIO1 trong cả 5 trạng thái công tắc?
- Thời gian hành trình kính thật bao lâu, có đủ >=25 mẫu sau 550 ms không? Mức ADC (không phải VOM) chạy bình thường và khi tải thay đổi?
- Có E-stop ngắt nguồn motor, cầu chì và đồ gá đo lực bảo vệ khi thử nghiệm chưa?

## 13. Nói ngắn gọn một lần nữa — trạng thái bàn giao

- **Đã thảo luận và có trong source mới:** CAL bằng tổ hợp lúc boot, CAL auto DOWN → giữ UP để học → auto SAVE → NORMAL, NVS, AP/dashboard, `baseline + 90 mV` cho cảnh báo/dừng AUTO UP.
- **Chưa có trong bản source đang bàn giao:** anti-pinch đã kiểm chứng trên cơ cấu thật; tự đảo chiều được bật; điều khiển giọng nói trong cùng firmware; kết quả đo ADC/force test có log thật.
- **Mong muốn tiếp theo:** Antigravity kiểm tra/bắt lỗi/compile firmware trên máy người dùng và chỉ sau đó, với người giám sát và hệ thống an toàn, mới đi kiểm thử thực tế; ưu tiên sửa đúng yêu cầu, ít phát sinh thao tác.
