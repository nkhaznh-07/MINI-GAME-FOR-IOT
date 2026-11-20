import os
import sys
import time
import base64
import tempfile
import threading
import subprocess

if not getattr(sys, "frozen", False):
    for mod, pkg in {"serial": "pyserial", "win32com": "pywin32", "PIL": "Pillow"}.items():
        try:
            __import__(mod)
        except ImportError:
            print(f"[*] Đang cài đặt thư viện {pkg}...")
            subprocess.check_call([sys.executable, "-m", "pip", "install", pkg])

import serial
import serial.tools.list_ports
import pythoncom
import win32com.client
from PIL import Image

BAUD_RATE = 460800
IMG_W = 158
IMG_H = 126
ESP_VIDS = {0x303A, 0x1A86, 0x10C4}

def find_device_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if p.vid in ESP_VIDS or "USB JTAG" in p.description or "ESP32" in p.description:
            return p.device
    return ports[0].device if len(ports) == 1 else None

def extract_shape_text(shape):
    texts = []
    try:
        if shape.HasTextFrame and shape.TextFrame.HasText:
            raw = shape.TextFrame.TextRange.Text.strip()
            if raw:
                texts.append(raw.replace("\r", "\n"))
        if shape.HasTable:
            table = shape.Table
            for r in range(1, table.Rows.Count + 1):
                row_items = []
                for c in range(1, table.Columns.Count + 1):
                    cell_txt = table.Cell(r, c).Shape.TextFrame.TextRange.Text.strip()
                    if cell_txt:
                        row_items.append(cell_txt.replace("\r", " "))
                if row_items:
                    texts.append(" | ".join(row_items))
        if shape.Type == 6:
            for i in range(1, shape.GroupItems.Count + 1):
                sub_txt = extract_shape_text(shape.GroupItems(i))
                if sub_txt:
                    texts.append(sub_txt)
    except Exception:
        pass
    return "\n".join(texts)

def extract_slide_text(slide):
    all_texts = []
    try:
        for i in range(1, slide.Shapes.Count + 1):
            t = extract_shape_text(slide.Shapes(i))
            if t:
                all_texts.append(t)
    except Exception:
        pass
    return "\n".join(all_texts).strip()

def pil_image_to_rgb565_rows(pil_img):
    img = pil_img.convert("RGB").resize((IMG_W, IMG_H), Image.Resampling.LANCZOS)
    pixels = list(img.getdata())
    rows_b64 = []
    for y in range(IMG_H):
        row_bytes = bytearray(IMG_W * 2)
        base_idx = y * IMG_W
        for x in range(IMG_W):
            r, g, b = pixels[base_idx + x]
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            row_bytes[x * 2] = rgb565 & 0xFF
            row_bytes[x * 2 + 1] = (rgb565 >> 8) & 0xFF
        rows_b64.append(base64.b64encode(row_bytes).decode("ascii"))
    return rows_b64

def slide_to_rgb565_rows(slide, temp_png):
    slide.Export(temp_png, "PNG", 632, 504)
    with Image.open(temp_png) as img:
        return pil_image_to_rgb565_rows(img)

def blank_rgb565_rows():
    black_img = Image.new("RGB", (IMG_W, IMG_H), (12, 12, 12))
    return pil_image_to_rgb565_rows(black_img)

