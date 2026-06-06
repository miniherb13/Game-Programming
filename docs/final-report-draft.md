# Chrono Rush — 프로젝트 최종 보고서 (초안)

> **과목:** [Global] Game Programming  
> **프로젝트명:** Chrono Rush  
> **팀:** [TODO: 팀명]  
> **팀원:** [TODO: 학번 · 이름 · 역할] (A / B / 조현진)  
> **제출일:** 2026년 6월 6일  
>
> ⚠️ 이 파일은 **초안**입니다. PDF 제출 전 `[TODO]`·`[캡처]`를 모두 채우고, `docs/report-writing-guide.md`의 분담표에 따라 팀원이 수정하세요.

---

## 1. 개요

### 1.1 프로젝트 목표

본 프로젝트 **Chrono Rush**는 Unity·Unreal 등 **상용 게임 엔진 없이**, C++과 SDL2만으로 2D 러너(Endless Runner)를 구현하는 것을 목표로 한다. 단순 달리기를 넘어 **시간 역행(Rewind)**, **슬로우 모션**, **폭탄·중력장(블랙홀)** 을 결합해 “시간을 조작하는 러너”라는 컨셉을 코드 수준에서 검증하였다.

### 1.2 게임 한 줄 소개

플레이어는 자동으로 스크롤되는 맵을 달리며 장애물과 낙하물을 피하고, **폭탄(X)** 으로 길을 열며, **역행(Z)** 과 **슬로우(F)** 로 위기를 넘긴다. HP는 이동 거리에 비례해 감소하며, 3개 스테이지·총 15km를 완주하면 클리어 연출이 재생된다.

### 1.3 개발 환경

| 항목 | 내용 |
|------|------|
| 언어 | C++20 |
| 그래픽·입력 | SDL2 2.30.6 (FetchContent) |
| 빌드 | CMake + Ninja / Visual Studio 2022 |
| 플랫폼 | Windows (MSVC) |
| 버전 관리 | GitHub (`miniherb13/Game-Programming`) |
| 에셋 | PNG 스프라이트, stb_image 로드, 빌드 시 `assets/` 복사 |

실행: 프로젝트 루트에서 `.\build.ps1 -Run` (MSVC 경로 자동 설정).

### 1.4 팀 구성 및 역할

| 역할 | 담당 | 주요 기여 |
|------|------|-----------|
| **A** | [TODO: 이름] | 폭탄 물리·투척, 중력장(블랙홀), `GameSnapshot`·객체별 스냅샷, 폭발 연동 |
| **B** | [TODO: 이름] | `RewindBuffer`, 스폰·스크롤·난이도, F 슬로우·스태미나 분리, 낙하 장애물 |
| **조현진** | UI·통합 | 3스테이지(Emerald), HUD·타이틀·조작법, `UiText`·VFX, 빌드 스크립트, 통합·버그 수정 |

---

## 2. 게임 기획

### 2.1 컨셉 및 테마

**키워드:** 시간, 시계, 역행, 러너, SF.

플레이어 실루엣과 시계·되감기 화살표를 타이틀·역행 VFX에 사용하였다. 스테이지는 **Mars → Glacier → Emerald** 순으로 환경색과 장애물 밀도가 변하며, “행성을 가로지르는 시간 여행 러너”라는 내러티브를 시각적으로 전달한다.

`[캡처: 타이틀 화면 — CHRONO RUSH 로고 + SPACE 안내]`

### 2.2 핵심 게임플레이 루프

1. **달리기** — 카메라는 플레이어 X를 고정하고 맵이 스크롤된다.  
2. **장애물 회피** — 점프(C), 실드(별 아이템), 폭탄 파괴.  
3. **자원 관리** — HP(거리), 슬로우 스태미나(파랑), 역행 스태미나(보라).  
4. **시간 조작** — F 슬로우, Z 역행(약 3초 분량 스냅샷 복원).  
5. **클리어 / 게임오버** — 15km 도달 시 Clear! + 폭죽, HP 0 또는 낙사 시 Game Over.

### 2.3 조작법

