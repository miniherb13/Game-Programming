#pragma once

#include "core/Input.h"
#include "game/Bomb.h"
#include "game/GravityField.h"
#include "game/PlayerSprite.h"
#include "game/StageGlacier.h"
#include "game/StageMap.h"
#include "physics/PhysicsWorld.h"
#include "render/UiText.h"
#include "rewind/RewindBuffer.h"

#include <vector>

struct SDL_Renderer;

namespace cr {

class Game {
public:
  static constexpr float kStageLengthM = 5000.0f;       // each planet stage (demo)
  static constexpr float kGlacierStageStartM = 5000.0f; // glacier begins after mars stage
  static constexpr float kTotalMapLengthM = kGlacierStageStartM + kStageLengthM;
  static constexpr float kStageTransitionSeconds = 0.9f;

  Game(int width, int height);
  ~Game();

  void InitRenderer(SDL_Renderer* renderer);
  void HandleInput(float dt, Input& input);
  void FixedUpdate(float dt, const InputState& input, Input& inputDevice);
  void Render(SDL_Renderer* r) const;

  bool WantsQuit() const { return m_quit; }
  bool IsPaused() const { return m_paused; }
  bool IsStarted() const { return m_started; }
  bool IsGameplayActive() const { return m_started && !m_paused; }

private:
  void SpawnProps();
  void UpdateSpawn();
  void SpawnPattern(float x, int pattern);  // 패턴별 장애물 배치
  void SpawnFallingObstacle();              // 위에서 떨어지는 장애물
  void UpdateFallingObstacles();            // 떨어지는 장애물 업데이트
  void CheckGameOver();
  void CheckCollision();
  void Restart();
  float CameraX() const;
  Vec2 MouseWorldPos(const InputState& input) const;
  void DrawTitleOverlay(SDL_Renderer* r) const;
  void DrawPauseOverlay(SDL_Renderer* r) const;
  void DrawGameOverOverlay(SDL_Renderer* r) const;
  void UpdateStageTransition(float dt);
  void DrawStageBackground(SDL_Renderer* r, float camX, float groundY, bool useGlacier) const;
  void DrawStageTransitionFade(SDL_Renderer* r) const;

  int m_w = 0;
  int m_h = 0;
  bool m_quit = false;
  bool m_paused = false;
  bool m_started = false;
  float m_uiBlinkPhase = 0.0f;

  UiText m_ui;

  PhysicsWorld m_world;

  int m_playerId = -1;
  BombSystem m_bombs{16};
  GravityFieldSystem m_fields;
  PlayerSprite m_playerSprite;
  StageMap m_stage;       // planet 1 — mars (do not edit for glacier work)
  StageGlacier m_glacier; // planet 2 — glacier
  std::vector<int> m_propIds;     // 땅 장애물
  std::vector<int> m_fallingIds;  // 떨어지는 장애물

  float m_scrollSpeed = 240.0f;
  float m_playerScreenX = 140.0f;
  float m_jumpBuffer = 0.0f;
  float m_jumpGroundGrace = 0.0f;
  float m_coyote = 0.0f;
  float m_runAnimPhase = 0.0f;
  float m_throwReleasePoseLeft = 0.0f;
  float m_rewindPoseLeft = 0.0f;
  bool m_rewindQueued = false;
  float m_rewindCooldownLeft = 0.0f;

  RewindBuffer m_rewind;
  GameSnapshot m_snapshotScratch{};
  float m_stamina = 3.0f;

  InputState m_lastInput{};

  // B 담당 추가 변수
  float m_distance    = 0.0f;
  float m_elapsed     = 0.0f;
  bool  m_gameOver    = false;
  float m_hp          = 1.0f;
  float m_hitCooldown = 0.0f;

  // 스폰 매니저
  float m_nextSpawnX      = 0.0f;
  float m_spawnGap        = 200.0f;  // 초반 간격 넓게
  int   m_patternIndex    = 0;       // 현재 패턴 인덱스

  // 떨어지는 장애물
  float m_fallingSpawnTimer   = 0.0f;
  float m_fallingSpawnInterval = 8.0f;  // 초반엔 8초마다

  // Mars → Glacier white fade (once per run)
  bool  m_glacierTransitionDone = false;
  bool  m_stageTransitionPlaying = false;
  float m_stageTransitionT = 0.0f;
};

} // namespace cr