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
  ultrasonic.py       # 초음파 거리 (효담 배선)
  lcd_actuator.py     # B 담당 — LCD / 버저 / LED / 초음파 출력
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
python3 -c "from reply import get_reply; print(get_reply('hello'))"
python3 test_motion.py live          # Pi 카메라 — 2초마다 motion 출력

# B 모듈 단독 테스트
python3 lcd_actuator.py

# 통합 실행 (A)
python3 companion.py
```

## VNC / SSH에서 실행 (시연용)

Pi 터미널(VNC·SSH 동일)에서:

```bash
cd ~/Iot
git pull origin Iot
python3 companion.py
```

| 목적 | 명령 |
|------|------|
| **시연·통합 (메인)** | `python3 companion.py` |
| LCD·버저·초음파만 | `python3 lcd_actuator.py` |
| 카메라·motion만 | `python3 test_motion.py live` |

종료: `Ctrl+C`

### 버저 배선 (팀 확정)

| 버저 선 | 물리 핀 |
|---------|---------|
| I/O | **13** (GPIO 27) |
| VCC | **1** (3.3V) |
| GND | **9** (다른 GND 핀도 가능) |

## 시연 시나리오 (3분)

1. 대기 — LCD `Companion Bot` + 거리(cm)
2. 손 흔들기 **또는** 20cm 이내 접근 → LCD `Hi there!` + LED + 버저
3. 터미널에 영어 질문 입력 (예: `hello`, `thanks`) → LCD 답변 표시

## 배선 (효담 회로 + 버저 추가, BCM 번호)

| 부품 | BCM GPIO | 물리 핀 (Pi 4) | 연결 |
|------|----------|----------------|------|
| 초음파 TRIG | **16** | Pin 36 | Pi GPIO 16 |
| 초음파 ECHO | **18** | Pin 12 | Pi GPIO 18 |
| LED | **17** | Pin 11 | LED + 저항 → GND |
| **버저 I/O** | **27** | Pin 13 | 버저 I/O (⚠️ **GPIO 18 금지** — ECHO와 충돌) |
| 버저 VCC | — | Pin 1 또는 17 (3.3V) | 버저 VCC |
| 버저 GND | — | **아무 GND 핀** | Pin 6·9·14·20·25·30·34·39 중 빈 곳 |
| LCD I2C SDA | 2 | Pin 3 | LCD SDA |
| LCD I2C SCL | 3 | Pin 5 | LCD SCL |
| LCD VCC/GND | — | Pin 2 / GND | 5V / GND (6번 말고 다른 GND도 OK) |

- 초음파: VCC→5V, GND→GND, TRIG→16, ECHO→18
- **버저 I/O는 반드시 GPIO 27** (GPIO 18은 초음파 ECHO 전용)
- **GND·5V·3.3V는 핀이 여러 개** — 이미 6번 쓰 중이면 **9·14·20번 등 다른 GND** 사용
- LCD 주소: `0x27` (`i2cdetect -y 1`로 확인)

## 동작

- **카메라 motion** 또는 **초음파 ≤ 20cm** → LCD `Hi there!` + LED ON + 버저
- 대기 중 LCD 2줄에 거리(cm) 표시
- 터미널 질문 → `reply.py` 답변 LCD 표시

## lab8 참고

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
