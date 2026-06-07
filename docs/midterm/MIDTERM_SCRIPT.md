# IoT Midterm Presentation Script (3 minutes)

**Slides:** `아이오티 중간발표 체크.pptx` (10 slides)  
**Team:** Park Hyodam (202304709, Captain) · Jo Hyeonjin (202304438)  
**Target time:** ~3:00 (leave ~2 min for Q&A separately)

---

## English Script

**[Slide 1 — Title · 0:00–0:15]**

Good morning/afternoon, Professor.  
We are Team Park Hyodam and Jo Hyeonjin from IoT Systems.  

Our project is **Motion-Adaptive AI Companion Robot** —  
an **LLM-based Emotional Interactive AIoT System** on Raspberry Pi.

---

**[Slide 2 — A. Main Idea & Objective · 0:15–0:40]**

Our main idea is a **desktop companion robot** that senses the user and responds emotionally.

We build one **AIoT pipeline** on a single device:  
**Sensor → Decision → Actuator.**

For sensing, we use a **camera** and an **ultrasonic sensor**.  
For output, we use **LCD, LED, and buzzer**.  
For interaction, we show **short English replies** on a 2×16 LCD using keywords or LLM.

Our four objectives are: build the pipeline, detect motion and proximity, show state on LCD and LED, and support short English chat.

---

**[Slide 3 — B. System Design & Methodology · 0:40–1:05]**

Our system has **three modules**.

**Input:** camera, ultrasonic HC-SR04, and terminal keyboard.  
**Processing:** `companion.py` as the main loop, plus `motion.py` and `reply.py`.  
**Output:** I2C LCD at address 0x27, LED on GPIO 17, and buzzer on GPIO 27.

Every two seconds, we capture a frame with `rpicam-still`.  
`detect_motion()` checks the center ROI frame difference.  
The ultrasonic sensor checks if distance is **20 cm or less**.  
If **motion OR proximity** is detected, we call `show_motion()`.

---

**[Slide 4 — C. Key Code · 1:05–1:30]**

The core class is **`Actuator`** in `lcd_actuator.py`.

`show_idle()` means waiting — LED off, LCD shows "Companion Bot" and "Zzz...".  
`show_motion()` means user detected — LED on, greeting on LCD, and a **0.4-second buzzer beep**.  
`show_reply()` prints a two-line chat response on the LCD.

We started from a simpler draft in `led_lcd.txt`, and we have now **integrated the buzzer** into `show_motion()`.

---

**[Slide 5 — D. Hardware · 1:30–1:50]**

Our hardware is Raspberry Pi 4 with five peripherals.

The camera uses CSI and `rpicam-still`.  
The LCD uses I2C.  
The ultrasonic uses TRIG on GPIO 16 and ECHO on GPIO 18.  
The LED is on GPIO 17.  
The buzzer is on GPIO 27.

We had a **pin conflict** because ECHO uses pin 18, so we moved the buzzer from 18 to 27.  
In Lab 9, we also practiced GPIO output with a touch sensor and dual LEDs.

---

**[Slide 6 — E. Current Progress · 1:50–2:10]**

Here is our current progress.

**Completed:** LCD, LED, and buzzer control; ultrasonic detection; OpenCV motion detection; and basic states — idle, motion, and reply.

**In progress:** `companion.py` integration testing and motion threshold tuning.

**To do:** emotional reply with LLM or keywords, state-based reactions for approach, stay, and alert, plus error handling and backup mode.

---

**[Slide 7 — F. Project Understanding · 2:10–2:30]**

Our final system is not just motion detection.

We plan a **state flow**:  
**Idle** when no user is detected,  
**Approach** when distance is within 20 cm,  
**Stay** when the user remains nearby and we ask "Want to talk?",  
**Alert** for sudden large motion,  
and **Return** to idle after timeout.

The key point is: we classify the user situation and choose an **emotional response**.

---

