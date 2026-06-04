#include "game/Game.h"

#include "core/Log.h"
#include "math/Vec2.h"

#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace cr {

namespace {

constexpr float kPlayerDisplayHeight = 82.0f;
constexpr float kRewindPoseSeconds = 1.0f;
constexpr float kRewindStaminaCost = 3.0f;

} // namespace

static SDL_Rect RectFromCircle(const Vec2& p, float r) {
  SDL_Rect out{};
  out.x = static_cast<int>(p.x - r);
  out.y = static_cast<int>(p.y - r);
  out.w = static_cast<int>(r * 2.0f);
  out.h = static_cast<int>(r * 2.0f);
  return out;
}

float Game::CameraX() const {
  const auto& p = m_world.Bodies().at(static_cast<std::size_t>(m_playerId));
  return p.pos.x - m_playerScreenX;
}

Game::Game(int width, int height)
    : m_w(width),
      m_h(height),
      m_world(WorldBounds{0.0f, static_cast<float>(width), static_cast<float>(height - 40), 0.0f}),
      m_rewind(180) {
  m_playerId = m_world.CreateCircle(16.0f, {140.0f, static_cast<float>(height - 80)}, 1.0f, false);
  auto& p = m_world.Get(m_playerId);
  p.restitution = 0.0f;
  p.linearDamping = 0.15f;

  m_bombs.InitPool(m_world);
  m_bombs.SetExplosionHandler([this](Vec2 center) {
    m_fields.SpawnBlackHole(center);
    // 땅 장애물 폭발 제거
    for (int id : m_propIds) {
      auto& crate = m_world.Get(id);
      if (!crate.active) continue;
      const float dx = crate.pos.x - center.x;
      const float dy = crate.pos.y - center.y;
      if (std::sqrt(dx * dx + dy * dy) < 120.0f) crate.active = false;
    }
    // 떨어지는 장애물 폭발 제거
    for (int id : m_fallingIds) {
      auto& crate = m_world.Get(id);
      if (!crate.active) continue;
      const float dx = crate.pos.x - center.x;
      const float dy = crate.pos.y - center.y;
      if (std::sqrt(dx * dx + dy * dy) < 120.0f) crate.active = false;
    }
  });

  SpawnProps();

  if (!m_playerSprite.Load()) {
    Log(LogLevel::Error, "Failed to load assets/player/*.png");
  }

  if (!m_stage.LoadMars()) {
    Log(LogLevel::Warn, "Mars stage assets missing; using fallback ground");
  }
}

Game::~Game() {
  m_ui.Shutdown();
}

void Game::InitRenderer(SDL_Renderer* /*renderer*/) {
  if (!m_ui.Init(19)) {
    Log(LogLevel::Warn, "UI text disabled (font load failed)");
  }
}

void Game::SpawnProps() {
  // 초반 상자 몇 개만 배치
  constexpr int propCount = 5;
  m_propIds.reserve(propCount);

  for (int i = 0; i < propCount; i++) {
    const float x = 400.0f + static_cast<float>(i) * 200.0f;
    SpawnPattern(x, 0); // 처음엔 전부 낮은 상자
  }

  m_nextSpawnX = 400.0f + static_cast<float>(propCount) * 200.0f;
}