| 입력 | 동작 |
|------|------|
| **C** | 점프 (코요테 타임·점프 버퍼 적용) |
| **X** 홀드 / 떼기 | 폭탄 충전 → 발사 (마우스 조준, 점선 궤도) |
| **F** | 슬로우 모션 ON/OFF (슬로우 스태미나 소모) |
| **Z** | 시간 역행 (역행 스태미나 1.5 소모, 약 3초 되감기) |
| **Esc** | 일시정지 / 일시정지 중 종료 |
| **Space** | 타이틀 → 조작법 → 게임 시작 / 일시정지 해제 / 재시작 |

아이템: **번개 = 슬로우 스태미나**, **하트 = HP**, **별 = 실드(무적 5초)**.

`[캡처: 인게임 조작법 오버레이 — 맵 위 패널]`

시작 흐름: **타이틀(Space)** → **인게임 배경 + 조작법 패널(Space)** → **본 게임**.

### 2.4 스테이지 구성

| 스테이지 | 시작 거리 | 길이 | 특징 |
|----------|-----------|------|------|
| **Mars** | 0 m | 5 km | 화성 타일·장애물 8종, 블랙홀 VFX 배경 |
| **Glacier** | 5 km | 5 km | 빙하 테마, 낙하 장애물(유성) 스폰 시작 |
| **Emerald** | 10 km | 5 km | 에메랄드 테마, 난이도·낙하 빈도 상승 |
| **클리어** | 15 km | — | Clear! 연출, Space로 타이틀 복귀 |

스테이지 전환 시 페이드·알림 UI(`Stage 2`, `Stage 3`)가 표시된다.

`[캡처: Mars / Glacier / Emerald 각 1장 — 작게 3열 배치]`

### 2.5 장애물·아이템

- **장애물 8종:** Normal, Tall, Bounce, Spike, Triangle, Ceiling, Moving + **낙하형(Falling)**  
- Tall 등 일부는 폭탄으로 파괴 가능.  
- 스프라이트는 스테이지별 PNG, 흰 테두리 bake(`SpriteOutline`)로 시인성 확보.  
- **HP:** 이동 거리에 따라 감소. 생존 가능 거리 ≈ 맵 길이의 150% (`kHpSurvivalDistanceM`).

### 2.6 기획 변경 사항

개발 중 컨셉·UX를 아래와 같이 조정하였다. **실패가 아니라 플레이 테스트·구현 비용을 반영한 개선**이다.

| 변경 항목 | 초기 방향 | 최종 방향 | 변경 이유 |
|-----------|-----------|-----------|-----------|
| 중력장 | V키로 수동 배치 (인력/척력) | **폭탄 폭발 시 블랙홀** | 조작 수 과다, 폭탄과 루프 분리 → “폭탄=공격+블랙홀”로 통합 |
| 역행 스태미나 | 단일 스태미나 | **슬로우(파랑) / 역행(보라) 분리** | F·Z 동시 남용 방지, HUD 가독성 |
| 슬로우 | (미구현) | **F 토글 + simDt 통일** | 물리·스폰·스태미나가 동일 시간 스케일을 쓰도록 정리 |
| 시작 UI | 타이틀에 조작법 동시 표시 | **타이틀 → 인게임 조작법** | 타이틀 아트와 UI 분리, 맵 미리보기 |
| 클리어 | 10km 도달 시 즉시 재시작 | **15km Clear! + 폭죽, Space로 타이틀** | 3스테이지(15km) 구조에 맞춤 |
| 실드 연출 | 노란 사각 테두리 | **타원형 파티클 오라** | 시인성·테마 통일 |

`[TODO: 팀원 A/B — 본인 담당 변경 1줄씩 추가]`

---

## 3. 시스템 구조

### 3.1 전체 아키텍처

```
main.cpp
  └─ App (SDL 윈도우, 메인 루프)
       ├─ Clock        — 가변 dt
       ├─ Input        — 키·마우스, 프레임당 1회 소비 규칙
       └─ Game         — 규칙·렌더·스폰
            ├─ PhysicsWorld   — 고정 timestep, 원-원 충돌
            ├─ RewindBuffer   — GameSnapshot × 180프레임
            ├─ BombSystem / GravityFieldSystem
            ├─ StageMap / StageGlacier / StageEmerald
            └─ VfxLibrary / UiText / PlayerSprite …
```

