# 팀원 B 작업 안내 (현진 브랜치 기준)

README **옵션 1** 분담과, 채팅/이미지에 나온 B 할 일 목록이 **맞는지**, **이미 무엇이 구현돼 있는지**, **실제로 무엇부터 하면 되는지**를 정리한 문서입니다.

---

## 이미지/채팅에 나온 B 할 일 vs README vs 현재 코드

| B 할 일 (채팅 목록) | README에 있나 | 현재 `현진` 브랜치 상태 |
|---------------------|---------------|------------------------|
| ① `RewindBuffer` `PushFrame` / `PopFrame` | ✅ | **이미 구현됨** (`GameSnapshot` 단위 링 버퍼) |
| ② 스태미나·`Z` 3초 소모 | ✅ | **기본만 있음** (`Game.cpp`에서 직접 처리) |
| ③ 스폰 매니저 + 카메라 스크롤 | ✅ | **스켈레톤만** (고정 속도 스크롤, `SpawnProps` 14개 고정 배치) |
| ④ HUD | ✅ | **스태미나 바만** (거리·게임오버·재시작 등 없음) |
| `GameSnapshot` include 후 버퍼 연동 | ✅ (A 담당 스냅샷) | **A가 이미 올림** — 기다릴 필요 없음 |

**결론:** 역할 분담 **방향은 맞음**. 다만 ①·`GameSnapshot` 연동은 **“새로 처음부터”**가 아니라 **기존 코드 확인 → 리팩터링·소유**에 가깝습니다.

---

## B에게 맞는 실제 작업 순서 (추천)

1. **`현진` 브랜치 pull**  
   - `include/rewind/Snapshot.h`, `RewindBuffer.h`, `docs/snapshot-agreement.md` 확인
2. **`Game.cpp`에서 B 담당 로직 분리** (모듈화)  
   - 리와인드 + 스태미나 → 예: `RewindSystem`  
   - 스폰 + 카메라 → 예: `RunnerSystem` / `SpawnManager`  
   - HUD + 일시정지 UI → 예: `GameUI`  
     - 일시정지: `Esc` 일시정지, `Space` 재개, 일시정지 중 `Esc` 종료 (이미 구현됨)
3. **러너 강화** — 동적 스폰, 오브젝트 풀링, 난이도(속도·스폰 밀도·패턴)
4. **HUD·게임 상태** — 거리 표시, 게임오버, 재시작, 디버그 토글
5. **리와인드 엔진 정리** — `Capture` / `Apply` 호출 위치·스태미나 규칙을 B 모듈로 통합

> 채팅 순서 “① `RewindBuffer`부터” → **구현보다 리팩터링·정리**부터 하는 것이 맞습니다.

---

## README에 있지만 채팅 4줄에 없는 B 일

- 게임오버 / 재시작
- 난이도 곡선
- 오브젝트 풀링 (러너 스폰)
- HUD 확장 (거리, 디버그 토글 등)

이 항목들도 B 범위입니다.

---

## A vs B — 겹치지 않게

| 팀원 A (현진) | 팀원 B |
|---------------|--------|
| 폭탄·중력장·객체별 `*Snapshot` 정의 | 버퍼 **사용 구조**, 스태미나·`Z` UX·게임 루프 연동 |
| `GameSnapshot::Capture` / `Apply` **내용** 확장 | 매 프레임 **언제** push/pop 할지, `Game`과 분리 |
| `docs/snapshot-agreement.md` 유지 | 스폰·카메라·HUD·게임 상태 |

`GameSnapshot`에 필드가 늘어나면:

- **A:** 스냅샷 필드·`Capture`/`Apply` 갱신  
- **B:** `RewindBuffer` 용량·호출 타이밍·UI 연동

---

## 이미 있는 파일 (B가 먼저 볼 것)

| 경로 | 설명 |
|------|------|
| `include/rewind/RewindBuffer.h` | `PushFrame` / `PopFrame` (`GameSnapshot`) |
| `include/rewind/Snapshot.h` | 프레임 단위 `GameSnapshot`, `Capture` / `Apply` |
| `include/rewind/BodySnapshot.h` | 공통 `active, pos, vel, onGround` |
| `docs/snapshot-agreement.md` | 사전 합의 3가지 (포맷·ID·역행 규칙) |
| `src/game/Game.cpp` | 스태미나·역행·스크롤·HUD·일시정지 (분리 대상) |

---

## 조작 (B 구현 시 참고)

| 키 | 동작 |
|----|------|
| `Z` | 약 3초 역행 (스태미나 3 소모) |
| `Esc` | 일시정지 |
| `Space` | 일시정지 중 재개 |
| `Esc` (일시정지 중) | 종료 |

---

## B에게 전달할 한 줄

> **`현진` pull 후 `RewindBuffer`·`GameSnapshot`은 이미 있으니, `Game.cpp`에서 B 담당 로직을 모듈로 빼고, 스폰·HUD·게임 상태부터 확장하면 됩니다.**

---

*작성 기준: `현진` 브랜치, 팀원 A 구현(폭탄·중력장·스냅샷·일시정지) 반영.*