// 패턴 종류
// 0: 낮은 상자 1개 (점프)
// 1: 높은 상자 1개 (폭탄)
// 2: 낮은 상자 2개 연속 (점프 타이밍)
// 3: 낮은 + 높은 조합 (점프 + 폭탄)
void Game::SpawnPattern(float x, int pattern) {
  const float groundY = static_cast<float>(m_h - 40);

  if (pattern == 0) {
    // 낮은 상자 1개
    const int id = m_world.CreateCircle(18.0f, {x, groundY - 18.0f}, 1.8f, false);
    auto& crate = m_world.Get(id);
    crate.restitution = 0.25f;
    crate.linearDamping = 1.2f;
    crate.groundFriction = 0.85f;
    m_propIds.push_back(id);

  } else if (pattern == 1) {
    // 높은 상자 1개 (반지름 36, 폭탄으로만 제거)
    const int id = m_world.CreateCircle(36.0f, {x, groundY - 36.0f}, 1.8f, false);
    auto& crate = m_world.Get(id);
    crate.restitution = 0.1f;
    crate.linearDamping = 1.5f;
    crate.groundFriction = 0.9f;
    m_propIds.push_back(id);

  } else if (pattern == 2) {
    // 낮은 상자 2개 연속
    for (int i = 0; i < 2; i++) {
      const int id = m_world.CreateCircle(18.0f, {x + static_cast<float>(i) * 50.0f, groundY - 18.0f}, 1.8f, false);
      auto& crate = m_world.Get(id);
      crate.restitution = 0.25f;
      crate.linearDamping = 1.2f;
      crate.groundFriction = 0.85f;
      m_propIds.push_back(id);
    }

  } else if (pattern == 3) {
    // 낮은 + 높은 조합
    const int id1 = m_world.CreateCircle(18.0f, {x, groundY - 18.0f}, 1.8f, false);
    auto& c1 = m_world.Get(id1);
    c1.restitution = 0.25f;
    c1.linearDamping = 1.2f;
    c1.groundFriction = 0.85f;
    m_propIds.push_back(id1);

    const int id2 = m_world.CreateCircle(36.0f, {x + 120.0f, groundY - 36.0f}, 1.8f, false);
    auto& c2 = m_world.Get(id2);
    c2.restitution = 0.1f;
    c2.linearDamping = 1.5f;
    c2.groundFriction = 0.9f;
    m_propIds.push_back(id2);
  }
}

void Game::SpawnFallingObstacle() {
  const auto& player = m_world.Get(m_playerId);
  // 플레이어 앞 400~600px 랜덤 위치에서 생성
  const float spawnX = player.pos.x + 400.0f + static_cast<float>(std::rand() % 200);
  const int id = m_world.CreateCircle(20.0f, {spawnX, -30.0f}, 1.5f, false);
  auto& crate = m_world.Get(id);
  crate.restitution = 0.3f;
  crate.linearDamping = 0.2f;
  crate.groundFriction = 0.5f;
  m_fallingIds.push_back(id);
}

void Game::UpdateFallingObstacles() {
  const float camLeft = CameraX() - 200.0f;

  for (int id : m_fallingIds) {
    auto& crate = m_world.Get(id);
    if (!crate.active) continue;
    // 카메라 뒤로 벗어나면 비활성화
    if (crate.pos.x < camLeft) {
      crate.active = false;
    }
  }
}

void Game::UpdateSpawn() {
  const float camLeft  = CameraX() - 200.0f;
  const float camRight = CameraX() + static_cast<float>(m_w) + 400.0f;

  // 난이도에 따라 간격 조정 (초반 200 → 최소 80)
  m_spawnGap = std::max(80.0f, 200.0f - m_elapsed * 0.5f);

  // 패턴 선택 (거리에 따라 어려워짐)
  // 0~300m: 패턴 0만
  // 300~600m: 패턴 0,1
  // 600~1000m: 패턴 0,1,2
  // 1000m~: 전체 패턴
  int maxPattern = 0;
  if (m_distance > 300.0f)  maxPattern = 1;
  if (m_distance > 600.0f)  maxPattern = 2;
  if (m_distance > 1000.0f) maxPattern = 3;

  // 카메라 뒤 상자 재활용
  for (int id : m_propIds) {
    auto& crate = m_world.Get(id);
    if (!crate.active) continue;
    if (crate.pos.x < camLeft) {
      crate.active = false;
    }
  }

  // 앞쪽에 새 패턴 생성
  while (m_nextSpawnX < camRight) {
    const int pattern = std::rand() % (maxPattern + 1);
    SpawnPattern(m_nextSpawnX, pattern);

    // 패턴 3은 두 개 상자라서 간격 더 벌림
    if (pattern == 3) {
      m_nextSpawnX += m_spawnGap + 120.0f;
    } else if (pattern == 2) {
      m_nextSpawnX += m_spawnGap + 50.0f;
    } else {
      m_nextSpawnX += m_spawnGap;
    }
  }
}

