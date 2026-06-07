# IoT Midterm Q&A — 한국어 / English

**Project:** Motion-Adaptive AI Companion Robot  
**Team:** Park Hyodam (202304709) · Jo Hyeonjin (202304438)

---

# Part 1 — 한국어 Q&A

## 교수님 예상 질문

### Q1. 카메라와 초음파 센서를 왜 둘 다 쓰나요? 하나만으로는 안 되나요?

**A:** 카메라는 **손을 흔드는 것 같은 motion**을 감지하고, 초음파는 **20cm 이하로 가까이 오는 proximity**를 감지합니다. 둘을 함께 쓰면 사용자 상황을 더 잘 포착할 수 있어서 “motion-adaptive” 목표에 더 맞습니다. 센서 하나만 쓰면 놓치는 상황이 생깁니다.

---

### Q2. idle, approach, stay, alert 상태의 차이는 무엇인가요?

**A:** **Idle**은 사용자가 없는 대기 상태입니다. **Approach**는 20cm 이하로 접근한 상태입니다. **Stay**는 사용자가 근처에 머무르며 대화를 유도하는 상태입니다. **Alert**는 갑작스러운 큰 움직임에 반응하는 상태입니다. 현재는 idle, motion, reply 기본 상태까지 구현했고, 최종에는 이 흐름을 더 세분화할 예정입니다.

---

### Q3. 키워드만 쓰는데 LLM 기반 프로젝트가 맞나요?

**A:** 중간 발표 시점에서는 Pi에서 **keyword matching**이 기본입니다. 네트워크가 되면 **Gemini API**로 더 자연스러운 응답을 보강합니다. Pi에서 로컬 LLM 전체를 돌리기에는 성능과 시간이 부족해서 단계적으로 적용하고 있습니다.

---

### Q4. 버저는 왜 GPIO 27번인가요?

**A:** 초음파 센서 **ECHO 핀이 GPIO 18**을 사용하기 때문에, 버저를 18번에 두면 **핀 충돌**이 발생합니다. 그래서 버저를 **GPIO 27번**으로 분리했습니다.

---

### Q5. 스피커가 없는데 사용자는 어떻게 로봇의 답을 확인하나요?

**A:** 스피커 모듈이 없어서 답변은 **LCD 텍스트**로 표시합니다. 최종 버전에서는 offline voice-assistant 참고자료를 활용해 음성 출력을 추가할 수 있습니다.

---

### Q6. `detect_motion()`은 어떻게 동작하나요?

**A:** 2초마다 `rpicam-still`로 사진을 찍고, **중앙 ROI**의 grayscale 프레임 차이 평균을 계산합니다. 그 값이 **threshold 25**를 넘으면 motion으로 판단합니다. Lab 8 OpenCV 실습을 기반으로 합니다.

---

### Q7. `show_motion()`에서는 무엇이 일어나나요?

**A:** LED가 켜지고, LCD에 인사 문구가 표시되며, **버저가 0.4초** 울립니다. `led_lcd.txt` 초안에서 시작해 현재는 버저까지 `show_motion()`에 통합했습니다.

---

### Q8. 지금까지 실제로 무엇을 완료했나요?

**A:** LCD·LED·버저 제어, 초음파 거리 감지, OpenCV motion 감지, idle/motion/reply 기본 상태를 완료했습니다. `companion.py` 통합 테스트를 진행 중이며, Lab 9 터치 센서와 LED 2개 제어 실습도 마쳤습니다.

---

### Q9. 최종 제출 전에 무엇이 남았나요?

**A:** 통합 안정화, approach/stay/alert 상태 정교화, 감정형 응답 개선, 오류 처리, 최종 데모 영상과 보고서 작성이 남아 있습니다.

---

### Q10. 팀원 역할은 어떻게 나눴나요?

**A:** 조현진(A)은 `motion.py`, `reply.py`, `companion.py`를 담당합니다. 박효담(B, 팀장)은 `lcd_actuator.py`, 하드웨어, 보고서 취합을 담당합니다.

---

## 학생 예상 질문

### Q11. 인터넷 없이도 동작하나요?

**A:** motion 감지, LCD, LED, 버저는 **오프라인**으로 동작합니다. 더 풍부한 텍스트 응답은 키워드 또는 API가 필요합니다.

---

### Q12. 얼굴이나 감정 인식도 하나요?

**A:** 아직은 아닙니다. 현재는 motion, distance, rule/keyword 기반 응답입니다. 시간이 되면 확장할 수 있습니다.

---

### Q13. 초음파 센서가 -1을 반환하면 어떻게 하나요?

**A:** 센서 오류로 보고, 카메라 motion만 사용하거나 idle/error 메시지를 표시합니다. 오류 처리 강화는 향후 계획에 포함되어 있습니다.

---

### Q14. Lab 9 터치 센서 과제와 무엇이 다른가요?

