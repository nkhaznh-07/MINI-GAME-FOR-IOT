# 🎮 MINI GAME CONSOLE & WIRELESS POWERPOINT PRESENTER

Hệ thống máy chơi game cầm tay đa năng kết hợp bút trình chiếu PowerPoint không dây sử dụng vi điều khiển **ESP32-S3** (tay cầm trung tâm) và **ESP32-C3 SuperMini** (bộ thu phát cầu nối USB-PC qua giao thức **ESP-NOW**). Nâng cấp toàn diện từ phiên bản V2 với âm thanh Buzzer không chặn, kết nối Wi-Fi/Web Gamepad 2 người chơi, bàn phím ảo trên màn hình TFT (`VirtualKeyboard-FOR-ESP32`) và tính năng điều khiển thuyết trình xem trước Slide thời gian thực.

---

## 📌 Tính năng nổi bật

- **4 Trò chơi kinh điển (Hỗ trợ 2 người chơi qua Web):**
  - 🐍 **SNAKE 2P (Rắn săn mồi):** Hỗ trợ tối đa 2 người chơi cùng lúc trên một màn hình (Player 1 điều khiển qua Joystick/Web, Player 2 tự động tham gia qua trình duyệt điện thoại).
  - 🧩 **TETRIS (Xếp hình):** Đầy đủ 7 khối gạch (I, O, T, S, Z, J, L), hỗ trợ rơi nhanh (*Soft drop*) và xóa hàng mượt mà.
  - 🟡 **PAC-MAN:** Mê cung chuẩn, Ma Đỏ tự động truy đuổi bằng thuật toán tìm kiếm theo chiều rộng (**BFS**).
  - 🧱 **BREAKOUT (Bắn gạch):** Tính toán góc nảy vật lý của bóng theo vị trí va chạm trên thanh đỡ.
- **Trình điều khiển PowerPoint không dây (ESP-NOW):**
  - Truyền hình ảnh xem trước **Slide hiện tại**, **Slide kế tiếp** (RGB565) và **Nội dung ghi chú tiếng Việt UTF-8** từ máy tính về màn hình cầm tay độ trễ thấp.
  - Hỗ trợ xoay linh hoạt giữa chế độ **Ngang (Landscape)** toàn màn hình và **Dọc (Portrait)** chia đôi màn hình (trên xem Slide hiện tại, dưới xem ghi chú hoặc Slide tiếp theo).
  - Tích hợp **LED trạng thái trên ESP32-C3 (GPIO 8)** báo hiệu trực quan tình trạng kết nối 2 chiều giữa Máy tính ↔ C3 ↔ S3.
  - Tự động ngắt hoàn toàn âm thanh Buzzer khi vào chế độ thuyết trình để giữ im lặng tuyệt đối.