void Game::CheckCollision() {
  const auto& p = m_world.Get(m_playerId);
  if (m_hitCooldown > 0.0f) return;

  // 땅 장애물 충돌
  for (int id : m_propIds) {
    const auto& crate = m_world.Get(id);
    if (!crate.active) continue;
    const float dx = p.pos.x - crate.pos.x;
    const float dy = p.pos.y - crate.pos.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < p.circle.radius + crate.circle.radius) {
      m_hp = std::max(0.0f, m_hp - 0.25f);
      m_hitCooldown = 0.8f;
      if (m_hp <= 0.0f) m_gameOver = true;
      return;
    }
  }

  // 떨어지는 장애물 충돌
  for (int id : m_fallingIds) {
    const auto& crate = m_world.Get(id);
    if (!crate.active) continue;
    const float dx = p.pos.x - crate.pos.x;
    const float dy = p.pos.y - crate.pos.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < p.circle.radius + crate.circle.radius) {
      m_hp = std::max(0.0f, m_hp - 0.3f);
      m_hitCooldown = 0.8f;
      if (m_hp <= 0.0f) m_gameOver = true;
      return;
    }
  }
}

void Game::CheckGameOver() {
  const auto& p = m_world.Get(m_playerId);
  if (p.pos.y > static_cast<float>(m_h + 100)) {
    m_gameOver = true;
  }
}

void Game::Restart() {
  m_gameOver           = false;
  m_distance           = 0.0f;
  m_elapsed            = 0.0f;
  m_scrollSpeed        = 240.0f;
  m_stamina            = 3.0f;
  m_jumpBuffer         = 0.0f;
  m_coyote             = 0.0f;
  m_rewindCooldownLeft = 0.0f;
  m_rewindQueued       = false;
  m_rewind             = RewindBuffer(180);
  m_nextSpawnX         = 0.0f;
  m_spawnGap           = 200.0f;
  m_hp                 = 1.0f;
  m_hitCooldown        = 0.0f;
  m_patternIndex       = 0;
  m_fallingSpawnTimer  = 0.0f;
  m_fallingSpawnInterval = 8.0f;

  auto& p = m_world.Get(m_playerId);
  p.pos = {140.0f, static_cast<float>(m_h - 80)};
  p.vel = {0.0f, 0.0f};
  p.onGround = false;

  for (int id : m_propIds) {
    auto& crate = m_world.Get(id);
    crate.active = false;
  }
  m_propIds.clear();

  for (int id : m_fallingIds) {
    auto& crate = m_world.Get(id);
    crate.active = false;
  }
  m_fallingIds.clear();

  SpawnProps();
}

Vec2 Game::MouseWorldPos(const InputState& input) const {
  return {input.mousePos.x + CameraX(), input.mousePos.y};
}

void Game::HandleInput(float dt, Input& input) {
  const InputState& in = input.State();
  m_uiBlinkPhase += dt;

  if (in.quit) {
    m_quit = true;
    return;
  }

  if (!m_started) {
    if (in.resumePressed) {
      m_started = true;
    }
    m_lastInput = in;
    return;
  }

  if (in.pausePressed) {
    if (m_paused) {
      m_quit = true;
    } else {
      m_paused = true;
    }
  }

  if (m_paused && in.resumePressed) {
    m_paused = false;
  }

  if (!m_paused) {
    auto& p = m_world.Get(m_playerId);
    if (in.rewindPressed) {
      m_bombs.CancelCharge();
      m_throwReleasePoseLeft = 0.0f;
      m_jumpBuffer = 0.0f;
      if (m_rewindCooldownLeft <= 0.0f && m_stamina >= kRewindStaminaCost) {
        m_rewindQueued = true;
        m_rewindPoseLeft = kRewindPoseSeconds;
      }
      input.ConsumeRewindPending();
    } else {
      if (m_bombs.IsCharging() && in.throwReleased) {
        m_throwReleasePoseLeft = 0.35f;
      }
      m_bombs.UpdateThrow(dt, in, MouseWorldPos(in), m_world, p);
      if (in.throwReleased) {
        input.ConsumeThrowReleasedPending();
      }
    }
    m_throwReleasePoseLeft = std::max(0.0f, m_throwReleasePoseLeft - dt);
    m_rewindPoseLeft = std::max(0.0f, m_rewindPoseLeft - dt);
  }

  m_lastInput = in;
}