**A:** Lab 9는 **터치 입력 → LED 출력**의 단순 GPIO 과제입니다. 기말 프로젝트는 **카메라, 초음파, LCD, 대화**가 합쳐진 **AIoT 시스템**입니다.

---

## 모르는 질문이 나왔을 때

> “좋은 질문입니다. 아직 완전히 구현하지는 못했지만, 최종 버전 계획에 포함되어 있습니다.”

---

## 외워두기 좋은 핵심 3개 (한국어)

1. **왜 카메라+초음파?** → motion과 proximity는 다른 상황을 잡는다.  
2. **왜 버저 27번?** → 18번은 초음파 ECHO 전용이다.  
3. **완료 vs 예정?** → 센서·액추에이터는 됐고, 상태 분류·감정 응답을 다듬는 중이다.

---

---

# Part 2 — English Q&A

## Professor — Likely Questions

### Q1. Why do you use both a camera and an ultrasonic sensor? Isn't one enough?

**A:** The camera detects **motion**, such as waving a hand. The ultrasonic sensor detects **proximity**, when someone comes within 20 cm. Together, they make the system more responsive and better match our “motion-adaptive” goal. With only one sensor, we would miss some situations.

---

### Q2. What is the difference between idle, approach, stay, and alert?

**A:** **Idle** means no user is detected. **Approach** means the user is within 20 cm. **Stay** means the user remains nearby, and we ask “Want to talk?” **Alert** is for sudden large motion. Right now, we have basic states — idle, motion, and reply — and we plan to refine this state flow for the final demo.

---

### Q3. Is this really “LLM-based” if you only use keyword matching?

**A:** At this midterm stage, **keyword matching** is our main method on the Pi. When the network is available, we optionally use the **Gemini API** for richer replies. A full offline LLM on the Pi is too slow for now, so we are adding it step by step.

---

### Q4. Why is the buzzer on GPIO 27 instead of GPIO 18?

**A:** The ultrasonic **ECHO pin uses GPIO 18**, so we had a pin conflict. We moved the buzzer to **GPIO 27** to avoid that problem.

---

### Q5. Why is there no speaker? How does the user get the robot’s reply?

**A:** We do not have a speaker module, so replies are shown as **text on the LCD**. For the final version, we may add voice output using the offline voice-assistant reference.

---

### Q6. How does `detect_motion()` work?

**A:** Every two seconds, we capture a frame with `rpicam-still`. We compare the **center ROI** grayscale difference between frames. If the average difference is above the **threshold of 25**, we return motion detected. This is based on our Lab 8 OpenCV practice.

---

### Q7. What happens in `show_motion()`?

**A:** The LED turns on, a greeting appears on the LCD, and the **buzzer beeps for 0.4 seconds**. We started from the `led_lcd.txt` draft and have now integrated the buzzer into `show_motion()`.

---

### Q8. What have you actually completed so far?

**A:** We have completed LCD, LED, and buzzer control, ultrasonic detection, OpenCV motion detection, and basic states — idle, motion, and reply. We are currently working on `companion.py` integration testing. We also completed the Lab 9 touch sensor and dual-LED exercise.

---

### Q9. What remains before the final submission?

**A:** We still need to stabilize integration, refine approach/stay/alert states, improve emotional replies, add error handling, and prepare the final demo video and report.

---

### Q10. How are tasks divided between team members?

**A:** **Jo Hyeonjin (A)** works on `motion.py`, `reply.py`, and `companion.py`. **Park Hyodam (B, captain)** works on `lcd_actuator.py`, hardware, and report coordination.

---

## Student — Likely Questions

### Q11. Does it work without internet?

**A:** Yes. Motion detection, LCD, LED, and buzzer work **offline**. For better text replies, we can use keywords offline or the API when internet is available.

---

### Q12. Can it recognize faces or emotions?

**A:** Not yet. Currently, we use motion, distance, and rule/keyword-based responses. Face or emotion recognition may be added later if we have time.

---

### Q13. What if the ultrasonic sensor returns -1?

**A:** That means a sensor error. In that case, we can still rely on camera motion, or show an idle/error message. Better error handling is part of our future plan.

---

### Q14. How is this different from the Lab 9 touch sensor task?

**A:** Lab 9 was a simple **touch input → LED output** task. Our final project combines **camera, ultrasonic, LCD, and chat** into one **AIoT system**.

---

## If you don't know the answer

> “That is a good question. We have considered it, but we have not fully implemented it yet. It is part of our plan for the final version.”

---

## Top 3 to Memorize (English)

1. **Why camera + ultrasonic?** — “Motion and proximity detect different situations.”  
2. **Why buzzer on GPIO 27?** — “GPIO 18 is used by the ultrasonic ECHO pin.”  
3. **Done vs. planned?** — “Sensors and actuators work; we are refining states and emotional replies.”
