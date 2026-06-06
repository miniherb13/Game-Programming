# 스냅샷 사전 합의 (팀원 A 구현 기준)

팀원 A(폭탄/중력장/스냅샷) 구현 시 확정한 3가지입니다. 팀원 B(리와인드 버퍼·러너)와 맞출 때 이 문서를 기준으로 합니다.

---

## 1. 스냅샷 공통 포맷

### 공통층: `BodySnapshot`

모든 물리 Body는 최소 아래 필드를 저장합니다.

| 필드 | 설명 |
|------|------|
| `active` | 객체 활성 여부 |
| `pos` | 위치 |
| `vel` | 속도 |
| `onGround` | 지면 접촉 (점프/착지 판정용, README 최소 3개에 추가) |

- 헤더: `include/rewind/BodySnapshot.h`
- API: `BodySnapshot::Save(const Body&)`, `Load(Body&)`

### 객체별 추가 필드

| 객체 | 스냅샷 타입 | 공통 + 추가 |
|------|-------------|-------------|
| 플레이어 | `GameSnapshot::player` | `BodySnapshot` + `jumpBuffer`, `coyote`, `stamina` |
| 폭탄(슬롯당) | `BombSlotSnapshot` | `physics`(BodySnapshot) + `phase`, `fuseLeft`, `visualLeft`, `explosionCenter`, `wasOnGround`, `prevVelY` |
| 중력장(슬롯당) | `GravityFieldSnapshot` | `active`, `mode`, `center`, `radius`, `timeLeft` (+ `fieldManualCooldown`는 GameSnapshot에 1개) |
| 상자 | `BodySnapshot` | 공통만 (별도 로직 필드 없음) |

### 프레임 단위

한 프레임 전체는 `GameSnapshot` (`include/rewind/Snapshot.h`)에 모읍니다.

- `Capture(...)` — 시뮬레이션 전 저장
- `Apply(...)` — 역행 시 복원
- `RewindBuffer` — `GameSnapshot`을 180프레임(약 3초) 링 버퍼에 `PushFrame` / `PopFrame`

---

## 2. 객체 ID 정책

역행 중에도 **같은 논리 객체**는 **고정 슬롯 인덱스**로 식별합니다. 풀링으로 `bodyId`가 바뀌지 않도록, 슬롯과 Body는 초기화 시 1:1로 묶습니다.

| 종류 | ID | 개수 | 비고 |
|------|-----|------|------|
| 폭탄 | 슬롯 `0 .. 15` | 16 | `BombSlot.bodyId`는 `InitPool` 시 고정 |
| 중력장 | 슬롯 `0 .. 5` | 6 | `GravityFieldTuning::maxFields` |
| 상자 | `m_propIds[i]` 순서 | 최대 32 | 생성 순서 고정 |

- 예전 방식(`RewindState`만, 날아가는 폭탄만 순서대로)은 사용하지 않습니다.
- `Body::rewindId`는 예약 필드이며, 현재 구현에서는 미사용.

---

## 3. 삭제 / 풀링 / 폭발의 역행 규칙

**한 줄 규칙:** 해당 프레임에 저장된 상태를 **그대로** 복원한다. 폭발·필드 생성 **이후** `Z`로 되감으면 **그 이전** 상태로 돌아간다.

| 상황 | 역행 시 |
|------|---------|
| 폭탄 비행 중이던 시점 | `Flying`, 위치·속도·퓨즈 복원 → 다시 날아감 |
| 폭탄 폭발 이후 | 과거 스냅샷 기준으로 비행/비활성 상태 복귀 → 폭발 **취소** |
| 중력장 생성 이후 | 과거에 없던/꺼진 필드로 복귀 → 필드 **사라짐** |
| 상자가 밀린 이후 | `props[i]` pos/vel 복원 → 위치 되돌림 |

**“되감으면 다시 살아나는가?”** → **예** (그 시점 스냅샷에 살아 있었다면). 폭발로 사라진 폭탄도, 되감기 목표 시점에 비행 중이면 다시 나타납니다.

### 코드 흐름 (`Game::FixedUpdate`)

1. `GameSnapshot::Capture` → `m_rewind.PushFrame`
2. `Z` + 스태미나 ≥ 3초 → 180프레임 `PopFrame` → `GameSnapshot::Apply`
3. 이후 폭탄/중력장/물리 시뮬레이션 진행

---

## 팀원 B 연동 시 참고

- `RewindBuffer` API: `PushFrame(const GameSnapshot&)`, `PopFrame(GameSnapshot&)`
- 객체 수가 늘어나면 `GameSnapshot`에 필드/배열을 추가하고 `Capture`/`Apply`만 확장하면 됩니다.
- 슬롯 인덱스 정책을 바꾸면 A/B 모두 저장·복원 순서를 다시 맞춰야 합니다.
