"""
참고용 — 팀원이 받은 초음파 PET ROBOT 예제 (그대로 사용 금지)

주의:
- ECHO=18 은 lab8 버저 핀(18)과 충돌
- 기획(카메라 + LLM)과 다름
- show_lcd(), 거리 구간별 상태 아이디어만 참고
"""

import RPi.GPIO as GPIO
import time
from RPLCD.i2c import CharLCD

TRIG = 16
ECHO = 18
LED = 17
BUZZER = 27

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

GPIO.setup(TRIG, GPIO.OUT)
GPIO.setup(ECHO, GPIO.IN)
GPIO.setup(LED, GPIO.OUT)
GPIO.setup(BUZZER, GPIO.OUT)

lcd = CharLCD("PCF8574", 0x27)


def get_distance():
    GPIO.output(TRIG, False)
    time.sleep(0.05)
    GPIO.output(TRIG, True)
    time.sleep(0.00001)
    GPIO.output(TRIG, False)

    start_time = time.time()
    stop_time = time.time()
    timeout = time.time() + 0.04

    while GPIO.input(ECHO) == 0:
        start_time = time.time()
        if time.time() > timeout:
            return -1

    timeout = time.time() + 0.04
    while GPIO.input(ECHO) == 1:
        stop_time = time.time()
        if time.time() > timeout:
            return -1

    elapsed = stop_time - start_time
    return round(elapsed * 34300 / 2, 2)


def show_lcd(line1, line2=""):
    lcd.clear()
    lcd.cursor_pos = (0, 0)
    lcd.write_string(line1[:16])
    lcd.cursor_pos = (1, 0)
    lcd.write_string(line2[:16])


try:
    show_lcd("PET ROBOT", "STARTING...")
    time.sleep(2)
    while True:
        distance = get_distance()
        print("Distance:", distance, "cm")
        if distance == -1:
            GPIO.output(LED, False)
            GPIO.output(BUZZER, False)
            show_lcd("SENSOR ERROR", "CHECK WIRE")
        elif distance <= 20:
            GPIO.output(LED, True)
            GPIO.output(BUZZER, True)
            show_lcd("TOO CLOSE!", f"{distance} cm")
        elif distance <= 50:
            GPIO.output(LED, True)
            GPIO.output(BUZZER, False)
            show_lcd("HELLO :)", f"{distance} cm")
        else:
            GPIO.output(LED, False)
            GPIO.output(BUZZER, False)
            show_lcd("WAITING...", f"{distance} cm")
        time.sleep(0.5)
except KeyboardInterrupt:
    print("Program stopped")
finally:
    GPIO.output(LED, False)
    GPIO.output(BUZZER, False)
    lcd.clear()
    GPIO.cleanup()
