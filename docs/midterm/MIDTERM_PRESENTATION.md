# Midterm Project Presentation (Draft)

**Course:** IoT Systems  
**Team:** Park Hyodam (202304709, Captain) · Cho Hyunjin (202304438)  
**Title:** Motion-Adaptive AI Companion Robot: LLM-based Emotional Interactive AIoT System  
**Time:** ~3 min presentation + ~2 min Q&A (max 5 min total)  
**Note:** Slides in **English** · Speech can be Korean (English preferred)

---

## Part 1 — PPT Slides (English, copy to PowerPoint)

### Slide 1 — Title
**Motion-Adaptive AI Companion Robot**  
LLM-based Emotional Interactive AIoT System  

Park Hyodam (202304709) · Cho Hyunjin (202304438)  
Team Captain: Park Hyodam  

---

### Slide 2 — A. Main Idea & Objective
**What is our project?**  
A desktop **AI companion robot** on Raspberry Pi that senses user presence and responds emotionally through **LCD, LED, and buzzer** — with **LLM-based text interaction**.

**Objective**
- Build an **AIoT pipeline**: Sensor → Decision → Actuator
- Detect **motion** (camera) and **proximity** (ultrasonic)
- Show robot **state** on LCD and LED; alert with buzzer
- Support **short English conversation** on LCD (keyword / LLM)

---

### Slide 3 — B. System Design & Methodology
**Architecture (3 modules)**

```
[Input]                    [Processing]              [Output]
Camera (OpenCV)     →     companion.py (main)  →    LCD (I2C)
Ultrasonic (HC-SR04) →    motion.py / reply.py →    LED (GPIO)
Terminal (keyboard)  →                         →    Buzzer (GPIO, planned)
```

**Methodology**
1. `rpicam-still` captures a frame every 2 seconds  
2. `detect_motion()` compares ROI grayscale diff between frames  
3. Ultrasonic sensor checks distance ≤ 20 cm  
4. If motion **or** close distance → `show_motion()`  
5. User types a question → `get_reply()` → `show_reply()` on LCD  

---

### Slide 4 — C. Key Code — `Actuator` class (draft: `led_lcd.txt`)
**Role:** Maps robot **state** to LCD + LED (buzzer to be added)

```python
class Actuator:
    def show_idle(self):
        self.led.off()
        self._lcd("Companion Bot", "Zzz...")

    def show_motion(self):
        self.led.on()
        self._lcd("Hi there!", "I see you!")

    def show_reply(self, line1, line2=""):
        self.led.on()
        self._lcd(line1, line2)
```

**Explanation**
- `show_idle()` — waiting state (no user detected)  
- `show_motion()` — user detected → LED ON + greeting on LCD  
- `show_reply()` — chat response on 2×16 LCD  
- **Next step:** add `_buzzer_beep()` inside `show_motion()` (Lab9 GPIO experience)

---

### Slide 5 — C. Key Code — Motion detection
```python
def detect_motion(prev_bgr, curr_bgr, threshold=25) -> bool:
    # Center ROI → grayscale → abs diff mean
    return motion_score(prev_bgr, curr_bgr) > threshold
```

- Uses **OpenCV** from Lab 8 camera practice  
- No heavy ML model → runs fast on Raspberry Pi  

---

### Slide 6 — D. Hardware Components
| Component | Interface | GPIO / Pin |
|-----------|-----------|------------|
| Raspberry Pi 4 | — | Main controller |
| Camera Module | CSI | `rpicam-still` |
| I2C LCD 1602 | I2C | SDA/SCL, addr **0x27** |
| LED | GPIO OUT | BCM **17** |
| Ultrasonic HC-SR04 | TRIG / ECHO | **16** / **18** |
| Buzzer *(planned)* | GPIO OUT | BCM **27** |
| Touch sensor *(Lab 9)* | GPIO IN | BCM **17** — practiced |

*(Insert wiring photo / diagram here)*

---

### Slide 7 — E. Challenges & Solutions
| Challenge | Solution |
|-----------|----------|
| GPIO pin conflict (ECHO 18 vs buzzer) | Buzzer moved to **GPIO 27** |
| No speaker available | Text reply on **LCD** only |
| Pi LLM too slow offline | Keyword replies + optional **Gemini API** |
| Motion false triggers | ROI + threshold tuning + 3 s cooldown |
| Module merge conflicts | Split files: A = motion/main, B = `lcd_actuator.py` |

---

### Slide 8 — F. Expected Output & Benefits
**Demo scenario (final)**
1. Idle → LCD `Companion Bot / Zzz...`, LED off  
2. User waves hand or comes close → LCD `Hi there!`, LED on, **buzzer beeps**  
3. User types "hello" → LCD shows 2-line reply  

**Benefits**
- Low-cost **emotional AIoT** prototype  
- Reuses **Lab 8** (camera, LCD) + **Lab 9** (GPIO, touch/LED) skills  
- Expandable to voice (offline voice-assistant reference)  

---

### Slide 9 — G. Future Plan & Next Steps
| Week | Task | Owner |
|------|------|-------|
| Now | Integrate **buzzer** into `Actuator.show_motion()` | B (Hyodam) |
| Next | Stable `companion.py` demo on both Pis | A (Hyunjin) |
| Next | LLM persona tuning (Gemini / keywords) | A |
| Final | 3-min demo video + report | Both |

