# 팀원 A / B 역할 분담 (Pi + PC 둘 다 있을 때)

**전제:** 스피커 없음 · 하루 MVP · lab8 배선 기준  
**브랜치:** `Iot`  
**충돌 방지:** 아래 표의 "소유 파일"만 수정. 다른 파일은 카톡으로 요청.

---

## 한 줄 요약

| | **A (박효담, 팀장)** | **B (조현진)** |
|---|----------------------|----------------|
| **담당** | `companion.py` + `reply.py` + `config.py` | `motion.py` + `lcd_actuator.py` |
| **Pi 테스트** | 대화(LCD 답) + 최종 통합 | 움직임 + LCD/버저 |
| **오후** | main 통합 · 시연 영상 | motion/LCD 버그 수정 |

---

## 파일 소유권

| 파일 | 작성·수정 | 다른 사람 |
|------|-----------|-----------|
| `config.py` | **A** | B는 pull만, 경로만 각자 Pi에서 수정 |
| `reply.py` | **A** | B 읽기만 |
| `companion.py` | **A** | B는 오후 버그 제안만 |
| `motion.py` | **B** | A는 import만 |
| `lcd_actuator.py` | **B** | A는 import만 |

**같은 파일 동시 수정 금지.**

---

## 모듈 간 인터페이스 (사전 합의)

### `config.py` (A 작성, 09:30 공유)

```python
BUZZER_PIN = 18
LED_PIN = None          # 사용 시 핀 번호, 없으면 None
LCD_ENABLED = True
LCD_ADDR = 0x27
CAPTURE_PATH = "/home/사용자명/iot_lab8/detect.jpg"  # 각자 Pi 경로
MOTION_THRESHOLD = 25
CAPTURE_INTERVAL = 2.0
```

### `motion.py` (B)

```python
def detect_motion(prev_bgr, curr_bgr, threshold=25) -> bool:
    """ROI 중앙 grayscale diff mean > threshold 이면 True"""
```

### `lcd_actuator.py` (B)

```python
class Actuator:
    def show_idle(self): ...           # "Companion Bot" / "Zzz..."
    def show_motion(self): ...         # "Hi there!" + 버저 0.4초
    def show_reply(self, line1, line2): ...
    def close(self): ...
```

### `reply.py` (A)

```python
def get_reply(user_text: str) -> tuple[str, str]:
    """(line1, line2) 각 16자 이내"""
```

### `companion.py` (A)

- `motion.py` + `lcd_actuator.py` + `reply.py` import
- 메인 루프: 촬영 → motion → idle/motion LCD
- 터미널 `input()` 또는 스레드로 질문 → `get_reply` → `show_reply`

---

## 오전 일정 (09:00–12:00) — 각자 Pi에서 모듈 완료

### 09:00–09:30 · 둘 다 (카톡/줌 15분)

- [ ] `Project/` 폴더 clone · `git checkout Iot`
- [ ] `config.py` 값 확정 (A 작성 → B에게 push 알림)
- [ ] 각자 Pi에서 `python3 color_alert.py live` 또는 lab8 하드웨어 확인

### A — 09:30–12:00

| 순서 | 할 일 | Pi 완료 기준 |
|------|--------|--------------|
| 1 | `reply.py` — 키워드 dict 10개 | `get_reply("안녕")` → 2줄 출력 |
| 2 | (선택) API 연동 | 네트워크 되면 API, 아니면 키워드만 |
| 3 | `companion.py` 뼈대 (import만) | import 에러 없음 |

**A는 오전에 `motion.py`, `lcd_actuator.py` 수정하지 않음.**

### B — 09:30–12:00

| 순서 | 할 일 | Pi 완료 기준 |
|------|--------|--------------|
| 1 | `lcd_actuator.py` — lab8 LCD/Buzzer 분리 | `show_idle`, `show_motion`, `show_reply` 각각 동작 |
| 2 | `motion.py` — 프레임 차이 감지 | 손 흔들면 `True` |
| 3 | (임시) motion만 도는 테스트 스크립트 | 2초마다 촬영 → 터미널에 motion 출력 |

**B는 오전에 `reply.py`, `companion.py` 수정하지 않음.**

### 12:00 · 1차 합치기 (30분)

1. B → `git push origin Iot` (motion, lcd_actuator)
2. A → `git pull` 후 `companion.py`에 B 모듈 연결
3. **목표:** 손 흔들면 LCD + 버저 1회 성공

---

## 오후 일정 (13:00–17:00)

| 시각 | A | B |
|------|---|---|
| **13:00** | `companion.py` 메인 루프 완성 | pull 후 본인 Pi에서 B 모듈 재테스트 |
| **14:30** | 통합본 양쪽 Pi에서 5분 이상 실행 | motion 임계값·LCD 문구 튜닝 |
| **15:30** | 시연 대본 확정 | 보고서: OpenCV motion, GPIO 파트 |
| **16:00** | **시연 영상 촬영** | 리허설 + 보고서 검토 |
| **17:00** | 보고서 취합·제출 | 서론·블록도·참고문헌 |

---

## 연락 체크포인트 (4번)

| 시각 | 내용 |
|------|------|
| 09:30 | `config.py` 확정 |
| 12:00 | 모듈 합치기, motion 1회 성공 |
| 14:30 | 통합본 둘 다 Pi에서 실행 |
| 16:00 | 시연 리허설 (영상 통화 10분) |

---

## 각자 "오전 완료" 조건

### A

- [ ] `get_reply("안녕")` → `(line1, line2)` (Pi 또는 PC)
- [ ] `reply.py` push 완료

### B

- [ ] 손 흔들면 `show_motion()` → LCD + 버저 (본인 Pi)
- [ ] `motion.py`, `lcd_actuator.py` push 완료

---

## 받은 초음파 코드에 대해

팀원이 받은 PET ROBOT(초음파) 코드는 `reference/ultrasonic_pet_robot.py`에 보관.

- **그대로 쓰지 말 것** — lab8 버저 핀(18)과 ECHO(18) 충돌
- 기획(카메라 + LLM)과도 다름
- 참고만: `show_lcd()`, 거리 구간별 상태 나누기 아이디어

---

## 리스크 Plan B

| 문제 | 담당 | 대안 |
|------|------|------|
| API 느림/실패 | A | 키워드 dict만 사용 |
| 카메라 오탐 | B | `MOTION_THRESHOLD` 30~35로 상향 |
| 통합 버그 | A | motion 루프와 `input()` 번갈아 (동시 실행 X) |
| LCD 안 됨 | B | `LCD_ENABLED = False`, 터미널 `[LCD]` 출력 |

---

## 보고서 분담

| 파트 | 담당 |
|------|------|
| 서론, LLM·AIoT 개념, 시스템 블록도, 결론 | A (취합) |
| 하드웨어 배선, OpenCV motion, GPIO, 시연 영상 | B |
| 일정·역할 표 | `ROLES.md` 첨부 또는 인용 |
