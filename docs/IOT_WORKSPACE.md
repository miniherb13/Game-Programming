# IoT 프로젝트 Git 연동 안내

- **원격:** https://github.com/miniherb13/Game-Programming.git
- **브랜치:** `Iot`

## 폴더 안내

| 경로 | 용도 |
|------|------|
| 루트 `*.py` | Pi에서 실행하는 코드 |
| `docs/` | 기획·역할·발표 자료 |
| `docs/midterm/` | 중간 발표 PPT, 대본, Q&A |
| `reference/` | 참고 코드·초안 (`led_lcd.txt` 등) |
| `lab8/`, `lab9/` | 수업 실습 참고 |

## Git 명령

```bash
cd Project   # 또는 Pi: cd ~/Iot
git pull origin Iot
git add .
git commit -m "설명"
git push origin Iot
```

## Pi 실행

```bash
cd ~/Iot
python3 companion.py
```