**References:** voice-assistant (GitHub), HRI papers (ACM), Raspberry Pi AI Kit projects  

---

### Slide 10 — Thank You
**Questions?**

Git branch: `Iot` — github.com/miniherb13/Game-Programming  

---

## Part 2 — Presentation Script (Korean, ~3 minutes)

> **Tip:** Slide는 영어, 말은 아래 대본 기준. 핵심 용어만 영어로 발음하면 됩니다.

---

**[Slide 1 — 0:00~0:15]**

안녕하세요. IoT 시스템 과목 기말 프로젝트 중간 발표입니다.  
팀장 박효담, 팀원 조현진입니다.  

프로젝트 제목은 **"Motion-Adaptive AI Companion Robot"** —  
LLM 기반 감정형 AIoT 동반 로봇입니다.

---

**[Slide 2 — 0:15~0:40]**

저희 프로젝트의 핵심 아이디어는, 라즈베리파이 위에 **작은 동반 로봇**을 만드는 것입니다.  

사용자가 가까이 오거나 움직이면 로봇이 **LCD와 LED, 버저**로 반응하고,  
키보드로 영어 질문을 입력하면 **짧은 답변**을 LCD에 보여 줍니다.  

목표는 **센서 → 판단 → 출력**이 연결된 AIoT 파이프라인을 완성하는 것입니다.

---

**[Slide 3 — 0:40~1:05]**

시스템 설계는 세 모듈로 나눴습니다.  

**입력**은 카메라, 초음파 센서, 터미널 키보드입니다.  
**처리**는 `companion.py` 메인 루프와 `motion.py`, `reply.py`가 담당합니다.  
**출력**은 `lcd_actuator.py`의 LCD, LED, 그리고 앞으로 붙일 버저입니다.  

2초마다 사진을 찍어 OpenCV로 움직임을 감지하고,  
초음파로 20cm 이하 거리도 함께 확인합니다.  
둘 중 하나라도 감지되면 **motion 상태**로 전환합니다.

---

**[Slide 4~5 — 1:05~1:40]**

코드 초안은 `led_lcd.txt`의 **Actuator 클래스**입니다.  

`show_idle`은 대기 상태로 LCD에 "Zzz...", LED는 끕니다.  
`show_motion`은 사용자 감지 시 LED를 켜고 인사말을 띄웁니다.  
`show_reply`는 대화 응답을 LCD 두 줄에 출력합니다.  

움직임 감지는 Lab 8에서 쓴 OpenCV 방식으로,  
화면 중앙 ROI의 프레임 차이 평균이 임계값을 넘으면 True를 반환합니다.  

현재 이 초안에 **버저 비프 함수**를 `show_motion` 안에 추가하는 작업이 다음 단계입니다.

---

**[Slide 6 — 1:40~2:00]**

하드웨어는 라즈베리파이, 카메라, I2C LCD, LED, 초음파 센서를 사용합니다.  
버저는 GPIO 27번에 연결할 예정입니다.  
ECHO 핀이 18번이라 버저를 18번에 쓰지 못해 27번으로 분리했습니다.  

Lab 9에서는 터치 센서와 LED 2개 제어 실습도 완료해, GPIO 출력 경험을 쌓았습니다.

---

**[Slide 7 — 2:00~2:20]**

현재 어려움은 세 가지입니다.  

첫째, GPIO 핀 충돌 — 해결했습니다.  
둘째, 스피커가 없어 음성 대신 LCD 텍스트로 응답합니다.  
셋째, Pi에서 로컬 LLM이 무거워서 키워드 응답과 API를 병행합니다.  

motion 오탐은 ROI와 cooldown 3초로 줄이고 있습니다.

---

**[Slide 8~9 — 2:20~2:50]**

최종 시연은 세 단계입니다.  
대기 → 움직임/근접 감지 시 LED·버저·LCD 반응 → 터미널 질문에 LCD 답변.  

앞으로 버저 통합, 양쪽 Pi에서 통합 테스트, LLM 페르소나 조정 후  
데모 영상과 보고서를 제출할 예정입니다.  

발표는 여기까지입니다. 질문 있으시면 말씀해 주세요. 감사합니다.

---

## Part 3 — Q&A 예상 질문 & 답변

**Q: Why no speaker?**  
A: We don't have a speaker module. LCD text output is our primary feedback. Voice can be added later using the offline voice-assistant reference.

**Q: Why both camera and ultrasonic?**  
A: Camera detects **motion** (waving). Ultrasonic detects **proximity** (approaching). Together they make the robot more responsive — "motion-adaptive."

**Q: Is LLM running on the Pi?**  
A: Currently keyword matching on Pi; Gemini API for richer replies when network is available. Full offline LLM is a stretch goal.

**Q: What did you complete so far?**  
A: `lcd_actuator.py` draft (LCD + LED), `motion.py`, `companion.py` integration, Lab 9 touch/LED demo, Git `Iot` branch with team role split.

---

## Part 4 — 체크리스트 (수업 전)

- [ ] PPT 영어 슬라이드 8~10장 복사 완료  
- [ ] 교실 PC 바탕화면에 PPT 저장 (수업 시작 전)  
- [ ] 시연 영상 또는 배선 사진 1~2장 슬라이드 삽입  
- [ ] 3분 맞춰 리허설 (질문 2분 별도)  
- [ ] 발표자 1명 이상 참석 확인  