`[캡처: 위 다이어그램을 draw.io 또는 PowerPoint로 그려 삽입]`

### 3.2 디렉터리 구조

| 경로 | 역할 |
|------|------|
| `src/app/` | 앱 진입·메인 루프 |
| `src/core/` | Clock, Input, Log |
| `src/physics/` | PhysicsWorld, Body |
| `src/rewind/` | BodySnapshot, Snapshot, RewindBuffer |
| `src/game/` | Game, Bomb, GravityField, Stage*, Sprites |
| `src/render/` | UiText, VfxLibrary, SpriteOutline |
| `assets/` | player, stages, items, vfx, title |
| `docs/` | snapshot-agreement, 보고서 초안 |

### 3.3 게임 루프

1. **입력** — `Input::Pump` → `HandleInput` (슬로우 토글, 역행 큐 등)  
2. **고정 업데이트** — `IsGameplayActive()`일 때만 `FixedUpdate(1/60s)` 누적  
3. **가변 업데이트** — `UpdateVisualEffects` (역행 VFX, 블랙홀 파티클)  
4. **렌더** — 배경 → 오브젝트 → HUD → 오버레이(일시정지·조작법·Clear)

슬로우 활성 시 `simDt = dt × 0.35`로 물리·스폰·HP 감소·스태미나 소모를 **동일 스케일**로 처리한다.

### 3.4 스냅샷·역행 데이터 흐름

```
FixedUpdate (매 1/60s)
  GameSnapshot::Capture(플레이어, 폭탄×16, 중력장×6, 상자…)
  RewindBuffer::PushFrame

Z 입력 + 스태미나 ≥ 1.5 + 버퍼 가득
  180 × PopFrame → GameSnapshot::Apply
  역행 VFX, 0.5s freeze, 무적 2s
```

상세 필드·ID 정책은 `docs/snapshot-agreement.md` 참고.

---

## 4. 핵심 기술 구현

> **4.1** → B 보완 / **4.2** → A 보완 / **4.3** → B + 조현진 보완

### 4.1 시간 역행 시스템 `[담당: B]`

#### 4.1.1 RewindBuffer

- **용량:** 180프레임 ≈ **3초** (고정 timestep 60Hz)  
- **단위:** 프레임마다 `GameSnapshot` 전체 저장 (플레이어·폭탄·필드·상자·게임 상태)  
- **API:** `PushFrame(const GameSnapshot&)`, `PopFrame(GameSnapshot&)`

#### 4.1.2 복원 규칙

- 해당 프레임에 저장된 상태를 **그대로** 복원한다.  
- 폭발·블랙홀 생성 **이후** 역행하면, 그 **이전** 스냅샷 기준으로 폭탄·필드가 취소된다.  
- 슬롯 인덱스 고정(폭탄 16, 중력장 6)으로 풀링 bodyId와 스냅샷 인덱스가 어긋나지 않게 설계하였다.

#### 4.1.3 게임플레이 연동

| 항목 | 값·동작 |
|------|---------|
| 역행 스태미나 소모 | 1.5 |
| 쿨다운 | 역행 직후 동일량 |
| 역행 성공 시 | 0.5s freeze, 2s 무적, 슬로우 해제 |
| 실패 시 | 포즈·VFX 없음 (스태미나·버퍼 부족) |

#### 4.1.4 역행 VFX `[조현진 — 연출·버그 수정]`

- 플레이어 과거 위치 **유령(Ghost)** + 수렴 파티클 + 시계 소용돌이  
- 유령: 달리기 스프라이트 + 파란 tint (어두운 사각 박스 아티팩트 제거)  
- 비네트 **코ner 검은 사각형 제거**, 화면 중앙 고정 포즈 제거 → **실제 좌표**에 캐릭터 표시  

`[캡처: Z 사용 직후 — 유령 + 소용돌이]`

```cpp
// Game.cpp — 역행 성공 시 (요약)
if (any) {
  m_snapshotScratch.Apply(...);
  m_staminaRewind = std::max(0.0f, staminaBeforeRewind - kRewindStaminaCost);
  SpawnRewindVfx(rewoundPlayer.pos);
  m_rewindFreezeLeft = kRewindFreezeSeconds;
  ...
}
```

