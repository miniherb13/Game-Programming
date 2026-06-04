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

#include <string>
#include <vector>

struct SDL_Renderer;

namespace cr {

enum class ItemType {
  Health,
  Stamina,
  Shield,
};

struct Item {
  int bodyId = -1;
  ItemType type = ItemType::Health;
  bool collected = false;
};

enum class ObstacleType {
  Normal,       // 갈색 — 점프로 넘기
  Tall,         // 빨간 — 폭탄으로 부수기
  Bounce,       // 주황 — 튀어오르는 장애물
  Spike,        // 하늘색 — 뾰족한 것 (더 많은 피해)
  Triangle,     // 노랑 — 삼각형 가시
  Ceiling,      // 보라 — 천장에서 내려오는 가로막이
  Moving,       // 초록 — 앞뒤로 움직이는 장애물
};

struct Obstacle {
  int bodyId = -1;
  ObstacleType type = ObstacleType::Normal;
  float bounceTimer  = 0.0f;
  float moveTimer    = 0.0f;   // Moving 타입용
  float moveRange    = 80.0f;  // 움직임 범위
  float moveOriginX  = 0.0f;   // 원래 X 위치
  float ceilingY     = 0.0f;   // Ceiling 타입 Y 위치
};

// 파티클
struct Particle {
  Vec2  pos{};
  Vec2  vel{};
  float life    = 0.0f;
  float maxLife = 0.0f;
  Uint8 r = 255, g = 255, b = 255;
};

// 텍스트 팝업
struct PopupText {
  std::string text;
  float x       = 0.0f;
  float y       = 0.0f;
  float life    = 0.0f;
  float maxLife = 0.0f;
  Uint8 r = 255, g = 255, b = 255;
};

class Game {
public:
  static constexpr float kStageLengthM = 5000.0f;
  static constexpr float kGlacierStageStartM = 5000.0f;
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
  void SpawnPattern(float x, int pattern);
  void SpawnFallingObstacle();
  void UpdateFallingObstacles();
  void UpdateObstacles(float dt);
  void SpawnItem(float x, ItemType type);
  void UpdateItems();
  void SpawnParticles(Vec2 center, int count, Uint8 r, Uint8 g, Uint8 b);
  void UpdateParticles(float dt);
  void AddPopup(const std::string& text, float x, float y, Uint8 r, Uint8 g, Uint8 b);
  void UpdatePopups(float dt);
  void DrawTriangle(SDL_Renderer* r, float cx, float cy, float size, Uint8 rr, Uint8 gg, Uint8 bb) const;
  void DrawHeart(SDL_Renderer* r, float cx, float cy, float size, Uint8 rr, Uint8 gg, Uint8 bb) const;
  void DrawLightning(SDL_Renderer* r, float cx, float cy, float size, Uint8 rr, Uint8 gg, Uint8 bb) const;
  void DrawStar(SDL_Renderer* r, float cx, float cy, float size, Uint8 rr, Uint8 gg, Uint8 bb) const;
  void CheckGameOver();
  void CheckCollision();
  void Restart();
  float CameraX() const;
  Vec2 MouseWorldPos(const InputState& input) const;
  void DrawTitleOverlay(SDL_Renderer* r) const;
  void DrawPauseOverlay(SDL_Renderer* r) const;
  void DrawGameOverOverlay(SDL_Renderer* r) const;
  void DrawStageNotify(SDL_Renderer* r) const;
  void DrawHitEffect(SDL_Renderer* r) const;
  void UpdateStageTransition(float dt);
  void DrawStageBackground(SDL_Renderer* r, float camX, float groundY, bool useGlacier) const;
  void DrawStageTransitionFade(SDL_Renderer* r) const;

  int m_w = 0;
  int m_h = 0;
  bool m_quit    = false;
  bool m_paused  = false;
  bool m_started = false;
  float m_uiBlinkPhase = 0.0f;

  UiText m_ui;
  PhysicsWorld m_world;

  int m_playerId = -1;
  BombSystem m_bombs{16};
  GravityFieldSystem m_fields;
  PlayerSprite m_playerSprite;
  StageMap m_stage;
  StageGlacier m_glacier;
  std::vector<int>       m_propIds;
  std::vector<int>       m_fallingIds;
  std::vector<Item>      m_items;
  std::vector<Obstacle>  m_obstacles;
  std::vector<Particle>  m_particles;
  std::vector<PopupText> m_popups;

  float m_scrollSpeed   = 240.0f;
  float m_playerScreenX = 140.0f;
  float m_jumpBuffer    = 0.0f;
  float m_jumpGroundGrace = 0.0f;
  float m_coyote        = 0.0f;
  float m_runAnimPhase  = 0.0f;
  float m_throwReleasePoseLeft = 0.0f;
  float m_rewindPoseLeft       = 0.0f;
  bool  m_rewindQueued         = false;
  float m_rewindCooldownLeft   = 0.0f;

  RewindBuffer  m_rewind;
  GameSnapshot  m_snapshotScratch{};
  float m_stamina = 3.0f;

  InputState m_lastInput{};

  // B 담당
  float m_distance    = 0.0f;
  float m_elapsed     = 0.0f;
  bool  m_gameOver    = false;
  float m_hp          = 1.0f;
  float m_hitCooldown = 0.0f;
  float m_shieldTimer = 0.0f;
  float m_blinkTimer  = 0.0f;

  // 점수
  int   m_score         = 0;
  int   m_bombKillCount = 0;

  // 피격 효과
  float m_hitEffectTimer = 0.0f;

  // 스폰 매니저
  float m_nextSpawnX   = 0.0f;
  float m_spawnGap     = 200.0f;
  int   m_patternIndex = 0;
  float m_nextItemX    = 300.0f;

  // 떨어지는 장애물
  float m_fallingSpawnTimer    = 0.0f;
  float m_fallingSpawnInterval = 8.0f;

  // 스테이지 알림
  float m_stageNotifyTimer = 0.0f;
  int   m_stageNotifyNum   = 0;
  bool  m_stage2Notified   = false;

  // Mars → Glacier 전환
  bool  m_glacierTransitionDone  = false;
  bool  m_stageTransitionPlaying = false;
  float m_stageTransitionT       = 0.0f;
};

} // namespace cr