void Game::FixedUpdate(float dt, const InputState& input) {
  if (!m_started || m_paused) return;

  if (m_gameOver) {
    if (input.jumpPressed) Restart();
    return;
  }

  m_rewindCooldownLeft = std::max(0.0f, m_rewindCooldownLeft - dt);
  m_hitCooldown = std::max(0.0f, m_hitCooldown - dt);

  const bool rewindFrame = m_rewindQueued;
  if (m_rewindQueued) {
    m_rewindQueued = false;

    if (m_rewindCooldownLeft <= 0.0f && m_stamina >= kRewindStaminaCost &&
        m_rewind.Size() >= m_rewind.Capacity()) {
      bool any = false;
      const std::size_t frames = m_rewind.Capacity();
      for (std::size_t i = 0; i < frames; i++) {
        if (!m_rewind.PopFrame(m_snapshotScratch)) break;
        any = true;
      }

      if (any) {
        const float staminaBeforeRewind = m_stamina;
        m_snapshotScratch.Apply(m_world,
                                m_playerId,
                                m_jumpBuffer,
                                m_coyote,
                                m_stamina,
                                m_bombs,
                                m_fields,
                                m_propIds);
        m_stamina = std::max(0.0f, staminaBeforeRewind - kRewindStaminaCost);
        m_rewindCooldownLeft = kRewindStaminaCost;
        m_rewindPoseLeft = kRewindPoseSeconds;
        m_hp = std::min(1.0f, m_hp + 0.1f);
        return;
      }
    }
  }

  // Jump buffer: only from a press (holding C must not re-trigger every landing frame).
  if (!rewindFrame && input.jumpPressed) {
    m_jumpBuffer = 0.12f;
  }
  m_jumpBuffer = std::max(0.0f, m_jumpBuffer - dt);

  m_snapshotScratch.Capture(m_world,
                            m_playerId,
                            m_jumpBuffer,
                            m_coyote,
                            m_stamina,
                            m_bombs,
                            m_fields,
                            m_propIds);
  m_rewind.PushFrame(m_snapshotScratch);

  auto& p = m_world.Get(m_playerId);

  if (input.debugPressed) m_fields.ToggleDebug();

  m_fields.ApplyForces(m_world, m_playerId, m_bombs.BodyIds(), m_propIds);

  const float laneXBeforeStep = p.pos.x;
  m_world.Step(dt);
  m_fields.FixedUpdate(dt);
  m_bombs.FixedUpdate(dt, m_world, m_playerId);

  // Runner lane: advance X by scroll only (black hole / blast must not drag world X backward).
  p.pos.x = laneXBeforeStep + m_scrollSpeed * dt;
  p.vel.x = m_scrollSpeed;

  if (p.onGround) {
    m_coyote = 0.10f;
    m_runAnimPhase += dt;
  } else {
    m_coyote = std::max(0.0f, m_coyote - dt);
  }

  if (!rewindFrame && m_jumpBuffer > 0.0f && (p.onGround || m_coyote > 0.0f)) {
    p.vel.y = -520.0f;
    p.pos.y -= 1.0f;
    p.onGround = false;
    m_jumpBuffer = 0.0f;
    m_coyote = 0.0f;
  }

  m_distance += m_scrollSpeed * dt;
  m_elapsed  += dt;
  m_scrollSpeed = 240.0f + m_elapsed * 1.5f;

  // 시간에 따라 체력 감소
  m_hp = std::max(0.0f, m_hp - dt * 0.02f);
  if (m_hp <= 0.0f) m_gameOver = true;

  m_stamina = std::min(3.0f, m_stamina + dt * 0.15f);

  // 떨어지는 장애물 스폰 타이머
  // 300m 이후부터 등장, 갈수록 자주
  if (m_distance > 300.0f) {
    m_fallingSpawnInterval = std::max(3.0f, 8.0f - m_elapsed * 0.1f);
    m_fallingSpawnTimer += dt;
    if (m_fallingSpawnTimer >= m_fallingSpawnInterval) {
      m_fallingSpawnTimer = 0.0f;
      SpawnFallingObstacle();
    }
  }

  UpdateSpawn();
  UpdateFallingObstacles();
  CheckCollision();
  CheckGameOver();
}