---

### 4.2 폭탄 및 중력장(블랙홀) `[담당: A]`

#### 4.2.1 폭탄

- **X 홀드:** 충전(`throw_0`→`throw_1`), **X 떼기:** 발사(`throw_2` 포즈)  
- **조준:** 마우스 방향, 점선 궤도(`BombTuning::trajectory*`)  
- **물리:** Dynamic Body, 바운스·마찰, 바닥·장애물 충돌  
- **폭발:** 퓨즈 또는 충돌 조건 → 반경 95px, 장애물·낙하물 파괴, 점수 +50  

#### 4.2.2 중력장(블랙홀)

- **트리거:** 폭탄 폭발 시 `GravityFieldSystem::SpawnFromExplosion`  
- **힘 모델:** 거리 제곱 반비례 + 최소 거리·최대 힘 클램프 (`GravityFieldTuning`)  
- **지속:** 약 1.25초, 최대 6개 동시  
- **연출:** `VfxLibrary::DrawBlackHole` — 소용돌이· accretion · shockwave  

`[캡처: 폭탄 궤도 + 폭발 후 블랙홀]`

#### 4.2.3 스냅샷 (A)

- `BombSlotSnapshot`: phase, fuse, explosionCenter 등  
- `GravityFieldSnapshot`: active, center, radius, timeLeft  
- `GameSnapshot::Capture` / `Apply`에서 일괄 저장·복원  

`[TODO: A — BombSlotSnapshot 필드 표 1개 추가]`

---

### 4.3 물리·러너·스테이지 `[담당: B + 조현진]`

#### 4.3.1 PhysicsWorld

- **MotionType:** Player = **Kinematic** (`lockVelX`), 폭탄 = Dynamic, 장애물 = Static/Dynamic  
- **충돌:** Dynamic끼리 탄성 반응; Player–Obstacle은 **Y만 분리** (스크롤 X 유지)  
- **실드:** `ignoreObstacleContact` — 무적 중 장애물에 밀려 즉시 착지하는 문제 방지  

#### 4.3.2 러너·스폰

- 플레이어 화면 X 고정(~140px), `m_distance`·`m_scrollSpeed` 증가  
- `UpdateSpawn` — 패턴 기반 장애물·아이템 스폰  
- Glacier 이후 **낙하 장애물**, Emerald에서 **스폰 간격 단축** (B)  

#### 4.3.3 UI·타이틀 `[조현진]`

- **UiText:** Windows GDI(맑은 고딕) 한글 HUD  
- **HUD:** Cookie Run 스타일 HP·슬로우/역행 스태미나·거리·점수·시간  
- **타이tle:** `title_screen_v2.png`, 비율 유지(contain), 배경색 `#080A12`  
- **실드:** 노란 타원 궤도 **파티클 20개** 회전  

`[캡처: HUD + 실드 오라]`

---

## 5. 협업 및 설계 의사결정

### 5.1 사전 합의 (`docs/snapshot-agreement.md`)

1. **공통 포맷:** `BodySnapshot` — active, pos, vel, onGround  
2. **ID 정책:** 폭탄·중력장·상자 **고정 슬롯 인덱스**  
3. **역행 규칙:** 저장된 프레임 그대로 복원 (폭발 취소 포함)  

A가 스냅샷 **내용**을, B가 **버퍼·호출 타이밍·UI**를 담당하는 분업이었다.

### 5.2 Git 협업

- 브랜치: `main`, `현jin` / `현진` 등 — 기능별 merge  
- 대표 커밋: 슬로우·스태미나 분리, Emerald 스테이지, 타이틀·UI, 역행 버그 수정  
- 빌드: `build.ps1` / `build.cmd`로 팀원 PC 환경 통일  

### 5.3 기술적 트레이드오프

| 결정 | 이유 |
|------|------|
| 플레이어 X = 스크롤 속도 | 러너 특성상 레인 고정, 장애물은 Y 위주 처리 |
| 엔진 미사용 | 과제 목표·저수준 제어 학습 |
| stb_image + SDL 텍스처 | 의존성 최소화, PNG/JPEG 혼용 가능 |

---

## 6. 개발 과정 및 문제 해결

