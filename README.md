# IoT 기말 프로젝트 — Motion-Adaptive AI Companion Robot

**팀:** 조현진(202304438, A) / 박효담(202304709, 팀장·B)  
**출력:** LCD + 버저 + LED (스피커 없음)  
**입력:** 카메라(움직임) + 초음파(근접) + 터미널 키보드(대화)

> 역할 분담: [docs/ROLES.md](docs/ROLES.md)

---

## 폴더 구조

```
Project/
├── README.md              # 이 파일
├── config.py              # 공통 설정
├── companion.py           # 메인 통합 (A)
├── motion.py              # 카메라 움직임 (A)
├── reply.py               # 키워드 응답 (A)
├── test_motion.py         # motion 단독 테스트 (A)
├── lcd_actuator.py        # LCD/LED/버저/초음파 (B)
├── ultrasonic.py          # 초음파 거리 함수
├── docs/
│   ├── ROLES.md           # A/B 역할 분담
│   ├── IOT_WORKSPACE.md   # Git 연동 안내
│   ├── 기말 프로젝트.txt   # 기획안
│   └── midterm/           # 중간 발표 자료
│       ├── MIDTERM_QA.md
│       ├── MIDTERM_SCRIPT.md / .docx
│       ├── MIDTERM_PRESENTATION.md
│       └── IoT_Midterm_Presentation.pptx / .pdf
├── reference/             # 참고 코드·초안
├── lab8/                  # Lab8 실습 참고
├── lab9/                  # Lab9 시연 영상
└── scripts/               # 유틸 스크립트
```

**Pi에서 실행하는 코드는 루트에만 있습니다.** `docs/`, `reference/`는 문서·참고용입니다.

---

## 빠른 시작 (Pi)

```bash
cd ~
git clone -b Iot https://github.com/miniherb13/Game-Programming.git Iot
cd ~/Iot
pip3 install RPLCD opencv-python-headless gpiozero --break-system-packages
mkdir -p ~/iot_lab8

git pull origin Iot
python3 companion.py
```

### 모듈 단독 테스트

```bash
python3 -c "from motion import detect_motion; print('motion OK')"
python3 -c "from reply import get_reply; print(get_reply('hello'))"
python3 test_motion.py live
python3 lcd_actuator.py
```

종료: `Ctrl+C`

---

## 시연 시나리오

1. 대기 — LCD `Companion Bot` + 거리(cm)
2. 손 흔들기 **또는** 20cm 이내 → LCD `Hi there!` + LED + 버저
3. 터미널 영어 입력 (`hello`) → LCD 답변

---

## 배선 (BCM)

| 부품 | GPIO | 물리 핀 |
|------|------|---------|
| 초음파 TRIG / ECHO | 16 / 18 | 36 / 12 |
| LED | 17 | 11 |
| 버저 I/O | **27** | 13 |
| LCD I2C | SDA/SCL | 3 / 5 |

⚠️ 버저는 **GPIO 18 금지** (ECHO와 충돌)

---

## Git

```bash
git pull origin Iot
git add .
git commit -m "설명"
git push origin Iot
```

브랜치: https://github.com/miniherb13/Game-Programming/tree/Iot