class PowerPointBridge:
    def __init__(self, ser):
        self.ser = ser
        self.running = True
        self.lock = threading.Lock()
        self.last_slide_signature = None

    def send_line(self, line_str):
        with self.lock:
            try:
                self.ser.write((line_str + "\n").encode("ascii"))
                self.ser.flush()
            except Exception as e:
                print(f"[!] Lỗi gửi dữ liệu: {e}")

    def get_ppt_app(self):
        try:
            return win32com.client.GetActiveObject("PowerPoint.Application")
        except Exception:
            return None

    def execute_ppt_command(self, cmd):
        pythoncom.CoInitialize()
        try:
            app = self.get_ppt_app()
            if not app:
                return
            if app.SlideShowWindows.Count > 0:
                view = app.SlideShowWindows(1).View
                if cmd == "NEXT":
                    view.Next()
                elif cmd == "PREV":
                    view.Previous()
            elif app.Windows.Count > 0:
                pres = app.ActivePresentation
                curr_idx = app.ActiveWindow.View.Slide.SlideIndex
                if cmd == "NEXT" and curr_idx < pres.Slides.Count:
                    app.ActiveWindow.View.GotoSlide(curr_idx + 1)
                elif cmd == "PREV" and curr_idx > 1:
                    app.ActiveWindow.View.GotoSlide(curr_idx - 1)
        except Exception as e:
            print(f"[!] Không thể chuyển slide: {e}")
        finally:
            pythoncom.CoUninitialize()

    def serial_reader_loop(self):
        while self.running:
            try:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode("utf-8", errors="ignore").strip()
                    if line == "CMD:NEXT":
                        print("[->] Tay cầm: Slide TIẾP THEO")
                        self.execute_ppt_command("NEXT")
                    elif line == "CMD:PREV":
                        print("[<-] Tay cầm: Slide TRƯỚC ĐÓ")
                        self.execute_ppt_command("PREV")
                else:
                    time.sleep(0.01)
            except Exception:
                time.sleep(0.1)

    def sync_loop(self):
        pythoncom.CoInitialize()
        temp_curr_png = os.path.join(tempfile.gettempdir(), "esp32_ppt_curr.png")
        temp_next_png = os.path.join(tempfile.gettempdir(), "esp32_ppt_next.png")
        last_ping = 0
        try:
            while self.running:
                now = time.time()
                if now - last_ping > 1.2:
                    self.send_line("P")
                    last_ping = now

                app = self.get_ppt_app()
                current_slide = None
                pres = None

                if app:
                    try:
                        if app.SlideShowWindows.Count > 0:
                            current_slide = app.SlideShowWindows(1).View.Slide
                            pres = app.SlideShowWindows(1).Presentation
                        elif app.Windows.Count > 0:
                            current_slide = app.ActiveWindow.View.Slide
                            pres = app.ActivePresentation
                    except Exception:
                        current_slide = None

                if current_slide is not None and pres is not None:
                    curr_idx = current_slide.SlideIndex
                    total_slides = pres.Slides.Count
                    sig = f"{pres.Name}_{current_slide.SlideID}_{curr_idx}_{total_slides}"

                    if sig != self.last_slide_signature:
                        self.last_slide_signature = sig
                        print(f"[*] Đồng bộ Slide #{curr_idx}/{total_slides} ({pres.Name})...")

                        slide_text = extract_slide_text(current_slide)
                        text_b64 = base64.b64encode(slide_text.encode("utf-8")).decode("ascii")
                        self.send_line(f"T:{text_b64}")
                        time.sleep(0.015)

                        curr_rows = slide_to_rgb565_rows(current_slide, temp_curr_png)
                        for idx, r_b64 in enumerate(curr_rows):
                            self.send_line(f"I:{idx}:{r_b64}")
                            time.sleep(0.0035)

                        if curr_idx < total_slides:
                            next_slide = pres.Slides(curr_idx + 1)
                            next_rows = slide_to_rgb565_rows(next_slide, temp_next_png)
                        else:
                            next_rows = blank_rgb565_rows()

                        for idx, r_b64 in enumerate(next_rows):
                            self.send_line(f"N:{idx}:{r_b64}")
                            time.sleep(0.0035)

                        print(f"[+] Hoàn tất đồng bộ Slide #{curr_idx} & Slide kế tiếp")

                time.sleep(0.15)
        finally:
            pythoncom.CoUninitialize()

def main():
    os.system("title PowerPoint Wireless Remote Bridge")
    print("====================================================")
    print("   HE THONG DIEU KHIEN & XEM TRUOC POWERPOINT       ")
    print("====================================================")

    ser = None
    while ser is None:
        port = find_device_port()
        if not port:
            print("[!] Chưa tìm thấy thiết bị kết nối máy tính. Đang quét lại sau 2 giây...")
            time.sleep(2)
            continue
        try:
            ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
            print(f"[+] Đã kết nối với thiết bị kết nối máy tính tại cổng: {port} ({BAUD_RATE} baud)")
        except serial.SerialException:
            print(f"[!] Cổng {port} của thiết bị kết nối máy tính đang bận. Thử lại sau 2 giây...")
            time.sleep(2)

    time.sleep(0.5)
    bridge = PowerPointBridge(ser)
    rx_thread = threading.Thread(target=bridge.serial_reader_loop, daemon=True)
    rx_thread.start()

    try:
        bridge.sync_loop()
    except KeyboardInterrupt:
        print("\n[*] Đã dừng chương trình.")
        bridge.running = False
        ser.close()

if __name__ == "__main__":
    main()