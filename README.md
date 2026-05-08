# Game-Programming

상용 엔진 없이(SDL2 + 자체 물리) 2D 런게임을 만드는 최소 골격입니다.

## Build

### Windows (Visual Studio)

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

실행 파일은 보통 `build/Debug/chrono_rush_demo.exe`에 생성됩니다.

### Windows (Ninja)

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Controls

- `LMB 드래그 후 놓기`: 폭탄 투척(데모: 원형 투사체)
- `Space`: 점프(데모: 플레이어 원)
- `R`: 3초 시간 역행(데모: 플레이어/폭탄 상태)
- `Esc`: 종료

## Repo structure

- `include/`: 헤더(수학/물리/리와인드/게임)
- `src/`: 구현
  - `app/`: 앱/윈도우/렌더 루프
  - `physics/`: 고정 timestep 물리 + 충돌(간단)
  - `rewind/`: 원형 버퍼 기반 리와인드
  - `game/`: 런게임 규칙/스폰(스켈레톤)
