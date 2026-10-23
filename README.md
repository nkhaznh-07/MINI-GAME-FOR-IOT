# 🎮 MINI GAME CONSOLE V2

Dự án máy chơi game cầm tay mini cổ điển sử dụng vi điều khiển **ESP32-S3** kết hợp màn hình hiển thị **ST7735 TFT 1.8"** và **Joystick 2 trục (PS2)**. Tích hợp 4 trò chơi kinh điển, hệ thống lưu điểm cao, cấp độ khó tăng dần và giao diện trực quan.

---

## 📌 Tính năng nổi bật

- **4 Trò chơi kinh điển:**
  - 🐍 **SNAKE (Rắn săn mồi):** Tối ưu hóa render cell, tốc độ tăng theo cấp độ.
  - 🧩 **TETRIS (Xếp hình):** Đầy đủ 7 khối gạch (I, O, T, S, Z, J, L), có tính năng thả nhanh (Soft drop) và xóa hàng.
  - 🟡 **PAC-MAN:** Hệ thống mê cung chuẩn, Ma Đỏ tự động tìm đường đuổi theo Pac-Man bằng thuật toán tìm kiếm theo chiều rộng (**BFS**).
  - 🧱 **BREAKOUT (Bắn gạch):** Điều khiển thanh đỡ, tính toán góc nảy vật lý của bóng khi va chạm với các hàng gạch nhiều màu.
- **Hệ thống Cấp độ & Độ khó (Difficulty System):**
  - Hỗ trợ **10 cấp độ (Level 1 - 10)** cho từng trò chơi riêng biệt.
  - Tốc độ trò chơi và độ khó tăng dần theo cấp độ.
  - Khung viền trang trí màn hình thay đổi màu sắc và họa tiết độc đáo theo từng cấp độ.
- **Lưu trữ dữ liệu (Flash Storage):** Tự động lưu Điểm cao nhất (*High Score*) và Cấp độ hiện tại (*Level*) vào bộ nhớ NVS (Non-Volatile Storage) thông qua thư viện `Preferences`.
- **Cơ chế điều khiển thông minh:**
  - Tự động cân chỉnh tâm Joystick (Auto-Calibration) khi khởi động.
  - Xử lý chống rung phím (Debounce) và phân biệt nhấn ngắn (Short Press) / nhấn giữ (Long Press).
- **Tính năng An toàn & Reset:** Cơ chế nhấn phím Reset 5 lần trong menu để xóa sạch dữ liệu điểm cao khi cần.

---

## 🛠️ Phần cứng yêu cầu

| Linh kiện | Chi tiết / Thông số |
| :--- | :--- |
| **Vi điều khiển** | ESP32-S3 (hoặc các dòng ESP32 tương đương) |
| **Màn hình** | ST7735 1.8 inch TFT LCD (Độ phân giải 128x160, chuẩn SPI) |
| **Bộ điều khiển** | PS2 Dual-Axis Joystick (2 Trục Anaglog + 1 Phím nhấn SW) |
| **Khác** | Testboard, dây nối DUPONT |

---

## 🔌 Sơ đồ kết nối dây (Pinout)

### 1. Kết nối Màn hình ST7735 TFT
| Chân ST7735 | Chân ESP32-S3 | Chú thích |
| :--- | :--- | :--- |
| **CS** | GPIO 10 | Chip Select |
| **DC / RS** | GPIO 9 | Data / Command |
| **RST / RES**| GPIO 8 | Reset |
| **SDA / MOSI**| GPIO 11 | SPI Data |
| **SCL / SCK** | GPIO 12 | SPI Clock (Tốc độ 27 MHz) |
| **VCC** | 3.3V / 5V | Nguồn cấp |
| **GND** | GND | Mass |

### 2. Kết nối Joystick (PS2)
| Chân Joystick | Chân ESP32-S3 | Chú thích |
| :--- | :--- | :--- |
| **VRx** | GPIO 4 | Trục X Analog (ADC) |
| **VRy** | GPIO 5 | Trục Y Analog (ADC) |
| **SW** | GPIO 6 | Phím nhấn Joystick (Pull-up) |
| **VCC** | 3.3V | Nguồn cấp |
| **GND** | GND | Mass |

---

## 📚 Thư viện phụ thuộc (Dependencies)

Cài đặt các thư viện sau thông qua **Library Manager** trên Arduino IDE hoặc PlatformIO:

1. **Adafruit GFX Library** (`Adafruit_GFX.h`)
2. **Adafruit ST7735 and ST7739 Library** (`Adafruit_ST7735.h`)
3. **Preferences** (Tích hợp sẵn trong ESP32 Arduino Core)
4. **SPI** & **Math** (Thư viện chuẩn C++/Arduino)

---

## 🕹️ Hướng dẫn điều khiển

### 1. Trong Menu chính
- **Gạt Y (Lên / Xuống):** Di chuyển con trỏ chọn game hoặc mục Reset.
- **Nhấn ngắn SW:**
  - Chọn game để chơi.
  - Nhấn 5 lần liên tiếp ở mục **RESET x5** (trong vòng 3 giây) để xóa toàn bộ kỷ lục.

### 2. Trong khi chơi Game
- **Joystick (X/Y):** Di chuyển nhân vật / thanh đỡ / hướng xoay khối gạch.
- **Nhấn giữ SW (> 1.2 giây):** Thoát game khẩn cấp và quay trở lại Menu chính.
- **Tetris:** Gạt Y xuống để rơi nhanh (*Soft Drop*), gạt Y lên để xoay khối gạch.

### 3. Màn hình Kết quả (Result Screen)
- **Gạt Y (Lên / Xuống):** Chuyển đổi giữa **NEXT LEVEL / PLAY AGAIN** và **BACK TO MENU**.
- **Nhấn ngắn SW:** Xác nhận lựa chọn (Nếu thắng, hệ thống sẽ tự động tăng Cấp độ ở lượt chơi tiếp theo).

---

## ⚙️ Cấu hình môi trường nạp code (Arduino IDE)

1. Thêm URL Bo mạch ESP32 vào Arduino IDE:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Chọn Board: **ESP32S3 Dev Module** (hoặc board ESP32 tương ứng bạn đang dùng).
3. Thiết lập thông số:
   - **Flash Size:** 4MB (hoặc cao hơn)
   - **Partition Scheme:** Default 4MB with spiffs / Huge APP (3MB No OTA)
   - **PSRAM:** Disabled (hoặc Enabled tùy dòng chip)
4. Mở Serial Monitor ở tốc độ Baudrate: **115200**.

---

## 📝 Giấy phép (License)

Dự án này được chia sẻ công khai cho mục đích học tập và nghiên cứu cá nhân.
