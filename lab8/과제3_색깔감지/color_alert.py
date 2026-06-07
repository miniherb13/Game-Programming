#!/usr/bin/env python3
"""
Lab 8 과제3: 색 감지 → LED / LCD / Buzzer (색마다 다른 반응)

※ GPIO 핀은 본인 이전 실습 배선에 맞게 CONFIG 수정
※ LCD 없으면 LCD_ENABLED = False (터미널만 출력)

실행 (Pi, ~/iot_lab8):
  python3 color_alert.py still              # 사진 1장 분석
  python3 color_alert.py still detect.jpg
  python3 color_alert.py live               # 2초마다 촬영 후 반복 (VNC 불필요)
"""

import subprocess
import sys
import time

import cv2
import numpy as np
from gpiozero import Buzzer

# ========== 배선 설정 ==========
BUZZER_PIN = 18       # 버저 I/O (Pin 12), VCC=3.3V

LCD_ENABLED = True
LCD_ADDR = 0x27       # i2cdetect 에서 27 확인됨

# 색 감지: ROI에서 해당 색 픽셀 비율이 이 값 이상이면 감지
DETECT_THRESHOLD = 0.12

# ========== LCD (Lab 5 — RPLCD) ==========
# pip3 install RPLCD --break-system-packages
from RPLCD.i2c import CharLCD


class LCD:
    """Lab5와 동일한 RPLCD 래퍼"""

    def __init__(self, addr=0x27):
        self.lcd = CharLCD(
            i2c_expander="PCF8574", address=addr, port=1, cols=16, rows=2
        )
        self.lcd.clear()

    def show(self, line1="", line2=""):
        self.lcd.cursor_pos = (0, 0)
        self.lcd.write_string(line1[:16].ljust(16))
        self.lcd.cursor_pos = (1, 0)
        self.lcd.write_string(line2[:16].ljust(16))

    def close(self):
        self.lcd.clear()
        self.lcd.close()


# ========== 색 정의 (HSV) ==========
COLOR_RANGES = {
    "RED": [
        ([0, 80, 80], [10, 255, 255]),
        ([160, 80, 80], [179, 255, 255]),
    ],
    "GREEN": [([40, 50, 50], [85, 255, 255])],
    "BLUE": [([90, 50, 50], [130, 255, 255])],
}


def mask_ranges(hsv, ranges):
    m = np.zeros(hsv.shape[:2], dtype=np.uint8)
    for lo, hi in ranges:
        m |= cv2.inRange(hsv, np.array(lo), np.array(hi))
    return m


def detect_color(bgr):
    h, w = bgr.shape[:2]
    roi = bgr[h // 4 : 3 * h // 4, w // 4 : 3 * w // 4]
    hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
    total = roi.shape[0] * roi.shape[1]

    best, best_ratio = "NONE", 0.0
    for name, ranges in COLOR_RANGES.items():
        m = mask_ranges(hsv, ranges)
        ratio = np.count_nonzero(m) / total
        if ratio > best_ratio:
            best, best_ratio = name, ratio

    if best_ratio < DETECT_THRESHOLD:
        return "NONE", best_ratio
    return best, best_ratio


class ColorAlertSystem:
    def __init__(self):
        self.buzzer = Buzzer(BUZZER_PIN)
        self.lcd = LCD(LCD_ADDR) if LCD_ENABLED else None

    def lcd_msg(self, line1, line2=""):
        print(f"[LCD] {line1} | {line2}")
        if self.lcd:
            try:
                self.lcd.show(line1, line2)
            except Exception as e:
                print(f"LCD error: {e}")

    def all_off(self):
        self.buzzer.off()

    def react(self, color):
        self.all_off()
        if color == "RED":
            # 빨강 → Buzzer만
            print("[RED] Buzzer only")
            self.buzzer.on()
            time.sleep(0.4)
            self.buzzer.off()
        elif color == "BLUE":
            # 파랑 → LCD만
            self.lcd_msg("BLUE detected", "LCD only")
            time.sleep(1.0)
        elif color == "GREEN":
            # 초록 → LCD + Buzzer
            self.lcd_msg("GREEN detected", "LCD + Buzzer")
            self.buzzer.beep(on_time=0.1, off_time=0.1, n=3)
            time.sleep(0.5)
        else:
            self.lcd_msg("No color", "Show R/G/B")
        self.all_off()

    def close(self):
        self.all_off()
        if self.lcd:
            self.lcd.close()
        self.buzzer.close()


def capture(path="detect.jpg"):
    subprocess.run(
        ["rpicam-still", "-n", "-t", "1000", "-o", path],
        check=True,
    )
    return path


def run_still(image_path):
    sys_img = cv2.imread(image_path)
    if sys_img is None:
        raise SystemExit(f"Cannot read {image_path}")
    color, ratio = detect_color(sys_img)
    print(f"Detected: {color} ({ratio * 100:.1f}%)")
    app = ColorAlertSystem()
    try:
        app.react(color)
    finally:
        app.close()


def run_live():
    app = ColorAlertSystem()
    path = "/home/hyeonjin/iot_lab8/detect.jpg"
    try:
        while True:
            capture(path)
            img = cv2.imread(path)
            color, ratio = detect_color(img)
            print(f"Detected: {color} ({ratio * 100:.1f}%)")
            app.react(color)
            time.sleep(1.5)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        app.close()


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    mode = sys.argv[1].lower()
    if mode == "still":
        img = sys.argv[2] if len(sys.argv) > 2 else "lab2_photo.jpg"
        run_still(img)
    elif mode == "live":
        run_live()
    else:
        print("Usage: python3 color_alert.py still [image.jpg]")
        print("       python3 color_alert.py live")
        sys.exit(1)


if __name__ == "__main__":
    main()