**[Slide 8 — Challenges & Solutions · 2:30–2:45]**

We faced five challenges.

GPIO pin conflict — solved by moving buzzer to GPIO 27.  
No speaker — we use LCD text only.  
Local LLM too slow on Pi — we use keyword replies plus optional Gemini API.  
Motion false triggers — we use ROI, threshold tuning, and a 3-second cooldown.

---

**[Slide 9 — Future Plan · 2:45–2:55]**

Our roadmap has three steps.

Step 1: stabilize sensors and actuators.  
Step 2: refine motion states — idle, approach, stay, and alert.  
Step 3: add emotional reply logic on the LCD.

Our final target is: **motion and distance sensing → emotional state → LCD, LED, buzzer, and short LLM response.**

---

**[Slide 10 — Thank You · 2:55–3:00]**

That is our midterm draft presentation.  
Thank you. We are happy to take your questions.

---

### English — Quick timing guide

| Slide | Topic | Time |
|-------|--------|------|
| 1 | Title | 0:15 |
| 2 | Main Idea | 0:25 |
| 3 | System Design | 0:25 |
| 4 | Key Code | 0:25 |
| 5 | Hardware | 0:20 |
| 6 | Progress | 0:20 |
| 7 | State Flow | 0:20 |
| 8 | Challenges | 0:15 |
| 9 | Future Plan | 0:10 |
| 10 | Thank You | 0:05 |
| **Total** | | **~3:00** |

---

---

## 한국어 대본

**[슬라이드 1 — 제목 · 0:00–0:15]**

안녕하세요, 교수님. IoT Systems 수업 **중간 발표**입니다.  
팀장 **박효담**, 팀원 **조현진**입니다.  

프로젝트 제목은 **Motion-Adaptive AI Companion Robot**,  
**LLM 기반 감정형 AIoT 동반 로봇**입니다.

---

**[슬라이드 2 — A. 주제와 목표 · 0:15–0:40]**

핵심 아이디어는 **사용자를 감지하고 감정적으로 반응하는 데스크톱 동반 로봇**입니다.

라즈베리파이 한 대에서 **센서 → 판단 → 출력** AIoT 파이프라인을 구현합니다.

센서는 **카메라**와 **초음파 센서**,  
출력은 **LCD, LED, 버저**,  
상호작용은 **2×16 LCD에 짧은 영어 응답**을 표시하는 방식입니다.

목표는 네 가지입니다. 파이프라인 구축, 움직임·근접 감지, LCD·LED 상태 표현, 짧은 영어 대화 지원입니다.

---

**[슬라이드 3 — B. 시스템 설계 · 0:40–1:05]**

시스템은 **세 모듈**로 나눴습니다.

**입력:** 카메라, 초음파 HC-SR04, 터미널 키보드.  
**처리:** 메인 루프 `companion.py`, `motion.py`, `reply.py`.  
**출력:** I2C LCD 0x27, LED GPIO 17, 버저 GPIO 27.

2초마다 `rpicam-still`로 프레임을 촬영하고,  
`detect_motion()`으로 중앙 ROI 프레임 차이를 확인합니다.  
초음파로 **20cm 이하** 거리도 함께 검사합니다.  
**움직임 또는 근접**이 감지되면 `show_motion()`을 호출합니다.

---

**[슬라이드 4 — C. 핵심 코드 · 1:05–1:30]**

핵심은 `lcd_actuator.py`의 **`Actuator` 클래스**입니다.

`show_idle()`은 대기 상태입니다. LED를 끄고 LCD에 "Companion Bot", "Zzz..."를 표시합니다.  
`show_motion()`은 사용자 감지 상태입니다. LED를 켜고 인사말을 띄우며, **버저를 0.4초** 울립니다.  
`show_reply()`는 LCD 두 줄에 대화 응답을 출력합니다.

처음에는 `led_lcd.txt` 초안으로 LCD와 LED만 구현했고,  
지금은 **`show_motion()`에 버저까지 통합**했습니다.

