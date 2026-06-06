#pragma once

#include "core/Input.h"
#include "game/Bomb.h"
#include "game/GravityField.h"
#include "game/ItemSprites.h"
#include "game/ObstacleSprites.h"
#include "game/PlayerSprite.h"
#include "game/StageGlacier.h"
#include "game/StageEmerald.h"
#include "game/StageMap.h"
#include "physics/PhysicsWorld.h"
#include "render/UiText.h"
#include "rewind/RewindBuffer.h"

#include <array>
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
  Normal,
  Tall,
  Bounce,
  Spike,
  Triangle,
  Ceiling,
  Moving,
};

struct Obstacle {
  int bodyId = -1;
  ObstacleType type = ObstacleType::Normal;
  float bounceTimer  = 0.0f;
  float moveTimer    = 0.0f;
  float moveRange    = 80.0f;
  float moveOriginX  = 0.0f;
  float ceilingY     = 0.0f;
};

struct Particle {
  Vec2  pos{};
  Vec2  vel{};
  float life    = 0.0f;
  float maxLife = 0.0f;
  float gravity = 400.0f;
  float size    = 5.0f;
  float drag    = 0.0f;
  bool  glow        = false;
  bool  rewindSpark = false;
  Uint8 r = 255, g = 255, b = 255;
};

struct RewindGhost {
  Vec2  pos{};
  float life    = 0.0f;
  float maxLife = 0.0f;
  float age01   = 0.0f;
};

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
  static constexpr float kEmeraldStageStartM = 10000.0f;
  static constexpr float kTotalMapLengthM = kEmeraldStageStartM + kStageLengthM;
  static constexpr float kHpSurvivalDistanceM = kTotalMapLengthM * 1.5f;
  static constexpr float kStageTransitionSeconds = 0.9f;

  Game(int width, int height);
  ~Game();

  void InitRenderer(SDL_Renderer* renderer);
  void HandleInput(float dt, Input& input);
  void FixedUpdate(float dt, const InputState& input, Input& inputDevice);
  void UpdateVisualEffects(float dt);
  void Render(SDL_Renderer* r) const;

  bool WantsQuit() const { return m_quit; }
  bool IsPaused() const { return m_paused; }
  bool IsStarted() const { return m_started; }
  bool IsGameplayActive() const { return m_started && !m_paused && !m_showControls; }