- **Kết nối Wi-Fi, Bàn phím ảo trên màn hình & Tay cầm Web 2 Người Chơi:**
  - Hỗ trợ cả 2 chế độ **Wi-Fi STA** (quét mạng và nhập mật khẩu trực tiếp bằng thư viện **[VirtualKeyboard-FOR-ESP32](https://github.com/nkhaznh-07/VirtualKeyboard-FOR-ESP32)**) và **Access Point** (`MiniGameConsole_V3`).
  - Giao diện tay cầm điều khiển cảm ứng trực quan trên điện thoại/máy tính không cần cài đặt ứng dụng.
- **Hệ thống Âm thanh Buzzer không chặn (Non-blocking PWM):**
  - Nhạc nền riêng biệt cho từng tựa game và hiệu ứng âm thanh thao tác (Click, Ăn điểm, Thắng/Thua).
  - Tùy chỉnh Bật/Tắt nhạc nền và 10 mức âm lượng trực tiếp trong menu **SETTING**.
- **Hệ thống Cấp độ & Lưu trữ NVS:**
  - **10 cấp độ (Level 1 - 10)** cho từng game với tốc độ và màu sắc khung viền biến đổi theo cấp độ.
  - Tự động lưu Điểm cao (*High Score*), Cấp độ, Cài đặt âm thanh và Thông tin Wi-Fi vào bộ nhớ Flash thông qua `Preferences`.
- **Module Điều khiển Linh hoạt:** Dễ dàng chuyển đổi giữa **Joystick Analog 2 trục** và **Cụm 5 nút bấm vật lý rời** chỉ bằng một cờ cấu hình (`USE_BUTTON_PAD`).

---

## 🛠️ Phần cứng yêu cầu

| Linh kiện | Chi tiết / Thông số |
| :--- | :--- |
| **Vi điều khiển tay cầm** | ESP32-S3 (Khuyến nghị bản N16R8 hoặc DevKit tương đương) |
| **Vi điều khiển cầu nối PC** | ESP32-C3 SuperMini (Cắm cổng USB máy tính) |
| **Màn hình hiển thị** | ST7735 1.8 inch TFT LCD (Độ phân giải 128x160, chuẩn SPI 40 MHz) |
| **Bộ điều khiển** | PS2 Dual-Axis Joystick (2 trục Analog + phím SW) hoặc 5 nút nhấn rời |
| **Âm thanh** | Còi chip thụ động (Passive Buzzer 3.3V - 5V) |
| **Khác** | Pin Li-ion 18650 / mạch sạc, testboard, dây nối Dupont |

---

## 🔌 Sơ đồ kết nối dây (Pinout)

### 1. Màn hình ST7735 TFT $\leftrightarrow$ ESP32-S3
| Chân ST7735 | Chân ESP32-S3 | Chú thích |
| :--- | :--- | :--- |
| **CS** | GPIO 10 | Chip Select |
| **DC / RS** | GPIO 9 | Data / Command |
| **RST / RES** | GPIO 8 | Reset |
| **SDA / MOSI** | GPIO 11 | SPI Data |
| **SCL / SCK** | GPIO 12 | SPI Clock (Tốc độ 40 MHz) |
| **VCC / LED** | 3.3V | Nguồn cấp & Đèn nền |
| **GND** | GND | Mass |

### 2. Bộ điều khiển & Buzzer $\leftrightarrow$ ESP32-S3
| Linh kiện / Chân | Chân ESP32-S3 | Chú thích |
| :--- | :--- | :--- |
| **Joystick VRx** | GPIO 4 | Trục X Analog (ADC 12-bit) |
| **Joystick VRy** | GPIO 5 | Trục Y Analog (ADC 12-bit) |
| **Joystick SW** | GPIO 6 | Phím nhấn chọn (Internal Pull-up) |
| **Buzzer (+)** | GPIO 7 | Tín hiệu PWM âm thanh (`LEDC`) |
| **VCC / GND** | 3.3V / GND | Nguồn cấp chung |

> **Tùy chọn 5 nút bấm rời (`#define USE_BUTTON_PAD 1`):**
> `UP`: GPIO 4 | `DOWN`: GPIO 5 | `LEFT`: GPIO 15 | `RIGHT`: GPIO 16 | `OK`: GPIO 6.

### 3. Mạch cầu nối ESP32-C3 SuperMini
- Kết nối trực tiếp với máy tính qua cáp **USB-C** (Giao tiếp Serial Baudrate **460800**).
- Sử dụng **LED Onboard tại GPIO 8** (Active-LOW) để hiển thị trạng thái kết nối.

---

## 📚 Thư viện phụ thuộc (Dependencies)

Cài đặt các thư viện sau qua **Library Manager** hoặc thêm trực tiếp từ GitHub vào thư mục `Arduino/libraries`:

1. **Adafruit GFX Library** (`Adafruit_GFX.h`)
2. **Adafruit ST7735 and ST7739 Library** (`Adafruit_ST7735.h`)
3. **U8g2_for_Adafruit_GFX** (`U8g2_for_Adafruit_GFX.h` - Hiển thị font tiếng Việt UTF-8)
4. **[VirtualKeyboard-FOR-ESP32](https://github.com/nkhaznh-07/VirtualKeyboard-FOR-ESP32)** (`VirtualKeyboard.h` - Thư viện bàn phím ảo đa bố cục gọn nhẹ tối ưu cho màn hình TFT nhỏ và điều khiển bằng Joystick):
   - Tải mã nguồn từ: `https://github.com/nkhaznh-07/VirtualKeyboard-FOR-ESP32`
   - Giải nén vào `Documents/Arduino/libraries/VirtualKeyboard-FOR-ESP32` (hoặc cài qua *Sketch $\rightarrow$ Include Library $\rightarrow$ Add .ZIP Library...*).
   - Tích hợp thông qua callback `vkb.begin(160, 128, 37, vkbDrawCallback)` và chuyển đổi linh hoạt 4 bộ ký tự: `MODE_LOWERCASE` (chữ thường), `MODE_UPPERCASE` (chữ hoa), `MODE_NUMBERS` (số) và `MODE_SYMBOLS` (ký tự đặc biệt).
5. **Thư viện lõi ESP32:** `WiFi.h`, `WebServer.h`, `esp_now.h`, `esp_wifi.h`, `Preferences.h`, `mbedtls/base64.h`

---

## 🕹️ Hướng dẫn sử dụng & Điều khiển

### 1. Menu chính & Cài đặt (`SETTING`)
- **Gạt Y (Lên / Xuống):** Di chuyển qua các mục: `SNAKE`, `TETRIS`, `PAC-MAN`, `BREAKOUT`, `POWERPOINT`, `SETTING`.
- **Nhấn ngắn SW:** Truy cập mục đang chọn.
- **Trong mục `SETTING`:**
  - **WIFI:** Quét mạng Wi-Fi xung quanh, nhập mật khẩu bằng **VirtualKeyboard** trên màn hình ngang hoặc bật **Access Point** (`MiniGameConsole_V3` / Pass: `12345678`) để lấy địa chỉ IP truy cập tay cầm Web trên điện thoại.
  - **SOUND:** Bật/Tắt nhạc nền và gạt **Trái / Phải** để chỉnh âm lượng (0% - 100%).
  - **RESET x5:** Nhấn ngắn 5 lần liên tiếp (trong vòng 3 giây) để xóa toàn bộ điểm cao, cấp độ và cấu hình đã lưu.

### 2. Nhập mật khẩu Wi-Fi với `VirtualKeyboard`
- **Gạt Joystick (4 hướng):** Di chuyển khung chọn màu vàng qua từng phím trên giao diện ngang `160x128`.
- **Nhấn ngắn SW:**
  - Chọn ký tự hiện tại để nhập vào ô mật khẩu.
  - Phím `^`: Chuyển đổi chữ hoa / chữ thường (`MODE_UPPERCASE` / `MODE_LOWERCASE`).
  - Phím `123` / `#+=` / `ABC`: Chuyển nhanh giữa bàn phím số, ký tự đặc biệt và chữ cái.
  - Phím `<X`: Xóa 1 ký tự vừa nhập.
  - Phím `Nhap`: Xác nhận mật khẩu và bắt đầu kết nối Wi-Fi.
- **Nhấn giữ SW (> 1 giây):** Xóa nhanh 1 ký tự (hoặc thoát bàn phím nếu ô nhập đang trống).

### 3. Trong khi chơi Game
- **Joystick (X / Y):** Điều khiển nhân vật, xoay khối Tetris (Gạt lên) hoặc rơi nhanh (Gạt xuống).
- **Nhấn giữ SW (> 1 giây):** Lưu cấp độ hiện tại và thoát về Menu chính.
- **Chế độ 2 người chơi (Web Gamepad):** Truy cập địa chỉ IP hiển thị trong mục `WIFI SETTING` trên trình duyệt điện thoại, chọn **Player 1** hoặc **Player 2** ở góc trên để cùng chơi Snake.

### 4. Chế độ Thuyết trình (`POWERPOINT`)
- **Gạt Trái / Phải (màn ngang) hoặc Lên / Xuống (màn dọc):** Chuyển Slide trước (`PREV`) / Slide kế tiếp (`NEXT`).
- **Nhấn đơn SW (1 lần):** Chuyển đổi nhanh giữa xem **Ảnh Slide hiện tại** và **Nội dung văn bản ghi chú** (khi ở chế độ xem chữ, gạt cần để cuộn dòng).
- **Nhấn đúp SW (2 lần liên tiếp):** Xem trước **Slide kế tiếp (Next Slide)**.
- **Nhấn giữ SW (~0.6 giây):** Chuyển đổi giao diện hiển thị **Ngang (Landscape)** $\leftrightarrow$ **Dọc (Portrait)**.
- **Nhấn giữ SW (> 2.2 giây):** Thoát chế độ PowerPoint và quay về Menu chính.

### 5. Bảng trạng thái LED GPIO 8 trên ESP32-C3
| Nhịp nháy LED (GPIO 8) | Trạng thái hệ thống |
| :--- | :--- |
| **Chớp 1 nhịp ngắn mỗi 1.5s** | Chế độ chờ (Chưa chạy script Python và tay cầm S3 chưa mở PowerPoint). |
| **Chớp đôi (2 nhịp) mỗi 1.2s** | Đã kết nối máy tính (Python đang chạy), đang chờ tay cầm S3 vào mục `POWERPOINT`. |
| **Chớp 3 nhịp ngắn mỗi 1.5s** | Tay cầm S3 đã sẵn sàng, đang chờ chạy script đồng bộ trên máy tính. |
| **Sáng đứng liên tục (Solid ON)** | Kết nối thông suốt 2 chiều (**Máy tính $\leftrightarrow$ ESP32-C3 $\leftrightarrow$ ESP32-S3**). |
| **Nháy nhanh liên tục** | Đang truyền dữ liệu hình ảnh/văn bản Slide hoặc vừa nhận lệnh chuyển trang. |

---

## ⚙️ Cấu hình nạp code (Arduino IDE)

### 1. Cấu hình cho ESP32-S3 (Tay cầm)
- **Board:** `ESP32S3 Dev Module`
- **USB CDC On Boot:** `Enabled`
- **Flash Size:** `16MB (128Mb)` *(hoặc `4MB` tùy phiên bản mạch)*
- **Partition Scheme:** `Huge APP (3MB No OTA / 1MB SPIFFS)`
- **PSRAM:** `OPI PSRAM` *(hoặc `Disabled` nếu dùng bản không có PSRAM)*
- **Baudrate Serial Monitor:** `115200`

### 2. Cấu hình cho ESP32-C3 SuperMini (Cầu nối PC)
- **Board:** `ESP32C3 Dev Module`
- **USB CDC On Boot:** `Enabled` *(Bắt buộc bật để giao tiếp dữ liệu qua cổng USB-C)*
- **Flash Size:** `4MB (32Mb)`
- **Baudrate giao tiếp Python:** `460800`

---

## 📝 Giấy phép (License)

Dự án được chia sẻ công khai phục vụ mục đích học tập, nghiên cứu hệ thống nhúng IoT và phát triển cá nhân.