| # | 문제 | 원인 | 해결 | 담당 |
|---|------|------|------|------|
| 1 | 타이틀 로드 후 즉시 종료 | `stbi_image_free` 후 surface가 참조한 메모리 해제 (UAF) | vector 복사 후 GPU 업로드 | 조현진 |
| 2 | 실드 중 장애물 접촉 시 즉시 착지 | Y 밀기 + CheckCollision만 면역 | `ignoreObstacleContact`, PreventClimb 스킵 | 조현진 |
| 3 | 역행 시 캐릭터 자리 검은 사각형 | Ghost dark colorMod + 비네트 corner | Ghost tint 수정, corner 제거 | 조현진 |
| 4 | 역행 연발·스태미나 오류 | 입력·스냅샷 타이밍 | 1회 소비, 분리 스태미나 | B |
| 5 | 스프라이트 안 보임 | GPU upload가 Draw 조건 뒤 | `EnsureUploaded()` 선행 | 조현진 |

`[TODO: A/B — 본인 해결 사례 1건 추가]`

---

## 7. 결과 및 평가

### 7.1 구현 완료 기능

| 기능 | 상태 |
|------|------|
| SDL2 윈도우·60Hz 고정 업데이트 | ✅ |
| 3스테이지·15km·클리어 연출 | ✅ |
| 폭탄·블랙홀 | ✅ |
| 시간 역행(3초)·스냅샷 | ✅ |
| F 슬로우·이중 스태미나 | ✅ |
| HUD·한글 UI·타이틀·조작법 | ✅ |
| 장애물 8종 + 낙하물 | ✅ |
| 아이템 3종(체력·스태미나·실드) | ✅ |
| Git 협업·CMake 빌드 | ✅ |

### 7.2 시연

`[TODO: 플레이 영상 URL 또는 “로컬 exe 시연”]`

### 7.3 한계 및 향후 계획

- **사운드 BGM/SFX 미구현**  
- **밸런스·15km 클리어 시간** 미세 조정 필요  
- **모듈 분리** — `Game.cpp` 단일 파일 비대 → RewindSystem·SpawnManager 분리 예정  
- **팀원 추가 작업 중** — zip 제출 전 최종 통합·QA  

`[TODO: 팀원 — 진행 중인 기능 1줄]`

---

## 8. 결론

Chrono Rush는 **SDL2 + 자체 물리**만으로 2D 러너와 **시간 조작(역행·슬로우)** 을 구현하였다. 특히 **프레임 단위 GameSnapshot**과 **폭탄–블랙홀 연동**은 상용 엔진 없이도 일관된 게임플레이를 만들 수 있음을 보여 주었다.

팀원 A·B와 **스냅샷 API 사전 합의** 후 병렬 개발한 경험이, 역행과 폭발이 동시에 동작하는 데 핵심이었다. UI·스테이지·VFX 통합과 버그 수정을 통해 **플레이 가능한 데모** 수준까지 끌어올렸으며, zip 제출·사운드·밸런스는 후속 작업으로 남긴다.

---

## 참고

- 저장소: https://github.com/miniherb13/Game-Programming  
- `README.md`, `docs/snapshot-agreement.md`, `docs/team-b-tasks.md`  
- SDL2: https://wiki.libsdl.org/  

---

## 부록 A. 조현진 담당 작업 상세 (초안 작성자 메모)

팀원이 역할 확인용으로 참고. PDF 본문에는 **1.4·6장 표** 정도만 남기고 이 부록은 **제출 PDF에서 삭제**해도 됩니다.

- Emerald 3스테이지(`StageEmerald`), 15km 맵·클리어  
- 타이틀 PNG·비율 유지 렌더·시작/조작법 2단계 UI  
- `UiText` 한글, HUD 레이아웃, Clear/GameOver/일시정지  
- `VfxLibrary` 역행·블랙홀 연출, 실드 파티클 오라  
- `SpriteOutline`, 장애물·아이템 시인성  
- `build.ps1` / `build.cmd`, README 빌드 안내  
- 통합 merge, 역행·실드·타이tle UAF 등 버그 수정  

---

*문서 버전: 2026-06-05 초안 — `docs/report-writing-guide.md`와 함께 수정*