private:
  void SpawnProps();
  void UpdateSpawn();
  void SpawnPattern(float x, int pattern);
  void SpawnFallingObstacle();
  void UpdateFallingObstacles(float dt);
  void SpawnMeteorTrailParticle(Vec2 meteorPos, Vec2 meteorVel, ObstacleStage stage);
  void UpdateObstacles(float dt);
  void SpawnItem(float x, ItemType type);
  void UpdateItems(float dt);
  void SpawnParticles(Vec2 center, int count, Uint8 r, Uint8 g, Uint8 b,
                      float size = 5.0f, bool glow = false);
  void SpawnExplosionVfx(Vec2 center);
  void SpawnBlackHoleOrbitParticle(Vec2 center, float radius, float spinHint);
  void UpdateBlackHoleParticles(float dt);
  void SpawnRewindOrbitParticle(Vec2 playerCenter);
  void UpdateParticles(float dt);
  void AddPopup(const std::string& text, float x, float y, Uint8 r, Uint8 g, Uint8 b);
  void UpdatePopups(float dt);
  void DrawTriangle(SDL_Renderer* r, float cx, float cy, float size, Uint8 rr, Uint8 gg, Uint8 bb) const;
  void DrawHeart(SDL_Renderer* r, float cx, float cy, float size, Uint8 rr, Uint8 gg, Uint8 bb) const;
  void DrawLightning(SDL_Renderer* r, float cx, float cy, float size, Uint8 rr, Uint8 gg, Uint8 bb) const;
  void DrawStar(SDL_Renderer* r, float cx, float cy, float size, Uint8 rr, Uint8 gg, Uint8 bb) const;
  void CheckGameOver();
  void CheckCollision();
  void TriggerGameOver();
  void PreventPlayerObstacleClimb();
  void ClampPlayerToGround();
  void Restart();
  float CameraX() const;
  float SpawnHorizonX() const;
  Vec2 MouseWorldPos(const InputState& input) const;
  void DrawTitleBackground(SDL_Renderer* r) const;
  void DrawTitleSplashPrompt(SDL_Renderer* r) const;
  void DrawControlsOverlay(SDL_Renderer* r) const;
  void DrawShieldAura(SDL_Renderer* r, float screenX, float screenY, float playerRadius) const;
  void EnsureTitleBackground(SDL_Renderer* renderer) const;
  void DrawPauseOverlay(SDL_Renderer* r) const;
  void DrawGameOverOverlay(SDL_Renderer* r) const;
  void DrawStageNotify(SDL_Renderer* r) const;
  void DrawHitEffect(SDL_Renderer* r) const;
  void RecordPlayerPositionHistory(Vec2 pos);
  void ResetPlayerPositionHistory(Vec2 pos);
  void SpawnRewindVfx(Vec2 playerCenter);
  void UpdateRewindVfx(float dt);
  void UpdateRewindVisuals(float dt);
  void DrawRewindGhosts(SDL_Renderer* r, float camX) const;
  void DrawRewindVortex(SDL_Renderer* r, float camX) const;
  void DrawRewindScreenFx(SDL_Renderer* r) const;
  void UpdateStageTransition(float dt);
  enum class VisualStage { Mars = 1, Glacier = 2, Emerald = 3 };
  VisualStage ResolveVisualStage() const;
  ObstacleStage ResolveObstacleStage() const;
  void DrawStageBackground(SDL_Renderer* r, float camX, float groundY, VisualStage stage) const;
  void DrawStageTransitionFade(SDL_Renderer* r) const;
  void EnterClearState();
  void UpdateClearCelebration(float dt);
  void SpawnFireworkBurst(float screenX, float screenY);
  void DrawClearOverlay(SDL_Renderer* r) const;
  void DrawGameplayHud(SDL_Renderer* r) const;
  void ReturnToTitle();
  void LoadLeaderboard();
  void SaveLeaderboard();
  void SubmitScore();
  int DrawLeaderboard(SDL_Renderer* r, int topY) const;

  enum class SlowInputMode { Hold, Toggle };

  int PickSpawnPattern(int maxPattern) const;

  int m_w = 0;
  int m_h = 0;
  bool m_quit    = false;
  bool m_paused  = false;
  bool m_started = false;
  bool m_showControls = false;
  float m_uiBlinkPhase = 0.0f;
  float m_shieldOrbitPhase = 0.0f;

  mutable void* m_titleTexture = nullptr;
  mutable int m_titleW = 0;
  mutable int m_titleH = 0;
  mutable bool m_titleLoadAttempted = false;

  UiText m_ui;
  PhysicsWorld m_world;

  int m_playerId = -1;
  BombSystem m_bombs{16};
  GravityFieldSystem m_fields;
  PlayerSprite m_playerSprite;
  ItemSprites m_itemSprites;
  ObstacleSprites m_obstacleSprites;
  StageMap m_stage;
  StageGlacier m_glacier;
  StageEmerald m_emerald;
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
  int   m_airJumpsLeft  = 0;
  float m_coyote        = 0.0f;
  float m_runAnimPhase  = 0.0f;
  float m_throwReleasePoseLeft = 0.0f;
  float m_rewindPoseLeft       = 0.0f;
  bool  m_rewindQueued         = false;
  float m_rewindCooldownLeft   = 0.0f;
  float m_rewindFreezeLeft     = 0.0f;
  float m_rewindFxTimer        = 0.0f;
  float m_rewindVortexAngle    = 0.0f;
  float m_rewindParticleAcc    = 0.0f;
  float m_blackHoleParticleAcc = 0.0f;
  Vec2  m_rewindVortexCenter{};
  std::array<Vec2, 6> m_playerPosHistory{};
  int m_playerPosHistoryCount  = 0;
  std::vector<RewindGhost>     m_rewindGhosts;

  RewindBuffer  m_rewind;
  GameSnapshot  m_snapshotScratch{};

  InputState m_lastInput{};

  // B 담당
  float m_distance    = 0.0f;
  float m_elapsed     = 0.0f;
  float m_startGraceLeft = 0.0f;
  bool  m_gameOver    = false;
  float m_hp          = 1.0f;
  float m_hitCooldown = 0.0f;
  float m_shieldTimer     = 0.0f;
  float m_blinkTimer  = 0.0f;

  // 슬로우모션
  bool  m_slowActive    = false;
  SlowInputMode m_slowInputMode = SlowInputMode::Hold;
  float m_slowTimer     = 0.0f;
  float m_staminaSlow   = 1.5f;
  float m_staminaRewind = 1.5f;

  // 점수
  int   m_score         = 0;
  int   m_bombKillCount = 0;

  // 폭탄 딜레이
  float m_bombCooldown = 0.0f;

  // 피격 효과
  float m_hitEffectTimer = 0.0f;

  // 스폰 매니저
  float m_nextSpawnX   = 0.0f;
  float m_spawnGap     = 200.0f;
  int   m_patternIndex = 0;
  int   m_lastSpawnPattern = -1;
  float m_nextItemX    = 300.0f;

  // 떨어지는 장애물
  float m_fallingSpawnTimer    = 0.0f;
  float m_fallingSpawnInterval = 8.0f;
  float m_meteorTrailAcc       = 0.0f;

  // 스테이지 알림
  float m_stageNotifyTimer = 0.0f;
  int   m_stageNotifyNum   = 0;
  bool  m_stage2Notified   = false;
  bool  m_stage3Notified   = false;

  // Mars → Glacier → Emerald 전환
  bool  m_glacierTransitionDone  = false;
  bool  m_emeraldTransitionDone  = false;
  int   m_transitionTargetStage  = 2;
  bool  m_stageTransitionPlaying = false;
  float m_stageTransitionT       = 0.0f;

  // 3스테이지(15km) 클리어 연출
  bool  m_cleared           = false;
  float m_fireworkCooldown  = 0.0f;
  float m_clearPulse        = 0.0f;

  // 리더보드
  struct ScoreEntry {
    int score    = 0;
    int distance = 0;
    int bombKills = 0;
  };
  std::vector<ScoreEntry> m_leaderboard;
  bool m_scoreSubmitted = false;
};

} // namespace cr