"""Shared settings. Edit CAPTURE_PATH per Pi."""

# Wiring (BCM): ultrasonic TRIG=16 ECHO=18, LED=17, buzzer=27 (not 18)
TRIG_PIN = 16
ECHO_PIN = 18
LED_PIN = 17
BUZZER_PIN = 27

LCD_ENABLED = True
LCD_ADDR = 0x27

CAPTURE_PATH = "/home/hyeonjin/iot_lab8/detect.jpg"

MOTION_THRESHOLD = 25
CAPTURE_INTERVAL = 2.0
MOTION_COOLDOWN = 3.0

ULTRASONIC_CLOSE_CM = 20

DEBUG_MOTION = False