---

**[슬라이드 5 — D. 하드웨어 · 1:30–1:50]**

하드웨어는 라즈베리파이 4와 다섯 가지 주변장치입니다.

카메라는 CSI, LCD는 I2C,  
초음파는 TRIG 16번·ECHO 18번,  
LED는 GPIO 17번, 버저는 GPIO 27번입니다.

ECHO가 18번이라 버저와 핀이 겹쳐서, **버저를 27번으로 분리**했습니다.  
Lab 9에서는 터치 센서와 LED 2개 제어로 GPIO 출력 경험도 확보했습니다.

---

**[슬라이드 6 — E. 현재 진행 상황 · 1:50–2:10]**

현재 진행 상황입니다.

**완료:** LCD·LED·버저 제어, 초음파 거리 감지, OpenCV 움직임 감지, idle·motion·reply 기본 상태.  
**진행 중:** `companion.py` 통합 테스트, motion 임계값 튜닝.  
**예정:** LLM·키워드 감정 응답, approach·stay·alert 상태 반응, 오류 처리·백업 모드.

---

**[슬라이드 7 — F. 프로젝트 이해 · 2:10–2:30]**

최종 시스템은 단순 움직임 감지가 아닙니다.

**상태 흐름**을 설계했습니다.  
**Idle** — 사용자 없음,  
**Approach** — 20cm 이하 접근,  
**Stay** — 근처에 머무름, "Want to talk?" 유도,  
**Alert** — 갑작스러운 큰 움직임,  
**Return** — 입력 없으면 idle 복귀.

핵심은 사용자 **상황을 상태로 분류**하고 **감정형 반응**을 선택하는 것입니다.

---

**[슬라이드 8 — 문제와 해결 · 2:30–2:45]**

다섯 가지 문제와 해결입니다.

GPIO 핀 충돌 → 버저 27번으로 분리.  
스피커 없음 → LCD 텍스트 응답.  
Pi 로컬 LLM 과부하 → 키워드 + Gemini API 병행.  
움직임 오탐 → ROI, 임계값, 3초 쿨다운.

---

**[슬라이드 9 — 향후 계획 · 2:45–2:55]**

로드맵은 세 단계입니다.

1단계: 센서·액추에이터 안정화.  
2단계: idle·approach·stay·alert 상태 정교화.  
3단계: LCD 감정형 응답 로직.

최종 목표는 **움직임·거리 감지 → 감정 상태 → LCD·LED·버저·짧은 LLM 응답**입니다.

---

**[슬라이드 10 — 감사 · 2:55–3:00]**

중간 발표는 여기까지입니다.  
감사합니다. 질문 있으시면 답변드리겠습니다.

---

### 한국어 — 슬라이드별 시간표

| 슬라이드 | 내용 | 시간 |
|----------|------|------|
| 1 | 제목 | 0:15 |
| 2 | 주제·목표 | 0:25 |
| 3 | 시스템 설계 | 0:25 |
| 4 | 핵심 코드 | 0:25 |
| 5 | 하드웨어 | 0:20 |
| 6 | 진행 상황 | 0:20 |
| 7 | 상태 흐름 | 0:20 |
| 8 | 문제·해결 | 0:15 |
| 9 | 향후 계획 | 0:10 |
| 10 | 마무리 | 0:05 |
| **합계** | | **~3:00** |

---

## 발표 팁

1. **슬라이드 6~7**이 내용이 많으니, 2분 넘으면 슬라이드 8을 짧게 줄이세요.  
2. PPT는 영어, 말은 한국어 대본으로 해도 됩니다. 영어 발표 시 위 English Script 사용.  
3. 시연 영상이 있으면 슬라이드 5 또는 6에서 "We also recorded a Lab 9 demo" 한 줄 추가 가능.  
4. Q&A 예상: Why camera + ultrasonic? / Why no speaker? / Is LLM on Pi?