void Game::DrawTitleOverlay(SDL_Renderer* r) const {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 0, 0, 0, 140);
  SDL_Rect dim{0, 0, m_w, m_h};
  SDL_RenderFillRect(r, &dim);

  const SDL_Rect panel{m_w / 2 - 260, m_h / 2 - 200, 520, 360};
  SDL_SetRenderDrawColor(r, 24, 24, 30, 245);
  SDL_RenderFillRect(r, &panel);
  SDL_SetRenderDrawColor(r, 110, 110, 130, 255);
  SDL_RenderDrawRect(r, &panel);

  const SDL_Color title{255, 255, 255, 255};
  const SDL_Color body{210, 210, 220, 255};
  const SDL_Color accent{120, 220, 255, 255};
  const int line = m_ui.LineHeight();
  int y = panel.y + 24;

  m_ui.DrawCentered(r, m_w / 2, y, "Chrono Rush", title);
  y += line + 4;
  m_ui.DrawCentered(r, m_w / 2, y, "조작법", accent);
  y += line + 10;

  const char* lines[] = {
      "C : 점프",
      "X 홀드 / 떼기 : 폭탄 충전·발사 (폭발=블랙홀)",
      "마우스 : 폭탄 조준 (점선 궤도)",
      "Z : 시간 역행 (3초, 스태미나 3)",
      "Esc : 일시정지",
  };

  for (const char* text : lines) {
    m_ui.Draw(r, panel.x + 28, y, text, body);
    y += line;
  }

  m_ui.DrawCenteredBlink(r,
                         m_w / 2,
                         panel.y + panel.h + 36,
                         "SPACE를 눌러 시작하세요",
                         SDL_Color{255, 255, 255, 255},
                         m_uiBlinkPhase);

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::DrawPauseOverlay(SDL_Renderer* r) const {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 0, 0, 0, 170);
  SDL_Rect dim{0, 0, m_w, m_h};
  SDL_RenderFillRect(r, &dim);

  const SDL_Rect panel{m_w / 2 - 220, m_h / 2 - 120, 440, 200};
  SDL_SetRenderDrawColor(r, 24, 24, 30, 245);
  SDL_RenderFillRect(r, &panel);
  SDL_SetRenderDrawColor(r, 110, 110, 130, 255);
  SDL_RenderDrawRect(r, &panel);

  const SDL_Color title{255, 220, 90, 255};
  const SDL_Color hint{220, 220, 230, 255};
  const int line = m_ui.LineHeight();
  int y = panel.y + 28;

  m_ui.DrawCentered(r, m_w / 2, y, "일시정지", title);
  y += line + 20;
  m_ui.DrawCentered(r, m_w / 2, y, "ESC : 종료", hint);
  y += line;
  m_ui.DrawCentered(r, m_w / 2, y, "SPACE : 재개", hint);

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::DrawGameOverOverlay(SDL_Renderer* r) const {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
  SDL_Rect dim{0, 0, m_w, m_h};
  SDL_RenderFillRect(r, &dim);

  const SDL_Rect panel{m_w / 2 - 220, m_h / 2 - 120, 440, 200};
  SDL_SetRenderDrawColor(r, 24, 24, 30, 245);
  SDL_RenderFillRect(r, &panel);
  SDL_SetRenderDrawColor(r, 180, 60, 60, 255);
  SDL_RenderDrawRect(r, &panel);

  const SDL_Color title{255, 80, 80, 255};
  const SDL_Color hint{220, 220, 230, 255};
  const int line = m_ui.LineHeight();
  int y = panel.y + 28;

  m_ui.DrawCentered(r, m_w / 2, y, "GAME OVER", title);
  y += line + 8;
  const std::string distText = std::to_string(static_cast<int>(m_distance)) + "m";
  m_ui.DrawCentered(r, m_w / 2, y, distText.c_str(), SDL_Color{255, 180, 60, 255});
  y += line + 12;
  m_ui.DrawCentered(r, m_w / 2, y, "C : 재시작", hint);

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::Render(SDL_Renderer* r) const {
  const float camX = CameraX();
  const float groundY = static_cast<float>(m_h - 40);
  const bool gameplayHud = m_started && !m_paused;

  m_stage.EnsureUploaded(r);
  if (m_stage.IsDrawReady()) {
    m_stage.DrawParallaxBackground(r, m_w, m_h, camX, groundY);
    m_stage.DrawTerrain(r, m_w, m_h, camX, groundY);
    m_stage.DrawParallaxNear(r, m_w, m_h, camX, groundY);
    m_stage.DrawStageObjects(r, m_w, m_h, camX, groundY);
  } else {
    SDL_SetRenderDrawColor(r, 20, 20, 24, 255);
    SDL_Rect ground{0, m_h - 40, m_w, 40};
    SDL_RenderFillRect(r, &ground);
  }

  auto drawKey = [&](int x, int y, bool held, bool pressed, SDL_Color base) {
    SDL_Rect bg{x, y, 22, 22};
    SDL_SetRenderDrawColor(r, 18, 18, 22, 255);
    SDL_RenderFillRect(r, &bg);

    SDL_Color c = base;
    if (!held) { c.r = static_cast<Uint8>(c.r / 3); c.g = static_cast<Uint8>(c.g / 3); c.b = static_cast<Uint8>(c.b / 3); }
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    SDL_Rect fg{x + 3, y + 3, 16, 16};
    SDL_RenderFillRect(r, &fg);

    if (pressed) {
      SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
      SDL_RenderDrawRect(r, &bg);
    }
  };

  if (gameplayHud) {
    drawKey(16, 34, m_lastInput.jumpHeld, m_lastInput.jumpPressed, SDL_Color{90, 220, 255, 255});
    drawKey(42, 34, m_lastInput.throwHeld, m_lastInput.throwPressed, SDL_Color{255, 220, 80, 255});
    drawKey(68, 34, m_lastInput.rewindHeld, m_lastInput.rewindPressed, SDL_Color{180, 80, 255, 255});
  }

  {
    const auto& p = m_world.Get(m_playerId);
    const float screenX = p.pos.x - camX;
    const bool rewindPose = m_rewindPoseLeft > 0.0f;
    const float footY =
        rewindPose ? static_cast<float>(m_h) * 0.5f + kPlayerDisplayHeight * 0.5f : p.pos.y + p.circle.radius;
    m_playerSprite.EnsureUploaded(r);
    if (m_playerSprite.IsReady()) {
      m_playerSprite.Draw(r,
                          screenX,
                          footY,
                          p.onGround,
                          m_paused || !m_started,
                          rewindPose,
                          m_bombs.IsCharging(),
                          m_throwReleasePoseLeft > 0.0f,
                          m_bombs.Charge01(),
                          m_runAnimPhase);
    } else {
      SDL_SetRenderDrawColor(r, 255, 80, 80, 255);
      SDL_Rect rc = RectFromCircle({screenX, p.pos.y}, p.circle.radius);
      SDL_RenderFillRect(r, &rc);
    }
  }

  // 땅 장애물
  for (int id : m_propIds) {
    const auto& crate = m_world.Get(id);
    if (!crate.active) continue;
    // 높은 상자는 다른 색으로 표시
    if (crate.circle.radius > 20.0f) {
      SDL_SetRenderDrawColor(r, 180, 60, 60, 255); // 빨간색 = 폭탄으로 부수기
    } else {
      SDL_SetRenderDrawColor(r, 150, 110, 80, 255); // 갈색 = 점프로 넘기
    }
    SDL_Rect rc = RectFromCircle({crate.pos.x - camX, crate.pos.y}, crate.circle.radius);
    SDL_RenderFillRect(r, &rc);
  }

  // 떨어지는 장애물 (보라색)
  for (int id : m_fallingIds) {
    const auto& crate = m_world.Get(id);
    if (!crate.active) continue;
    SDL_SetRenderDrawColor(r, 150, 80, 220, 255);
    SDL_Rect rc = RectFromCircle({crate.pos.x - camX, crate.pos.y}, crate.circle.radius);
    SDL_RenderFillRect(r, &rc);
  }

  m_fields.Render(r, camX);
  const bool showAimGuide =
      m_started && m_rewindPoseLeft <= 0.0f && !m_lastInput.rewindHeld;
  m_bombs.Render(r, camX, groundY, m_world, m_world.Get(m_playerId), MouseWorldPos(m_lastInput), showAimGuide);

  if (gameplayHud) {
    // 스태미나 바
    const int barW = 240;
    const int barH = 12;
    const int x = 16;
    const int y = 16;
    SDL_Rect bg{x, y, barW, barH};
    SDL_SetRenderDrawColor(r, 40, 40, 48, 255);
    SDL_RenderFillRect(r, &bg);

    const int fill = static_cast<int>((m_stamina / 3.0f) * barW);
    SDL_Rect fg{x, y, fill, barH};
    SDL_SetRenderDrawColor(r, 110, 255, 140, 255);
    SDL_RenderFillRect(r, &fg);

    // 체력 바
    SDL_Rect hpBg{x, y + 16, barW, barH};
    SDL_SetRenderDrawColor(r, 40, 40, 48, 255);
    SDL_RenderFillRect(r, &hpBg);
    const int hpFill = static_cast<int>(m_hp * barW);
    SDL_Rect hpFg{x, y + 16, hpFill, barH};
    const Uint8 rr = static_cast<Uint8>(255 * (1.0f - m_hp));
    const Uint8 gg = static_cast<Uint8>(255 * m_hp);
    SDL_SetRenderDrawColor(r, rr, gg, 60, 255);
    SDL_RenderFillRect(r, &hpFg);

    // 거리 바
    SDL_Rect distBg{m_w - 260, 16, 244, 12};
    SDL_SetRenderDrawColor(r, 40, 40, 48, 255);
    SDL_RenderFillRect(r, &distBg);
    const int distFill = static_cast<int>(m_distance / 10.0f) % 244;
    SDL_Rect distFg{m_w - 260, 16, distFill, 12};
    SDL_SetRenderDrawColor(r, 255, 180, 60, 255);
    SDL_RenderFillRect(r, &distFg);

    // 거리 숫자
    const std::string distText = std::to_string(static_cast<int>(m_distance)) + "m";
    m_ui.Draw(r, m_w - 260, 32, distText.c_str(), SDL_Color{255, 180, 60, 255});
  }

  if (!m_started) {
    DrawTitleOverlay(r);
  } else if (m_gameOver) {
    DrawGameOverOverlay(r);
  } else if (m_paused) {
    DrawPauseOverlay(r);
  }
}

} // namespace cr