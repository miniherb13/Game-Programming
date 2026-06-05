# IoT 기말 프로젝트 — Motion-Adaptive AI Companion Robot

**팀:** 조현진(202304438, A) / 박효담(202304709, 팀장·B)  
**환경:** 둘 다 Raspberry Pi + PC 보유  
**출력:** LCD + 버저 + LED (스피커/TTS 없음)  
**입력:** 카메라(움직임) + 터미널 키보드(대화)

> 상세 역할 분담은 [ROLES.md](./ROLES.md) 참고

## 프로젝트 구조

```
Project/
  README.md           # 이 파일
  ROLES.md            # A/B 역할 분담 (필독)
  config.py           # A 담당 — 공통 설정 (카메라·motion 포함)
  motion.py           # A 담당 — 카메라 움직임 감지
  reply.py            # A 담당 — 키워드·LLM 답변 생성
  companion.py        # A 담당 — 메인 통합 루프
  test_motion.py      # A 담당 — motion 단독 테스트 (Pi live)
  lcd_actuator.py     # B 담당 — LCD / 버저 / LED 출력
  reference/
    ultrasonic_pet_robot.py   # 참고용 (초음파 예제, lab8 핀과 충돌 주의)
```

## 빠른 시작 (Pi)

```bash
# 처음 한 번
cd ~
git clone -b Iot https://github.com/miniherb13/Game-Programming.git Iot
cd ~/Iot
pip3 install RPLCD opencv-python-headless gpiozero --break-system-packages
mkdir -p ~/iot_lab8

# 매번
cd ~/Iot
git pull origin Iot

# A 모듈 단독 테스트
python3 -c "from motion import detect_motion; import cv2; print('motion.py OK')"
python3 -c "from reply import get_reply; print(get_reply('안녕'))"
python3 test_motion.py live          # Pi 카메라 — 2초마다 motion 출력

# B 모듈 단독 테스트
python3 lcd_actuator.py

# 통합 실행 (A)
python3 companion.py
```

## 시연 시나리오 (3분)

1. 대기 — LCD `Companion Bot` / `Zzz...`
2. 손 흔들기 — motion 감지 → LCD `Hi there!` + 버저
3. 터미널에 질문 입력 → LCD 2줄에 답변 표시

## lab8 참고

- `BUZZER_PIN = 18`, `LCD_ADDR = 0x27`
- 촬영: `rpicam-still -n -t 1000 -o detect.jpg`
- 기존 실습: `Iot/lab8/과제3_색깔감지/color_alert.py`

## Git 브랜치

이 프로젝트는 `Iot` 브랜치에서 작업합니다.

```bash
git checkout Iot
git pull origin Iot
# 작업 후
git add .
git commit -m "설명"
git push origin Iot
```
