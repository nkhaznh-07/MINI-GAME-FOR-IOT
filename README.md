# 🎮 MINI GAME CONSOLE V2

Dự án máy chơi game cầm tay mini cổ điển sử dụng vi điều khiển **ESP32-S3** kết hợp màn hình hiển thị **ST7735 TFT 1.8"** và **Joystick 2 trục (PS2)**. Tích hợp 4 trò chơi kinh điển, hệ thống bàn phím ảo nhập liệu, lưu điểm cao, cấp độ khó tăng dần và giao diện trực quan.

## 📋 Mục Lục

* [📌 Tính năng nổi bật](#-tính-năng-nổi-bật)
* [🛠️ Phần cứng yêu cầu](#️-phần-cứng-yêu-cầu)
* [🔌 Sơ đồ kết nối dây (Pinout)](#-sơ-đồ-kết-nối-dây-pinout)
* [📚 Thư viện & Module phụ thuộc](#-thư-viện--module-phụ-thuộc)
* [⚙️ Cài đặt & Cấu hình nạp code](#️-cài-đặt--cấu-hình-nạp-code)
* [🕹️ Hướng dẫn điều khiển](#️-hướng-dẫn-điều-khiển)
* [🤝 Đóng góp (Contributing)](#-đóng-góp-contributing)
* [📝 Giấy phép (License)](#-giấy-phép-license)
* [📞 Liên hệ](#-liên-hệ)

## 📌 Tính năng nổi bật

* **4 Trò chơi kinh điển:**
  * 🐍 **SNAKE (Rắn săn mồi):** Tối ưu hóa render cell, tốc độ tăng theo cấp độ.
  * 🧩 **TETRIS (Xếp hình):** Đầy đủ 7 khối gạch (`I`, `O`, `T`, `S`, `Z`, `J`, `L`), có tính năng thả nhanh (Soft drop) và xóa hàng.
  * 🟡 **PAC-MAN:** Hệ thống mê cung chuẩn, Ma Đỏ tự động tìm đường đuổi theo Pac-Man bằng thuật toán tìm kiếm theo chiều rộng (**BFS**).
  * 🧱 **BREAKOUT (Bắn gạch):** Điều khiển thanh đỡ, tính toán góc nảy vật lý của bóng khi va chạm với các hàng gạch nhiều màu.
* **Bàn phím ảo tích hợp (`VirtualKeyboard`):**
  * Hiển thị bàn phím ký tự trực quan ngay trên màn hình ST7735, cho phép người chơi dùng Joystick để di chuyển con trỏ và nhập tên người chơi/dữ liệu khi lập kỷ lục mới.
  * Được phát triển và tùy biến dựa trên dự án mã nguồn mở: [VirtualKeyboard-FOR-ESP32](https://github.com/nkhaznh-07/VirtualKeyboard-FOR-ESP32).
* **Hệ thống Cấp độ & Độ khó (Difficulty System):**
  * Hỗ trợ **10 cấp độ (Level 1 - 10)** cho từng trò chơi riêng biệt.
  * Tốc độ trò chơi và độ khó tăng dần theo cấp độ.
  * Khung viền trang trí màn hình thay đổi màu sắc và họa tiết độc đáo theo từng cấp độ.
* **Lưu trữ dữ liệu (Flash Storage):** Tự động lưu Điểm cao nhất (*High Score*), tên người chơi và Cấp độ hiện tại (*Level*) vào bộ nhớ NVS (Non-Volatile Storage) thông qua thư viện `Preferences`.
* **Cơ chế điều khiển thông minh:**
  * Tự động cân chỉnh tâm Joystick (*Auto-Calibration*) khi khởi động.
  * Xử lý chống rung phím (*Debounce*) và phân biệt nhấn ngắn (*Short Press*) / nhấn giữ (*Long Press*).
* **Tính năng An toàn & Reset:** Cơ chế nhấn phím Reset 5 lần trong menu để xóa sạch dữ liệu điểm cao khi cần.

*(Mẹo: Bạn có thể chèn ảnh chụp thực tế mạch hoặc video/GIF lúc đang chơi game tại đây để dự án thêm sinh động!)*

## 🛠️ Phần cứng yêu cầu

| Linh kiện | Chi tiết / Thông số |
| :--- | :--- |
| **Vi điều khiển** | ESP32-S3 (hoặc các dòng ESP32 tương đương) |
| **Màn hình** | ST7735 1.8 inch TFT LCD (Độ phân giải 128x160, chuẩn SPI) |
| **Bộ điều khiển** | PS2 Dual-Axis Joystick (2 Trục Analog + 1 Phím nhấn SW) |
| **Khác** | Testboard (Breadboard), dây nối DUPONT |

## 🔌 Sơ đồ kết nối dây (Pinout)

### 1. Kết nối Màn hình ST7735 TFT

| Chân ST7735 | Chân ESP32-S3 | Chú thích |
| :--- | :--- | :--- |
| **CS** | `GPIO 10` | Chip Select |
| **DC / RS** | `GPIO 9` | Data / Command |
| **RST / RES** | `GPIO 8` | Reset |
| **SDA / MOSI** | `GPIO 11` | SPI Data |
| **SCL / SCK** | `GPIO 12` | SPI Clock (Tốc độ 27 MHz) |
| **VCC** | `3.3V / 5V` | Nguồn cấp |
| **GND** | `GND` | Mass |

### 2. Kết nối Joystick (PS2)

| Chân Joystick | Chân ESP32-S3 | Chú thích |
| :--- | :--- | :--- |
| **VRx** | `GPIO 4` | Trục X Analog (ADC) |
| **VRy** | `GPIO 5` | Trục Y Analog (ADC) |
| **SW** | `GPIO 6` | Phím nhấn Joystick (Pull-up) |
| **VCC** | `3.3V` | Nguồn cấp |
| **GND** | `GND` | Mass |

## 📚 Thư viện & Module phụ thuộc

Cài đặt các thư viện sau thông qua **Library Manager** trên Arduino IDE hoặc PlatformIO:

1. **Adafruit GFX Library** (`Adafruit_GFX.h`)
2. **Adafruit ST7735 and ST7739 Library** (`Adafruit_ST7735.h`)
3. **Preferences** (Tích hợp sẵn trong ESP32 Arduino Core)
4. **SPI** & **Math** (Thư viện chuẩn C++/Arduino)
5. **Module `VirtualKeyboard`:** Tích hợp trực tiếp trong mã nguồn, phát triển dựa trên repository [VirtualKeyboard-FOR-ESP32](https://github.com/nkhaznh-07/VirtualKeyboard-FOR-ESP32) bởi [@nkhaznh-07](https://github.com/nkhaznh-07).

## ⚙️ Cài đặt & Cấu hình nạp code

### 1. Tải mã nguồn

Clone repository này về máy tính của bạn:

```bash
git clone https://github.com/nkhaznh-07/mini-game-console-v2.git
cd mini-game-console-v2
```

### 2. Cấu hình Arduino IDE

1. Thêm URL Bo mạch ESP32 vào **File > Preferences > Additional Boards Manager URLs**:
   ```text
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. Vào **Tools > Board**, chọn **ESP32S3 Dev Module** (hoặc board ESP32 tương ứng bạn đang dùng).
3. Thiết lập thông số nạp:
   * **Flash Size:** `4MB` (hoặc cao hơn)
   * **Partition Scheme:** `Default 4MB with spiffs` / `Huge APP (3MB No OTA)`
   * **PSRAM:** `Disabled` (hoặc `Enabled` tùy dòng chip)
4. Kết nối cáp USB, chọn đúng cổng **COM** và bấm **Upload**.
5. Mở **Serial Monitor** ở tốc độ Baudrate: `115200` để theo dõi thông số khởi động và cân chỉnh Joystick.

## 🕹️ Hướng dẫn điều khiển

### 1. Trong Menu chính

* **Gạt Y (Lên / Xuống):** Di chuyển con trỏ chọn game hoặc mục Reset.
* **Nhấn ngắn SW:**
  * Chọn game để bắt đầu chơi.
  * Nhấn **5 lần liên tiếp** ở mục **RESET x5** (trong vòng 3 giây) để xóa toàn bộ kỷ lục điểm cao.

### 2. Trong khi chơi Game

* **Joystick (X/Y):** Di chuyển nhân vật / thanh đỡ / hướng xoay khối gạch.
* **Nhấn giữ SW (> 1.2 giây):** Thoát game khẩn cấp và quay trở lại Menu chính.
* **Riêng game Tetris:** Gạt Y xuống để rơi nhanh (*Soft Drop*), gạt Y lên để xoay khối gạch.

### 3. Trên Bàn phím ảo (`VirtualKeyboard`)

* **Gạt X / Y (Trái / Phải / Lên / Xuống):** Di chuyển khung chọn giữa các phím ký tự (`A-Z`, `0-9`, `DEL`, `OK`).
* **Nhấn ngắn SW:** Chọn ký tự đang trỏ tới hoặc xác nhận hoàn tất nhập liệu.

### 4. Màn hình Kết quả (Result Screen)

* **Gạt Y (Lên / Xuống):** Chuyển đổi giữa **NEXT LEVEL / PLAY AGAIN** và **BACK TO MENU**.
* **Nhấn ngắn SW:** Xác nhận lựa chọn (Nếu thắng, hệ thống sẽ tự động tăng Cấp độ ở lượt chơi tiếp theo).

## 🤝 Đóng góp (Contributing)

Mọi ý tưởng nâng cấp game mới, tối ưu hóa khung hình (FPS) hoặc thiết kế vỏ in 3D cho máy chơi game đều được hoan nghênh!

1. Fork dự án này.
2. Tạo một nhánh mới (`git checkout -b feature/TenTinhNangMoi`).
3. Commit các thay đổi của bạn (`git commit -m 'Thêm tính năng mới'`).
4. Push lên nhánh vừa tạo (`git push origin feature/TenTinhNangMoi`).
5. Tạo một **Pull Request**.

## 📝 Giấy phép (License)

Dự án này được chia sẻ công khai cho mục đích học tập và nghiên cứu cá nhân.

## 📞 Liên hệ

* **Tác giả:** nkhaznh-07
* **GitHub:** [@nkhaznh-07](https://github.com/nkhaznh-07)
* **Dự án liên quan:** [VirtualKeyboard-FOR-ESP32](https://github.com/nkhaznh-07/VirtualKeyboard-FOR-ESP32)
