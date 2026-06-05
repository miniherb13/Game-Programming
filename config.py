"""공통 설정 — A(조현진) 관리. B는 pull만, CAPTURE_PATH는 각자 Pi에서 로컬 수정."""

BUZZER_PIN = 18
LED_PIN = None  # LED 사용 시 GPIO 번호, 없으면 None

LCD_ENABLED = True
LCD_ADDR = 0x27

# 각자 Pi 사용자명·경로에 맞게 수정
CAPTURE_PATH = "/home/hyeonjin/iot_lab8/detect.jpg"

# motion: ROI grayscale diff mean > MOTION_THRESHOLD 이면 움직임
MOTION_THRESHOLD = 25
CAPTURE_INTERVAL = 2.0
MOTION_COOLDOWN = 3.0  # motion 감지 후 버저/LCD 재트리거 대기(초)

# True면 터미널에 diff 점수 출력 (Pi 튜닝용)
DEBUG_MOTION = False
