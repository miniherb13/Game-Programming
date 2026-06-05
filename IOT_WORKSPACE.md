# IoT 프로젝트 Git 연동 안내

이 폴더(`Project/`)가 GitHub **Game-Programming** 저장소 **`Iot` 브랜치**와 연결된 작업 폴더입니다.

- **원격:** https://github.com/miniherb13/Game-Programming.git
- **브랜치:** `Iot`

## 폴더 구성

| 파일 | 설명 | 담당 |
|------|------|------|
| `ROLES.md` | A/B 역할 분담 (필독) | 공통 |
| `companion.py` | 메인 통합 루프 | A |
| `reply.py` | 키워드·LLM 답변 | A |
| `config.py` | 공통 설정 | A |
| `motion.py` | 움직임 감지 | B |
| `lcd_actuator.py` | LCD / 버저 / LED | B |
| `reference/` | 참고 코드 (초음파 예제 등) | 참고 |
| `기말 프로젝트.txt` | 기획안 | 공통 |

## Git 명령

```bash
cd Project
git pull origin Iot
git add .
git commit -m "설명"
git push origin Iot
```

## Pi에서 실행

```bash
cd ~/.../Project
pip3 install RPLCD opencv-python-headless gpiozero --break-system-packages
# config.py 의 CAPTURE_PATH 수정 후
python3 companion.py
```

## 상위 폴더 (`Iot/`)

`lab8/`, `lab9/` 등 수업 실습은 상위 `Iot/` 폴더에 두고, Git 추적 대상이 아닙니다.
