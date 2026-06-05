"""공통 설정 — A(조현진) 관리. B는 pull만, CAPTURE_PATH는 각자 Pi에서 로컬 수정."""

# --- 효담 배선 (BCM) ---
# 초음파 TRIG=16, ECHO=18 / LED=17 / 버저=27 (18은 ECHO와 충돌)
TRIG_PIN = 16
ECHO_PIN = 18
LED_PIN = 17
BUZZER_PIN = 27

LCD_ENABLED = True
LCD_ADDR = 0x27

# 각자 Pi 사용자명·경로에 맞게 수정
CAPTURE_PATH = "/home/hyeonjin/iot_lab8/detect.jpg"

# motion: ROI grayscale diff mean > MOTION_THRESHOLD 이면 움직임
MOTION_THRESHOLD = 25
CAPTURE_INTERVAL = 2.0
MOTION_COOLDOWN = 3.0

# 초음파: 이 거리(cm) 이하면 가까움 → motion과 동일 반응
ULTRASONIC_CLOSE_CM = 20

DEBUG_MOTION = False
