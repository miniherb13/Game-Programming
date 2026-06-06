#include "game/Game.h"

#include "core/Log.h"
#include "math/Vec2.h"
#include "render/VfxLibrary.h"

#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace cr {

namespace {

constexpr float kPlayerDisplayHeight = 82.0f;
constexpr float kRewindPoseSeconds   = 1.0f;
constexpr float kRewindStaminaCost   = 1.5f;
constexpr float kRewindFxSeconds     = 1.4f; // 0.7s fade-in + 0.7s fade-out
constexpr int   kRewindConvergeParticles = 340;
constexpr int   kRewindSparkParticles    = 220;
constexpr int   kRewindRingParticles     = 200;
constexpr int   kMaxParticles            = 6500;
constexpr float kStageNotifyDuration = 3.0f;
constexpr float kHitInvincibleTime   = 0.5f;
constexpr float kRewindInvincibleTime = 2.0f;
constexpr float kRewindFreezeSeconds  = 0.5f;
constexpr float kRewindSpinRadPerSec  = 5.5f;
constexpr float kStartGraceSeconds    = 1.0f;
constexpr float kItemMagnetRadius     = 110.0f;
constexpr float kItemMagnetPullSpeed  = 95.0f;

// In-game HUD layout (1280x720 baseline; avoids bar/key/score overlap).
constexpr int kHudMargin   = 12;
constexpr int kHudBarW     = 310;
constexpr int kHudBarH     = 10;
constexpr int kHudHpBarH   = 14;
constexpr int kHudHpBadgeR = 11;
constexpr int kHudBarGap   = 4;
constexpr int kHudDistBarW = 210;
constexpr int kHudEnergyH  = 8;
constexpr float kClearFireworkInterval = 0.35f;

constexpr float kItemDisplaySize = 36.0f * 1.25f;

// Clock 1 o'clock → 7 o'clock: ~45° descent (shallow diagonal, down-left).
Vec2 SampleMeteorFallVelocity() {
  constexpr float kBaseDeg   = 135.0f;
  constexpr float kSpreadDeg = 10.0f;
  const float t = static_cast<float>(std::rand() % 1000) / 999.0f;
  const float deg = kBaseDeg + (t * 2.0f - 1.0f) * kSpreadDeg;
  const float rad = deg * 3.14159265f / 180.0f;
  const float speed = 203.0f + static_cast<float>(std::rand() % 53);
  return {std::cos(rad) * speed, std::sin(rad) * speed};
}

ItemSpriteId ItemSpriteFor(ItemType type) {
  switch (type) {
  case ItemType::Health: return ItemSpriteId::Health;
  case ItemType::Stamina: return ItemSpriteId::Stamina;
  case ItemType::Shield: return ItemSpriteId::Shield;
  }
  return ItemSpriteId::Health;
}

ObstacleSpriteId ObstacleSpriteFor(ObstacleType type) {
  switch (type) {
  case ObstacleType::Normal: return ObstacleSpriteId::Normal;
  case ObstacleType::Tall: return ObstacleSpriteId::Tall;
  case ObstacleType::Bounce: return ObstacleSpriteId::Bounce;
  case ObstacleType::Spike: return ObstacleSpriteId::Spike;
  case ObstacleType::Triangle: return ObstacleSpriteId::Triangle;
  case ObstacleType::Ceiling: return ObstacleSpriteId::Ceiling;
  case ObstacleType::Moving: return ObstacleSpriteId::Moving;
  }
  return ObstacleSpriteId::Normal;
}

float ObstacleSpriteHeight(ObstacleType type, float radius) {
  switch (type) {
  case ObstacleType::Tall: return radius * 2.35f;
  case ObstacleType::Ceiling: return radius * 2.0f;
  default: return radius * 2.2f;
  }
}

void DrawWhiteRectOutline(SDL_Renderer* r, SDL_Rect rc) {
  SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
  SDL_RenderDrawRect(r, &rc);
  rc.x -= 1;
  rc.y -= 1;
  rc.w += 2;
  rc.h += 2;
  SDL_RenderDrawRect(r, &rc);
}

void FillGradientBarH(SDL_Renderer* r,
                      const SDL_Rect& rect,
                      float fill01,
                      Uint8 r0,
                      Uint8 g0,
                      Uint8 b0,
                      Uint8 r1,
                      Uint8 g1,
                      Uint8 b1) {
  const int fillW = static_cast<int>(static_cast<float>(rect.w) * std::clamp(fill01, 0.0f, 1.0f));
  if (fillW <= 0) return;
  for (int x = 0; x < fillW; ++x) {
    const float u = fillW <= 1 ? 1.0f : static_cast<float>(x) / static_cast<float>(fillW - 1);
    SDL_SetRenderDrawColor(r,
                           static_cast<Uint8>(r0 + static_cast<int>((r1 - r0) * u)),
                           static_cast<Uint8>(g0 + static_cast<int>((g1 - g0) * u)),
                           static_cast<Uint8>(b0 + static_cast<int>((b1 - b0) * u)),
                           255);
    SDL_RenderDrawLine(r, rect.x + x, rect.y, rect.x + x, rect.y + rect.h - 1);
  }
}

void DrawHudBarShell(SDL_Renderer* r, const SDL_Rect& rect) {
  SDL_SetRenderDrawColor(r, 16, 18, 28, 230);
  SDL_RenderFillRect(r, &rect);
  SDL_SetRenderDrawColor(r, 70, 78, 110, 255);
  SDL_RenderDrawRect(r, &rect);
}

bool IsInsideCapsule(int x, int y, const SDL_Rect& bar) {
  if (y < bar.y || y >= bar.y + bar.h) return false;
  if (x < bar.x || x >= bar.x + bar.w) return false;

  const int r = bar.h / 2;
  const int cy = bar.y + r;
  if (x < bar.x + r) {
    const int dx = x - (bar.x + r);
    const int dy = y - cy;
    return dx * dx + dy * dy <= r * r;
  }
  if (x >= bar.x + bar.w - r) {
    const int dx = x - (bar.x + bar.w - r);
    const int dy = y - cy;
    return dx * dx + dy * dy <= r * r;
  }
  return true;
}

void FillDisc(SDL_Renderer* r, int cx, int cy, int radius, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 alpha) {
  SDL_SetRenderDrawColor(r, cr, cg, cb, alpha);
  for (int y = -radius; y <= radius; ++y) {
    const int halfW = static_cast<int>(std::sqrt(std::max(0, radius * radius - y * y)));
    SDL_RenderDrawLine(r, cx - halfW, cy + y, cx + halfW, cy + y);
  }
}

void FillCapsule(SDL_Renderer* renderer, const SDL_Rect& bar, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 alpha) {
  if (bar.w <= 0 || bar.h <= 0) return;
  SDL_SetRenderDrawColor(renderer, cr, cg, cb, alpha);
  const int capR = bar.h / 2;
  if (bar.w >= bar.h) {
    SDL_Rect mid{bar.x + capR, bar.y, bar.w - bar.h, bar.h};
    SDL_RenderFillRect(renderer, &mid);
  }
  FillDisc(renderer, bar.x + capR, bar.y + capR, capR, cr, cg, cb, alpha);
  FillDisc(renderer, bar.x + bar.w - capR, bar.y + capR, capR, cr, cg, cb, alpha);
}

void DrawCapsuleOutline(SDL_Renderer* renderer, const SDL_Rect& bar, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 alpha) {
  if (bar.w <= 0 || bar.h <= 0) return;
  SDL_SetRenderDrawColor(renderer, cr, cg, cb, alpha);
  const int capR = bar.h / 2;
  const int cy = bar.y + capR;

  if (bar.w >= bar.h) {
    SDL_RenderDrawLine(renderer, bar.x + capR, bar.y, bar.x + bar.w - capR, bar.y);
    SDL_RenderDrawLine(renderer, bar.x + capR, bar.y + bar.h - 1, bar.x + bar.w - capR, bar.y + bar.h - 1);
  }

  constexpr int kSeg = 24;
  for (int cap = 0; cap < 2; ++cap) {
    const int ccx = cap == 0 ? bar.x + capR : bar.x + bar.w - capR;
    int px = ccx + capR;
    int py = cy;
    for (int i = 1; i <= kSeg; ++i) {
      const float ang = static_cast<float>(i) / static_cast<float>(kSeg) * 3.14159265f;
      const int nx = ccx + static_cast<int>(std::cos(ang) * static_cast<float>(capR));
      const int ny = cy + static_cast<int>(std::sin(ang) * static_cast<float>(capR) * (cap == 0 ? -1.0f : 1.0f));
      SDL_RenderDrawLine(renderer, px, py, nx, ny);
      px = nx;
      py = ny;
    }
  }
}

void FillCookieStripedCapsule(SDL_Renderer* r,
                              const SDL_Rect& bar,
                              float fill01,
                              Uint8 stripe0R,
                              Uint8 stripe0G,
                              Uint8 stripe0B,
                              Uint8 stripe1R,
                              Uint8 stripe1G,
                              Uint8 stripe1B) {
  const float clamped = std::clamp(fill01, 0.0f, 1.0f);
  const int fillW = std::max(1, static_cast<int>(static_cast<float>(bar.w) * clamped));

  for (int y = bar.y; y < bar.y + bar.h; ++y) {
    for (int x = bar.x; x < bar.x + fillW; ++x) {
      if (!IsInsideCapsule(x, y, bar)) continue;
      const bool stripeA = ((x - y) / 3) % 2 == 0;
      if (stripeA) {
        SDL_SetRenderDrawColor(r, stripe0R, stripe0G, stripe0B, 255);
      } else {
        SDL_SetRenderDrawColor(r, stripe1R, stripe1G, stripe1B, 255);
      }
      SDL_RenderDrawPoint(r, x, y);
    }
  }

  if (fillW > 2) {
    const int edgeX = bar.x + fillW - 1;
    for (int y = bar.y + 1; y < bar.y + bar.h - 1; ++y) {
      if (!IsInsideCapsule(edgeX, y, bar)) continue;
      SDL_SetRenderDrawColor(r, 255, 255, 255, 230);
      SDL_RenderDrawPoint(r, edgeX, y);
      if (IsInsideCapsule(edgeX - 1, y, bar)) {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 110);
        SDL_RenderDrawPoint(r, edgeX - 1, y);
      }
    }
  }
}

void DrawCookieRunBarShell(SDL_Renderer* r,
                           const SDL_Rect& bar,
                           Uint8 trackR,
                           Uint8 trackG,
                           Uint8 trackB,
                           Uint8 outlineR,
                           Uint8 outlineG,
                           Uint8 outlineB,
                           Uint8 highlightR,
                           Uint8 highlightG,
                           Uint8 highlightB) {
  FillCapsule(r, bar, trackR, trackG, trackB, 235);
  DrawCapsuleOutline(r, bar, outlineR, outlineG, outlineB, 255);
  if (bar.h >= 4) {
    SDL_SetRenderDrawColor(r, highlightR, highlightG, highlightB, 90);
    SDL_RenderDrawLine(r, bar.x + bar.h / 2, bar.y + 1, bar.x + bar.w - bar.h / 2, bar.y + 1);
  }
}

void DrawCookieRunHpBarShell(SDL_Renderer* r, const SDL_Rect& bar) {
  DrawCookieRunBarShell(r, bar, 28, 16, 22, 12, 8, 10, 55, 42, 38);
}

void DrawMiniHeart(SDL_Renderer* r, float cx, float cy, float size, Uint8 rr, Uint8 gg, Uint8 bb) {
  SDL_SetRenderDrawColor(r, rr, gg, bb, 255);
  for (int row = 0; row < static_cast<int>(size * 2); row++) {
    const float t = static_cast<float>(row) / (size * 2.0f);
    const float y = cy - size + static_cast<float>(row);
    float halfW = 0.0f;
    if (t < 0.5f) {
      const float tt = t * 2.0f;
      halfW = size * (0.5f + 0.5f * std::sqrt(std::max(0.0f, 1.0f - (tt - 0.5f) * (tt - 0.5f) * 4.0f)));
    } else {
      const float tt = (t - 0.5f) * 2.0f;
      halfW = size * (1.0f - tt);
    }
    SDL_RenderDrawLine(r,
                       static_cast<int>(cx - halfW),
                       static_cast<int>(y),
                       static_cast<int>(cx + halfW),
                       static_cast<int>(y));
  }
}

void DrawHpHeartBadge(SDL_Renderer* r, float cx, float cy, const ItemSprites* items) {
  FillDisc(r, static_cast<int>(cx), static_cast<int>(cy), kHudHpBadgeR + 2, 210, 150, 35, 255);
  FillDisc(r, static_cast<int>(cx), static_cast<int>(cy), kHudHpBadgeR, 52, 32, 22, 255);

  if (items && items->IsReady()) {
    items->Draw(r, ItemSpriteId::Health, cx, cy + 1.0f, 16.0f);
  } else {
    DrawMiniHeart(r, cx, cy + 1.0f, 6.0f, 255, 70, 90);
  }
}

void DrawStaminaBadge(SDL_Renderer* r, float cx, float cy, const ItemSprites* items) {
  FillDisc(r, static_cast<int>(cx), static_cast<int>(cy), kHudHpBadgeR + 2, 170, 210, 55, 255);
  FillDisc(r, static_cast<int>(cx), static_cast<int>(cy), kHudHpBadgeR, 24, 48, 28, 255);

  if (items && items->IsReady()) {
    items->Draw(r, ItemSpriteId::Stamina, cx, cy + 1.0f, 16.0f);
  } else {
    SDL_SetRenderDrawColor(r, 120, 230, 90, 255);
    const int ix = static_cast<int>(cx);
    const int iy = static_cast<int>(cy);
    SDL_RenderDrawLine(r, ix + 4, iy - 6, ix - 2, iy + 2);
    SDL_RenderDrawLine(r, ix - 2, iy + 2, ix + 6, iy + 6);
  }
}

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

float Game::SpawnHorizonX() const {
  return CameraX() + static_cast<float>(m_w) + 400.0f;
}

Game::Game(int width, int height)
    : m_w(width),
      m_h(height),
      m_world(WorldBounds{0.0f, static_cast<float>(width), static_cast<float>(height - 40), 0.0f}),
      m_rewind(180) {
  m_playerId = m_world.CreateCircle(16.0f, {140.0f, static_cast<float>(height - 80)}, 1.0f,
                                     MotionType::Kinematic);
  auto& p = m_world.Get(m_playerId);
  p.kind          = BodyKind::Player;
  p.restitution   = 0.0f;
  p.linearDamping = 0.15f;
  p.lockVelX      = true;

  m_bombs.InitPool(m_world);
  m_bombs.SetExplosionHandler([this](Vec2 center, int hitObstacleId) {
    SpawnExplosionVfx(center);
    m_fields.SpawnBlackHole(center);
    constexpr float kBlastRadius = BombTuning::explosionRadius;
    constexpr float kSpritePadding = 14.0f;

    for (auto& obs : m_obstacles) {
      auto& body = m_world.Get(obs.bodyId);
      if (!body.active) continue;
      if (obs.type == ObstacleType::Ceiling) continue;

      const bool directHit = obs.bodyId == hitObstacleId;
      const float dx   = body.pos.x - center.x;
      const float dy   = body.pos.y - center.y;
      const float dist = std::sqrt(dx * dx + dy * dy);
      const float destroyRadius = kBlastRadius + body.circle.radius + kSpritePadding;
      if (!directHit && dist > destroyRadius) continue;

      SpawnParticles(body.pos, 12, 255, 160, 40);
      body.active = false;
      body.vel = {};
      m_score += 50;
      m_bombKillCount++;
    }
    for (int id : m_fallingIds) {
      auto& crate = m_world.Get(id);
      if (!crate.active) continue;
      const float dx = crate.pos.x - center.x;
      const float dy = crate.pos.y - center.y;
      const float dist = std::sqrt(dx * dx + dy * dy);
      if (dist <= kBlastRadius + crate.circle.radius + kSpritePadding) {
        SpawnParticles(crate.pos, 8, 150, 80, 220);
        crate.active = false;
        m_score += 30;
      }
    }
  });

  m_nextSpawnX = m_playerScreenX + static_cast<float>(width) + 400.0f;
  m_nextItemX  = m_nextSpawnX;

  if (!m_playerSprite.Load())  Log(LogLevel::Error, "Failed to load assets/player/*.png");
  if (!m_itemSprites.Load())   Log(LogLevel::Warn,  "Item icons missing (assets/items/*.png)");
  if (!m_obstacleSprites.Load()) Log(LogLevel::Warn, "Obstacle sprites missing (assets/stages/*/obstacles/*.png)");
  if (!m_stage.LoadMars())     Log(LogLevel::Warn,  "Mars stage assets missing");
  if (!m_glacier.Load())       Log(LogLevel::Warn,  "Glacier stage assets missing");
  if (!m_emerald.Load())       Log(LogLevel::Warn,  "Emerald stage assets missing");
}

Game::~Game() {
  m_ui.Shutdown();
  VfxLibrary::Instance().Shutdown();
}

void Game::InitRenderer(SDL_Renderer* renderer) {
  if (!VfxLibrary::Instance().Init(renderer)) Log(LogLevel::Warn, "VFX glow textures unavailable");
  if (!m_ui.Init(19)) Log(LogLevel::Warn, "UI text disabled (font load failed)");
}

void Game::SpawnProps() {
  constexpr int propCount = 5;
  m_propIds.reserve(propCount);
  for (int i = 0; i < propCount; i++) {
    SpawnPattern(400.0f + static_cast<float>(i) * 200.0f, 0);
  }
  m_nextSpawnX = 400.0f + static_cast<float>(propCount) * 200.0f;
}

void Game::SpawnPattern(float x, int pattern) {
  const float groundY = static_cast<float>(m_h - 40);

  auto addObs = [&](int id, ObstacleType type) {
    auto& b = m_world.Get(id);
    b.motion = MotionType::Dynamic;
    Obstacle obs;
    obs.bodyId      = id;
    obs.type        = type;
    obs.moveOriginX = x;
    m_obstacles.push_back(obs);
    m_propIds.push_back(id);
  };

  if (pattern == 0) {
    // 낮은 상자 (점프)
    const int id = m_world.CreateCircle(18.0f, {x, groundY - 18.0f}, 1.8f, false);
    auto& b = m_world.Get(id);
    b.kind = BodyKind::Obstacle;
    b.restitution = 0.25f; b.linearDamping = 1.2f; b.groundFriction = 0.85f;
    addObs(id, ObstacleType::Normal);

  } else if (pattern == 1) {
    // 높은 상자 (폭탄)
    const int id = m_world.CreateCircle(36.0f, {x, groundY - 36.0f}, 1.8f, false);
    auto& b = m_world.Get(id);
    b.kind = BodyKind::Obstacle;
    b.restitution = 0.1f; b.linearDamping = 1.5f; b.groundFriction = 0.9f;
    addObs(id, ObstacleType::Tall);

  } else if (pattern == 2) {
    // 낮은 상자 2개 연속
    for (int i = 0; i < 2; i++) {
      const int id = m_world.CreateCircle(18.0f,
          {x + static_cast<float>(i) * 50.0f, groundY - 18.0f}, 1.8f, false);
      auto& b = m_world.Get(id);
      b.kind = BodyKind::Obstacle;
      b.restitution = 0.25f; b.linearDamping = 1.2f; b.groundFriction = 0.85f;
      addObs(id, ObstacleType::Normal);
    }

  } else if (pattern == 3) {
    // 낮은 + 높은 조합
    const int id1 = m_world.CreateCircle(18.0f, {x, groundY - 18.0f}, 1.8f, false);
    auto& c1 = m_world.Get(id1);
    c1.kind = BodyKind::Obstacle;
    c1.restitution = 0.25f; c1.linearDamping = 1.2f; c1.groundFriction = 0.85f;
    addObs(id1, ObstacleType::Normal);

    const int id2 = m_world.CreateCircle(36.0f, {x + 120.0f, groundY - 36.0f}, 1.8f, false);
    auto& c2 = m_world.Get(id2);
    c2.kind = BodyKind::Obstacle;
    c2.restitution = 0.1f; c2.linearDamping = 1.5f; c2.groundFriction = 0.9f;
    addObs(id2, ObstacleType::Tall);

  } else if (pattern == 4) {
    // 튀어오르는 장애물
    const int id = m_world.CreateCircle(20.0f, {x, groundY - 20.0f}, 1.5f, false);
    auto& b = m_world.Get(id);
    b.kind = BodyKind::Obstacle;
    b.restitution = 0.9f; b.linearDamping = 0.3f; b.groundFriction = 0.1f;
    b.vel.y = -300.0f;
    addObs(id, ObstacleType::Bounce);

  } else if (pattern == 5) {
    // 삼각형 가시 (Triangle) — 중반부터
    const int id = m_world.CreateCircle(22.0f, {x, groundY - 22.0f}, 1.8f, false);
    auto& b = m_world.Get(id);
    b.kind = BodyKind::Obstacle;
    b.restitution = 0.1f; b.linearDamping = 2.0f; b.groundFriction = 0.95f;
    addObs(id, ObstacleType::Triangle);

  } else if (pattern == 6) {
    // 천장 가로막이 (Ceiling) — 점프하면 맞음, 숙여야 함
    const int id = m_world.CreateCircle(20.0f, {x, 80.0f}, 1.8f, true); // 천장에 고정
    auto& b = m_world.Get(id);
    b.kind = BodyKind::Obstacle;
    b.restitution = 0.0f; b.linearDamping = 0.0f;
    Obstacle obs;
    obs.bodyId    = id;
    obs.type      = ObstacleType::Ceiling;
    obs.ceilingY  = 80.0f;
    obs.moveOriginX = x;
    m_obstacles.push_back(obs);
    m_propIds.push_back(id);

  } else if (pattern == 7) {
    // 앞뒤로 움직이는 장애물 (Moving) — 후반부터
    const int id = m_world.CreateCircle(20.0f, {x, groundY - 20.0f}, 1.8f, false);
    auto& b = m_world.Get(id);
    b.kind          = BodyKind::Obstacle;
    b.motion         = MotionType::Kinematic;
    b.restitution    = 0.2f;
    b.linearDamping  = 0.0f;
    b.groundFriction = 0.0f;
    Obstacle obs;
    obs.bodyId      = id;
    obs.type        = ObstacleType::Moving;
    obs.moveOriginX = x;
    obs.moveRange   = 100.0f;
    m_obstacles.push_back(obs);
    m_propIds.push_back(id);
  }
}

void Game::SpawnFallingObstacle() {
  const auto& player = m_world.Get(m_playerId);
  const float landingAhead = 340.0f + static_cast<float>(std::rand() % 180);
  const float spawnX = player.pos.x + landingAhead + 240.0f + static_cast<float>(std::rand() % 120);
  const float spawnY = -80.0f - static_cast<float>(std::rand() % 60);
  const int id = m_world.CreateCircle(20.0f, {spawnX, spawnY}, 1.5f, false);
  auto& crate = m_world.Get(id);
  crate.kind = BodyKind::Obstacle;
  crate.restitution = 0.3f;
  crate.linearDamping = 0.06f;
  crate.groundFriction = 0.5f;
  crate.vel = SampleMeteorFallVelocity();
  m_fallingIds.push_back(id);
}

void Game::SpawnMeteorTrailParticle(Vec2 meteorPos, Vec2 meteorVel, ObstacleStage stage) {
  Vec2 dir = Normalize(meteorVel);
  if (dir.LenSq() < 1e-4f) dir = {-0.707f, 0.707f};

  const Vec2 tailDir = dir * -1.0f;
  const Vec2 tangent{-dir.y, dir.x};
  const float behind = 18.0f + static_cast<float>(std::rand() % 16);
  const float side = static_cast<float>(std::rand() % 24) - 12.0f;

  Particle pt;
  pt.pos = meteorPos + tailDir * behind + tangent * side;

  const float backSpeed = 35.0f + static_cast<float>(std::rand() % 70);
  const float spread = static_cast<float>(std::rand() % 50) - 25.0f;
  pt.vel = tailDir * backSpeed + tangent * spread + meteorVel * 0.12f;
  pt.drag = 2.0f;
  pt.maxLife = 0.24f + static_cast<float>(std::rand() % 20) / 100.0f;
  pt.life = pt.maxLife;
  pt.size = 2.0f + static_cast<float>(std::rand() % 4);
  pt.glow = true;

  if (stage == ObstacleStage::Emerald) {
    pt.gravity = 50.0f;
    const int tone = std::rand() % 100;
    if (tone < 45) {
      pt.r = static_cast<Uint8>(60 + std::rand() % 40);
      pt.g = static_cast<Uint8>(190 + std::rand() % 50);
      pt.b = static_cast<Uint8>(40 + std::rand() % 35);
    } else if (tone < 80) {
      pt.r = static_cast<Uint8>(90 + std::rand() % 45);
      pt.g = static_cast<Uint8>(220 + std::rand() % 35);
      pt.b = static_cast<Uint8>(70 + std::rand() % 40);
    } else {
      pt.r = static_cast<Uint8>(140 + std::rand() % 50);
      pt.g = static_cast<Uint8>(240 + std::rand() % 15);
      pt.b = static_cast<Uint8>(90 + std::rand() % 40);
    }
  } else if (stage == ObstacleStage::Glacier) {
    pt.gravity = 60.0f;
    const int tone = std::rand() % 100;
    if (tone < 45) {
      pt.r = static_cast<Uint8>(90 + std::rand() % 40);
      pt.g = static_cast<Uint8>(195 + std::rand() % 45);
      pt.b = 255;
    } else if (tone < 80) {
      pt.r = static_cast<Uint8>(160 + std::rand() % 50);
      pt.g = static_cast<Uint8>(225 + std::rand() % 30);
      pt.b = 255;
    } else {
      pt.r = static_cast<Uint8>(210 + std::rand() % 35);
      pt.g = static_cast<Uint8>(240 + std::rand() % 15);
      pt.b = 255;
    }
  } else {
    pt.gravity = 100.0f;
    const int tone = std::rand() % 100;
    if (tone < 50) {
      pt.r = static_cast<Uint8>(210 + std::rand() % 45);
      pt.g = static_cast<Uint8>(20 + std::rand() % 35);
      pt.b = static_cast<Uint8>(10 + std::rand() % 20);
    } else if (tone < 85) {
      pt.r = static_cast<Uint8>(170 + std::rand() % 60);
      pt.g = static_cast<Uint8>(10 + std::rand() % 25);
      pt.b = static_cast<Uint8>(8 + std::rand() % 18);
    } else {
      pt.r = static_cast<Uint8>(255);
      pt.g = static_cast<Uint8>(35 + std::rand() % 40);
      pt.b = static_cast<Uint8>(15 + std::rand() % 25);
    }
  }

  m_particles.push_back(pt);
}

void Game::UpdateFallingObstacles(float dt) {
  const float camLeft = CameraX() - 200.0f;
  const ObstacleStage trailStage = ResolveObstacleStage();

  m_meteorTrailAcc += dt;
  constexpr float kTrailInterval = 0.03f;
  const bool spawnTrail = m_meteorTrailAcc >= kTrailInterval;
  if (spawnTrail) m_meteorTrailAcc -= kTrailInterval;

  for (int id : m_fallingIds) {
    auto& crate = m_world.Get(id);
    if (!crate.active) continue;
    if (crate.pos.x < camLeft) {
      crate.active = false;
      continue;
    }

    if (spawnTrail && crate.vel.LenSq() > 64.0f) {
      SpawnMeteorTrailParticle(crate.pos, crate.vel, trailStage);
      if ((std::rand() % 100) < 35) {
        SpawnMeteorTrailParticle(crate.pos, crate.vel, trailStage);
      }
    }
  }
}

void Game::UpdateObstacles(float dt) {
  const float camLeft = CameraX() - 200.0f;
  for (auto& obs : m_obstacles) {
    auto& body = m_world.Get(obs.bodyId);
    if (!body.active) continue;
    if (body.pos.x < camLeft) { body.active = false; continue; }

    if (obs.bounceTimer < 0.0f) {
      obs.bounceTimer -= dt;
      if (obs.bounceTimer < -1.5f) {
        SpawnParticles(body.pos, 6, 255, 160, 40);
        body.active = false;
      }
      continue;
    }

    if (obs.type == ObstacleType::Bounce) {
      obs.bounceTimer += dt;
      if (obs.bounceTimer > 1.2f && body.onGround) {
        body.vel.y = -320.0f;
        obs.bounceTimer = 0.0f;
      }
    } else if (obs.type == ObstacleType::Moving) {
      obs.moveTimer += dt;
      body.pos.x = obs.moveOriginX + std::sin(obs.moveTimer * 2.0f) * obs.moveRange;
      body.vel.x = 0.0f;
    }
  }
}

void Game::SpawnParticles(Vec2 center, int count, Uint8 r, Uint8 g, Uint8 b, float size, bool glow) {
  for (int i = 0; i < count; i++) {
    Particle pt;
    pt.pos = center;
    const float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
    const float speed = 80.0f + static_cast<float>(std::rand() % 160);
    pt.vel    = {std::cos(angle) * speed, std::sin(angle) * speed - 100.0f};
    pt.maxLife = 0.4f + static_cast<float>(std::rand() % 40) / 100.0f;
    pt.life   = pt.maxLife;
    pt.size   = size + static_cast<float>(std::rand() % 4);
    pt.glow   = glow;
    pt.r = r; pt.g = g; pt.b = b;
    m_particles.push_back(pt);
  }
}

void Game::SpawnExplosionVfx(Vec2 center) {
  // Outward debris burst.
  for (int i = 0; i < 95; ++i) {
    Particle pt;
    pt.pos = center;
    const float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
    const float speed = 140.0f + static_cast<float>(std::rand() % 320);
    pt.vel = {std::cos(angle) * speed, std::sin(angle) * speed - 60.0f};
    pt.maxLife = 0.35f + static_cast<float>(std::rand() % 45) / 100.0f;
    pt.life = pt.maxLife;
    pt.size = 3.0f + static_cast<float>(std::rand() % 7);
    pt.drag = 2.5f;
    pt.glow = (std::rand() % 100) < 65;
    pt.r = static_cast<Uint8>(120 + std::rand() % 70);
    pt.g = static_cast<Uint8>(35 + std::rand() % 55);
    pt.b = static_cast<Uint8>(130 + std::rand() % 80);
    m_particles.push_back(pt);
  }

  // Inward spiraling embers sucked toward the singularity.
  for (int i = 0; i < 110; ++i) {
    Particle pt;
    const float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
    const float dist = 28.0f + static_cast<float>(std::rand() % 220);
    pt.pos = {center.x + std::cos(angle) * dist, center.y + std::sin(angle) * dist};

    Vec2 toCenter = center - pt.pos;
    const float len = toCenter.Len();
    if (len > 1e-4f) toCenter = toCenter / len;
    Vec2 tangent{-toCenter.y, toCenter.x};
    const float spinDir = (std::rand() % 2 == 0) ? 1.0f : -1.0f;
    const float speed = 180.0f + static_cast<float>(std::rand() % 220);
    pt.vel = toCenter * speed + tangent * (spinDir * (130.0f + static_cast<float>(std::rand() % 120)));
    pt.gravity = 0.0f;
    pt.drag = 1.2f;
    pt.maxLife = 0.55f + static_cast<float>(std::rand() % 40) / 100.0f;
    pt.life = pt.maxLife;
    pt.size = 2.0f + static_cast<float>(std::rand() % 6);
    pt.glow = true;
    pt.r = static_cast<Uint8>(85 + std::rand() % 70);
    pt.g = static_cast<Uint8>(30 + std::rand() % 55);
    pt.b = static_cast<Uint8>(120 + std::rand() % 75);
    m_particles.push_back(pt);
  }

  // Slow drifting sparks in the blast radius.
  for (int i = 0; i < 55; ++i) {
    Particle pt;
    const float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
    const float dist = static_cast<float>(std::rand() % 100);
    pt.pos = {center.x + std::cos(angle) * dist, center.y + std::sin(angle) * dist};
    pt.vel = {std::cos(angle) * 40.0f, std::sin(angle) * 40.0f - 20.0f};
    pt.gravity = 0.0f;
    pt.drag = 0.6f;
    pt.maxLife = 0.75f + static_cast<float>(std::rand() % 40) / 100.0f;
    pt.life = pt.maxLife;
    pt.size = 1.5f + static_cast<float>(std::rand() % 4);
    pt.glow = true;
    pt.r = static_cast<Uint8>(90 + std::rand() % 60);
    pt.g = static_cast<Uint8>(40 + std::rand() % 45);
    pt.b = static_cast<Uint8>(150 + std::rand() % 70);
    m_particles.push_back(pt);
  }
}

void Game::SpawnBlackHoleOrbitParticle(Vec2 center, float radius, float spinHint) {
  const float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
  const float dist = radius * (0.42f + static_cast<float>(std::rand() % 58) / 100.0f);
  Particle pt;
  pt.pos = {center.x + std::cos(angle) * dist, center.y + std::sin(angle) * dist};

  Vec2 toCenter = center - pt.pos;
  const float len = toCenter.Len();
  if (len > 1e-4f) toCenter = toCenter / len;
  Vec2 tangent{-toCenter.y, toCenter.x};
  const float orbitSpeed = 140.0f + static_cast<float>(std::rand() % 180);
  const float spinDir = (std::rand() % 2 == 0) ? 1.0f : -1.0f;
  pt.vel = toCenter * (110.0f + static_cast<float>(std::rand() % 120)) +
           tangent * (orbitSpeed * spinDir + spinHint * 0.22f);
  pt.gravity = 0.0f;
  pt.drag = 0.42f;
  pt.maxLife = 0.34f + static_cast<float>(std::rand() % 36) / 100.0f;
  pt.life = pt.maxLife;
  pt.size = 1.0f + static_cast<float>(std::rand() % 3);
  pt.glow = false;
  pt.rewindSpark = false;
  const int tone = std::rand() % 100;
  if (tone < 8) {
    pt.r = 235;
    pt.g = static_cast<Uint8>(200 + std::rand() % 35);
    pt.b = static_cast<Uint8>(140 + std::rand() % 40);
  } else if (tone < 70) {
    pt.r = static_cast<Uint8>(95 + std::rand() % 65);
    pt.g = static_cast<Uint8>(35 + std::rand() % 45);
    pt.b = static_cast<Uint8>(130 + std::rand() % 60);
  } else {
    pt.r = static_cast<Uint8>(70 + std::rand() % 40);
    pt.g = static_cast<Uint8>(25 + std::rand() % 30);
    pt.b = static_cast<Uint8>(95 + std::rand() % 45);
  }
  m_particles.push_back(pt);
}

void Game::UpdateBlackHoleParticles(float dt) {
  std::vector<GravityFieldSystem::ActiveVisual> fields;
  m_fields.AppendActiveVisuals(fields);
  if (fields.empty()) {
    m_blackHoleParticleAcc = 0.0f;
    return;
  }

  m_blackHoleParticleAcc += dt;
  constexpr float kSpawnInterval = 0.014f;
  while (m_blackHoleParticleAcc >= kSpawnInterval) {
    m_blackHoleParticleAcc -= kSpawnInterval;
    for (const auto& field : fields) {
      if (field.lifeT <= 0.05f) continue;
      SpawnBlackHoleOrbitParticle(field.center, field.radius, field.spinAngle);
      if ((std::rand() % 100) < 40) {
        SpawnBlackHoleOrbitParticle(field.center, field.radius * 0.9f, field.spinAngle);
      }
    }
  }
}

void Game::SpawnRewindOrbitParticle(Vec2 playerCenter) {
  Particle pt;
  const float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
  const float dist = 90.0f + static_cast<float>(std::rand() % 260);
  pt.pos = {playerCenter.x + std::cos(angle) * dist, playerCenter.y + std::sin(angle) * dist};

  Vec2 toCenter = playerCenter - pt.pos;
  const float len = toCenter.Len();
  if (len > 1e-4f) toCenter = toCenter / len;
  Vec2 tangent{-toCenter.y, toCenter.x};
  const float spinDir = (std::rand() % 2 == 0) ? 1.0f : -1.0f;
  const float speed = 260.0f + static_cast<float>(std::rand() % 280);
  pt.vel = toCenter * speed + tangent * (spinDir * (150.0f + static_cast<float>(std::rand() % 180)));
  pt.gravity = 0.0f;
  pt.drag = 0.68f;
  pt.maxLife = 0.42f + static_cast<float>(std::rand() % 36) / 100.0f;
  pt.life = pt.maxLife;
  pt.size = 1.0f + static_cast<float>(std::rand() % 4);
  pt.glow = false;
  pt.rewindSpark = true;
  const int tone = std::rand() % 100;
  if (tone < 38) {
    pt.r = 255;
    pt.g = static_cast<Uint8>(200 + std::rand() % 45);
    pt.b = static_cast<Uint8>(70 + std::rand() % 70);
  } else if (tone < 68) {
    const Uint8 w = static_cast<Uint8>(215 + std::rand() % 40);
    pt.r = pt.g = pt.b = w;
  } else if (tone < 86) {
    pt.r = static_cast<Uint8>(80 + std::rand() % 50);
    pt.g = static_cast<Uint8>(175 + std::rand() % 70);
    pt.b = 255;
  } else {
    pt.r = static_cast<Uint8>(185 + std::rand() % 70);
    pt.g = static_cast<Uint8>(75 + std::rand() % 70);
    pt.b = 255;
  }
  m_particles.push_back(pt);
}

void Game::UpdateParticles(float dt) {
  for (auto& pt : m_particles) {
    if (pt.life <= 0.0f) continue;
    pt.life   -= dt;
    pt.pos.x  += pt.vel.x * dt;
    pt.pos.y  += pt.vel.y * dt;
    if (pt.drag > 0.0f) {
      const float damp = std::max(0.0f, 1.0f - pt.drag * dt);
      pt.vel.x *= damp;
      pt.vel.y *= damp;
    }
    pt.vel.y  += pt.gravity * dt;
  }
  m_particles.erase(
    std::remove_if(m_particles.begin(), m_particles.end(),
                   [](const Particle& p) { return p.life <= 0.0f; }),
    m_particles.end());
  if (static_cast<int>(m_particles.size()) > kMaxParticles) {
    m_particles.erase(m_particles.begin(),
                      m_particles.begin() + (m_particles.size() - static_cast<std::size_t>(kMaxParticles)));
  }
}

void Game::AddPopup(const std::string& text, float x, float y, Uint8 r, Uint8 g, Uint8 b) {
  PopupText popup;
  popup.text    = text;
  popup.x       = x;
  popup.y       = y;
  popup.maxLife = 1.2f;
  popup.life    = popup.maxLife;
  popup.r = r; popup.g = g; popup.b = b;
  m_popups.push_back(popup);
}

void Game::UpdatePopups(float dt) {
  for (auto& popup : m_popups) {
    if (popup.life <= 0.0f) continue;
    popup.life -= dt;
    popup.y    -= 40.0f * dt;
  }
  m_popups.erase(
    std::remove_if(m_popups.begin(), m_popups.end(),
                   [](const PopupText& p) { return p.life <= 0.0f; }),
    m_popups.end());
}

// 삼각형 그리기 (선 3개)
void Game::DrawTriangle(SDL_Renderer* r, float cx, float cy, float size,
                        Uint8 rr, Uint8 gg, Uint8 bb) const {
  SDL_SetRenderDrawColor(r, rr, gg, bb, 255);
  const int x1 = static_cast<int>(cx);
  const int y1 = static_cast<int>(cy - size);
  const int x2 = static_cast<int>(cx - size);
  const int y2 = static_cast<int>(cy + size);
  const int x3 = static_cast<int>(cx + size);
  const int y3 = static_cast<int>(cy + size);
  SDL_RenderDrawLine(r, x1, y1, x2, y2);
  SDL_RenderDrawLine(r, x2, y2, x3, y3);
  SDL_RenderDrawLine(r, x3, y3, x1, y1);
  // 내부 채우기 (여러 수평선)
  for (int row = 0; row < static_cast<int>(size * 2); row++) {
    const float t  = static_cast<float>(row) / (size * 2.0f);
    const float lx = static_cast<float>(x1) + t * static_cast<float>(x2 - x1);
    const float rx = static_cast<float>(x1) + t * static_cast<float>(x3 - x1);
    const int   ry = y1 + row;
    SDL_RenderDrawLine(r, static_cast<int>(lx), ry, static_cast<int>(rx), ry);
  }
}
void Game::DrawHeart(SDL_Renderer* r, float cx, float cy, float size,
                     Uint8 rr, Uint8 gg, Uint8 bb) const {
  SDL_SetRenderDrawColor(r, rr, gg, bb, 255);
  for (int row = 0; row < static_cast<int>(size * 2); row++) {
    const float t  = static_cast<float>(row) / (size * 2.0f);
    const float y  = cy - size + static_cast<float>(row);
    float halfW    = 0.0f;
    if (t < 0.5f) {
      const float tt = t * 2.0f;
      halfW = size * (0.5f + 0.5f * std::sqrt(std::max(0.0f, 1.0f - (tt - 0.5f) * (tt - 0.5f) * 4.0f)));
    } else {
      const float tt = (t - 0.5f) * 2.0f;
      halfW = size * (1.0f - tt);
    }
    SDL_RenderDrawLine(r,
      static_cast<int>(cx - halfW), static_cast<int>(y),
      static_cast<int>(cx + halfW), static_cast<int>(y));
  }
}

void Game::DrawLightning(SDL_Renderer* r, float cx, float cy, float size,
                         Uint8 rr, Uint8 gg, Uint8 bb) const {
  SDL_SetRenderDrawColor(r, rr, gg, bb, 255);
  const int x1 = static_cast<int>(cx + size * 0.2f);
  const int y1 = static_cast<int>(cy - size);
  const int x2 = static_cast<int>(cx - size * 0.1f);
  const int y2 = static_cast<int>(cy);
  const int x3 = static_cast<int>(cx + size * 0.3f);
  const int y3 = static_cast<int>(cy);
  const int x4 = static_cast<int>(cx - size * 0.2f);
  const int y4 = static_cast<int>(cy + size);
  for (int i = -2; i <= 2; i++) {
    SDL_RenderDrawLine(r, x1 + i, y1, x2 + i, y2);
    SDL_RenderDrawLine(r, x2 + i, y2, x3 + i, y3);
    SDL_RenderDrawLine(r, x3 + i, y3, x4 + i, y4);
  }
}

void Game::DrawStar(SDL_Renderer* r, float cx, float cy, float size,
                    Uint8 rr, Uint8 gg, Uint8 bb) const {
  SDL_SetRenderDrawColor(r, rr, gg, bb, 255);
  const int   points  = 5;
  const float outerR  = size;
  const float innerR  = size * 0.4f;
  const float pi      = 3.14159f;
  SDL_Point pts[11];
  for (int i = 0; i < points * 2; i++) {
    const float angle = static_cast<float>(i) * pi / static_cast<float>(points) - pi / 2.0f;
    const float rad   = (i % 2 == 0) ? outerR : innerR;
    pts[i].x = static_cast<int>(cx + std::cos(angle) * rad);
    pts[i].y = static_cast<int>(cy + std::sin(angle) * rad);
  }
  pts[10] = pts[0];
  for (int i = 0; i < 10; i++) {
    SDL_RenderDrawLine(r, pts[i].x, pts[i].y, pts[i+1].x, pts[i+1].y);
    SDL_RenderDrawLine(r, static_cast<int>(cx), static_cast<int>(cy), pts[i].x, pts[i].y);
  }
}
void Game::SpawnItem(float x, ItemType type) {
  const float groundY = static_cast<float>(m_h - 40);
  const int id = m_world.CreateCircle(14.0f, {x, groundY - 80.0f}, 0.1f, true);
  m_world.Get(id).kind = BodyKind::Item;
  Item item;
  item.bodyId    = id;
  item.type      = type;
  item.collected = false;
  m_items.push_back(item);
}

void Game::UpdateItems(float dt) {
  const auto& p    = m_world.Get(m_playerId);
  const float camLeft = CameraX() - 200.0f;
  const float camX    = CameraX();
  const float magnetRadiusSq = kItemMagnetRadius * kItemMagnetRadius;

  for (auto& item : m_items) {
    if (item.collected) continue;
    auto& body = m_world.Get(item.bodyId);
    if (!body.active) continue;
    if (body.pos.x < camLeft) { body.active = false; item.collected = true; continue; }

    const float dx = p.pos.x - body.pos.x;
    const float dy = p.pos.y - body.pos.y;
    const float distSq = dx * dx + dy * dy;

    const float dist = std::sqrt(distSq);
    if (distSq < magnetRadiusSq && distSq > 4.0f) {
      const float proximity = 1.0f - (dist / kItemMagnetRadius);
      const float pull = kItemMagnetPullSpeed * proximity * proximity * dt;
      body.pos.x += (dx / dist) * pull;
      body.pos.y += (dy / dist) * pull;
    }

    if (dist < p.circle.radius + body.circle.radius + 8.0f) {
      item.collected = true;
      body.active    = false;
      const float sx = body.pos.x - camX;
      const float sy = body.pos.y;
      if (item.type == ItemType::Health) {
        m_hp = std::min(1.0f, m_hp + 0.3f);
        m_score += 10;
        AddPopup("+HP",      sx, sy,  80, 220,  80);
        SpawnParticles(body.pos, 6,  80, 220,  80);
      } else if (item.type == ItemType::Stamina) {
        m_staminaRewind = std::min(1.5f, m_staminaRewind + 1.5f);
        m_score += 10;
        AddPopup("+STAMINA", sx, sy,  80, 160, 255);
        SpawnParticles(body.pos, 6,  80, 160, 255);
      } else {
        m_shieldTimer = 5.0f;
        m_score += 20;
        AddPopup("+SHIELD",  sx, sy, 255, 220,  60);
        SpawnParticles(body.pos, 6, 255, 220,  60);
      }
    }
  }
}

void Game::UpdateSpawn() {
  const float camRight = SpawnHorizonX();

  // 스테이지별 난이도 설정
  // 스테이지 1 (0~5000m): 쉬움
  // 스테이지 2 (5000~10000m): 중간
  // 스테이지 3 (10000~15000m): 어려움

  int maxPattern = 0;
  float minGap   = 160.0f;
  float baseGap  = 220.0f;

  if (m_distance < kGlacierStageStartM) {
    // 스테이지 1 — 쉬움
    minGap  = 160.0f;
    baseGap = 220.0f;
    if (m_distance > 300.0f)  maxPattern = 1;
    if (m_distance > 1000.0f) maxPattern = 2;
    if (m_distance > 2000.0f) maxPattern = 3;
    if (m_distance > 3000.0f) maxPattern = 4;

  } else if (m_distance < kEmeraldStageStartM) {
    // 스테이지 2 — 중간
    minGap  = 120.0f;
    baseGap = 180.0f;
    maxPattern = 3;
    if (m_distance > 6000.0f) maxPattern = 4;
    if (m_distance > 7000.0f) maxPattern = 5;
    if (m_distance > 8000.0f) maxPattern = 6;
    if (m_distance > 9000.0f) maxPattern = 7;

  } else {
    // 스테이지 3 — 어려움
    minGap  = 80.0f;
    baseGap = 140.0f;
    maxPattern = 5;
    if (m_distance > 11000.0f) maxPattern = 6;
    if (m_distance > 12000.0f) maxPattern = 7;
  }

  m_spawnGap = std::max(minGap, baseGap - m_elapsed * 0.3f);

  const float mapEndX = m_playerScreenX + kTotalMapLengthM;
  while (m_nextSpawnX < camRight && m_nextSpawnX < mapEndX) {
    const int pattern = std::rand() % (maxPattern + 1);
    SpawnPattern(m_nextSpawnX, pattern);
    if (pattern == 3)      m_nextSpawnX += m_spawnGap + 120.0f;
    else if (pattern == 2) m_nextSpawnX += m_spawnGap + 50.0f;
    else if (pattern == 6) m_nextSpawnX += m_spawnGap + 80.0f;
    else                   m_nextSpawnX += m_spawnGap;
  }

  // 아이템 랜덤 스폰 (40% 확률)
  while (m_nextItemX < camRight) {
    if (std::rand() % 10 < 4) {
      const int typeRoll = std::rand() % 10;
      ItemType type = ItemType::Health;
      if (typeRoll < 4)      type = ItemType::Health;
      else if (typeRoll < 7) type = ItemType::Stamina;
      else                   type = ItemType::Shield;
      SpawnItem(m_nextItemX, type);
    }
    m_nextItemX += std::max(250.0f, 400.0f - m_elapsed * 0.5f);
  }
}

void Game::ClampPlayerToGround() {
  auto& p = m_world.Get(m_playerId);
  const float floorY = static_cast<float>(m_h - 40) - p.circle.radius;
  if (p.pos.y > floorY) {
    p.pos.y = floorY;
    if (p.vel.y > 0.0f) p.vel.y = 0.0f;
    if (m_jumpGroundGrace <= 0.0f) p.onGround = true;
  }
}

void Game::PreventPlayerObstacleClimb() {
  auto& p = m_world.Get(m_playerId);
  const float floorY = static_cast<float>(m_h - 40) - p.circle.radius;
  for (const auto& obs : m_obstacles) {
    const auto& body = m_world.Get(obs.bodyId);
    if (!body.active) continue;
    if (obs.type == ObstacleType::Ceiling) continue;

    const float dx = p.pos.x - body.pos.x;
    const float dy = p.pos.y - body.pos.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float minDist = p.circle.radius + body.circle.radius;
    if (dist >= minDist || dist < 1e-5f) continue;

    const float penetration = minDist - dist;
    // Push down only — never touch X or the scroll lane freezes on contact.
    if (p.pos.y <= body.pos.y + body.circle.radius * 0.35f) {
      p.pos.y = std::min(p.pos.y + penetration * 0.9f, floorY);
    }
  }
}

void Game::CheckCollision() {
  const auto& p = m_world.Get(m_playerId);
  if (m_hitCooldown > 0.0f || m_shieldTimer > 0.0f) return;

  for (const auto& obs : m_obstacles) {
    const auto& body = m_world.Get(obs.bodyId);
    if (!body.active) continue;
    const float dx   = p.pos.x - body.pos.x;
    const float dy   = p.pos.y - body.pos.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < p.circle.radius + body.circle.radius + 4.0f) {
      float damage = 0.25f;
      if (obs.type == ObstacleType::Triangle) damage = 0.35f;
      if (obs.type == ObstacleType::Ceiling)  damage = 0.3f;
      if (obs.type == ObstacleType::Bounce)   damage = 0.2f;
      m_hp = std::max(0.0f, m_hp - damage);
      m_hitCooldown    = kHitInvincibleTime;
      m_blinkTimer     = kHitInvincibleTime;
      m_hitEffectTimer = 0.4f;
      SpawnParticles(p.pos, 8, 255, 80, 80);
      if (m_hp <= 0.0f) TriggerGameOver();
      return;
    }
  }

  for (int id : m_fallingIds) {
    const auto& crate = m_world.Get(id);
    if (!crate.active) continue;
    const float dx = p.pos.x - crate.pos.x;
    const float dy = p.pos.y - crate.pos.y;
    if (std::sqrt(dx * dx + dy * dy) < p.circle.radius + crate.circle.radius) {
      m_hp = std::max(0.0f, m_hp - 0.3f);
      m_hitCooldown    = kHitInvincibleTime;
      m_blinkTimer     = kHitInvincibleTime;
      m_hitEffectTimer = 0.4f;
      SpawnParticles(p.pos, 8, 255, 80, 80);
      if (m_hp <= 0.0f) TriggerGameOver();
      return;
    }
  }
}

void Game::CheckGameOver() {
  const auto& p = m_world.Get(m_playerId);
  if (p.pos.y > static_cast<float>(m_h + 100)) TriggerGameOver();
}

void Game::TriggerGameOver() {
  if (m_gameOver) return;
  m_gameOver = true;
  m_fields.ClearAll();
  m_blackHoleParticleAcc = 0.0f;
}

void Game::EnterClearState() {
  if (m_cleared) return;
  m_cleared     = true;
  m_distance    = kTotalMapLengthM;
  m_scrollSpeed = 0.0f;
  m_bombs.CancelCharge();
  m_rewindQueued = false;
  m_fields.ClearAll();
  m_blackHoleParticleAcc = 0.0f;

  auto& p = m_world.Get(m_playerId);
  p.vel.x = 0.0f;
  p.vel.y = 0.0f;

  for (int i = 0; i < 10; i++) {
    const float sx = static_cast<float>(std::rand() % m_w);
    const float sy = static_cast<float>(m_h) * 0.15f +
                     static_cast<float>(std::rand() % static_cast<int>(m_h * 0.55f));
    SpawnFireworkBurst(sx, sy);
  }
  m_fireworkCooldown = 0.15f;
  m_clearPulse       = 0.0f;

  for (int id : m_fallingIds) m_world.Get(id).active = false;
}

void Game::SpawnFireworkBurst(float screenX, float screenY) {
  static const Uint8 palette[][3] = {
      {255, 70, 70},  {255, 200, 50}, {90, 220, 255}, {180, 90, 255},
      {80, 255, 130}, {255, 120, 200}, {255, 255, 255},
  };
  const int paletteN = static_cast<int>(sizeof(palette) / sizeof(palette[0]));
  const int ci       = std::rand() % paletteN;
  const Vec2 center{CameraX() + screenX, screenY};

  for (int i = 0; i < 48; i++) {
    Particle pt;
    pt.pos     = center;
    const float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
    const float speed = 100.0f + static_cast<float>(std::rand() % 220);
    pt.vel     = {std::cos(angle) * speed, std::sin(angle) * speed - 80.0f};
    pt.maxLife = 0.7f + static_cast<float>(std::rand() % 50) / 100.0f;
    pt.life    = pt.maxLife;
    pt.r       = palette[ci][0];
    pt.g       = palette[ci][1];
    pt.b       = palette[ci][2];
    pt.glow    = true;
    pt.size    = 3.0f + static_cast<float>(std::rand() % 5);
    if (std::rand() % 4 == 0) {
      const int cj = std::rand() % paletteN;
      pt.r = palette[cj][0];
      pt.g = palette[cj][1];
      pt.b = palette[cj][2];
    }
    m_particles.push_back(pt);
  }
}

void Game::UpdateClearCelebration(float dt) {
  m_clearPulse += dt;
  m_fireworkCooldown -= dt;
  if (m_fireworkCooldown <= 0.0f) {
    m_fireworkCooldown = kClearFireworkInterval;
    const float sx = static_cast<float>(std::rand() % m_w);
    const float sy = static_cast<float>(m_h) * 0.12f +
                     static_cast<float>(std::rand() % static_cast<int>(m_h * 0.58f));
    SpawnFireworkBurst(sx, sy);
  }
}

void Game::ReturnToTitle() {
  Restart();
  m_started = false;
  m_cleared = false;
}

void Game::DrawClearOverlay(SDL_Renderer* r) const {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 0, 0, 0, 120);
  SDL_Rect dim{0, 0, m_w, m_h};
  SDL_RenderFillRect(r, &dim);

  const float pulse = 0.85f + 0.15f * std::sin(m_clearPulse * 4.0f);
  const Uint8 titleA = static_cast<Uint8>(std::clamp(pulse, 0.0f, 1.0f) * 255.0f);

  m_ui.DrawCentered(r, m_w / 2, m_h / 2 - 56, "Clear!",
                    SDL_Color{255, 220, 60, titleA});
  m_ui.DrawCentered(r, m_w / 2, m_h / 2 - 20,
                    (std::to_string(static_cast<int>(m_distance)) + "m · 2 Stages").c_str(),
                    SDL_Color{220, 220, 230, 220});
  m_ui.DrawCentered(r, m_w / 2, m_h / 2 + 12,
                    ("Score: " + std::to_string(m_score)).c_str(),
                    SDL_Color{255, 200, 80, 255});
  m_ui.DrawCenteredBlink(r, m_w / 2, m_h / 2 + 52, "SPACE : 재시작",
                         SDL_Color{120, 220, 255, 255}, m_uiBlinkPhase);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::Restart() {
  m_gameOver               = false;
  m_distance               = 0.0f;
  m_elapsed                = 0.0f;
  m_scrollSpeed            = 240.0f;
  m_stamina                = 3.0f;
  m_jumpBuffer             = 0.0f;
  m_jumpGroundGrace        = 0.0f;
  m_coyote                 = 0.0f;
  m_rewindCooldownLeft     = 0.0f;
  m_rewindQueued           = false;
  m_rewindFreezeLeft       = 0.0f;
  m_rewind                 = RewindBuffer(180);
  m_nextSpawnX             = 0.0f;
  m_spawnGap               = 200.0f;
  m_hp                     = 1.0f;
  m_hitCooldown            = 0.0f;
  m_shieldTimer            = 0.0f;
  m_blinkTimer             = 0.0f;
  m_hitEffectTimer         = 0.0f;
  m_score                  = 0;
  m_bombKillCount          = 0;
  m_patternIndex           = 0;
  m_fallingSpawnTimer      = 0.0f;
  m_fallingSpawnInterval   = 8.0f;
  m_meteorTrailAcc         = 0.0f;
  m_nextItemX              = 300.0f;
  m_stageNotifyTimer       = 0.0f;
  m_stageNotifyNum         = 0;
  m_stage2Notified         = false;
  m_stage3Notified         = false;
  m_glacierTransitionDone  = false;
  m_emeraldTransitionDone  = false;
  m_transitionTargetStage  = 2;
  m_stageTransitionPlaying = false;
  m_stageTransitionT       = 0.0f;
  m_cleared                = false;
  m_fireworkCooldown       = 0.0f;
  m_clearPulse             = 0.0f;
  m_rewindFxTimer          = 0.0f;
  m_rewindVortexAngle      = 0.0f;
  m_playerPosHistoryCount  = 0;
  m_rewindGhosts.clear();
  m_fields.ClearAll();
  m_blackHoleParticleAcc = 0.0f;

  auto& p = m_world.Get(m_playerId);
  p.pos = {140.0f, static_cast<float>(m_h - 80)};
  p.vel = {0.0f, 0.0f};
  p.onGround = false;

  for (auto& obs : m_obstacles) m_world.Get(obs.bodyId).active = false;
  m_obstacles.clear();
  m_propIds.clear();

  for (int id : m_fallingIds) m_world.Get(id).active = false;
  m_fallingIds.clear();

  for (auto& item : m_items) m_world.Get(item.bodyId).active = false;
  m_items.clear();

  m_particles.clear();
  m_popups.clear();

  m_nextSpawnX     = SpawnHorizonX();
  m_nextItemX      = m_nextSpawnX;
  m_startGraceLeft = kStartGraceSeconds;
  m_slowActive      = false;
  m_slowTimer       = 0.0f;
  m_staminaSlow     = 1.5f;
  m_staminaRewind   = 1.5f;
}

Vec2 Game::MouseWorldPos(const InputState& input) const {
  return {input.mousePos.x + CameraX(), input.mousePos.y};
}

void Game::UpdateStageTransition(float dt) {
  if (!m_started) return;

  if (m_distance < kGlacierStageStartM - 100.0f) {
    m_glacierTransitionDone  = false;
    m_emeraldTransitionDone  = false;
    m_stageTransitionPlaying = false;
    m_stageTransitionT       = 0.0f;
    return;
  }

  if (m_distance < kEmeraldStageStartM - 100.0f) {
    m_emeraldTransitionDone = false;
    if (m_stageTransitionPlaying && m_transitionTargetStage == 3) {
      m_stageTransitionPlaying = false;
      m_stageTransitionT       = 0.0f;
    }
  }

  if (!m_stageTransitionPlaying) {
    if (!m_glacierTransitionDone && m_distance >= kGlacierStageStartM && m_glacier.IsLoaded()) {
      m_stageTransitionPlaying = true;
      m_stageTransitionT       = 0.0f;
      m_transitionTargetStage  = 2;
    } else if (m_glacierTransitionDone && !m_emeraldTransitionDone && m_distance >= kEmeraldStageStartM &&
               m_emerald.IsLoaded()) {
      m_stageTransitionPlaying = true;
      m_stageTransitionT       = 0.0f;
      m_transitionTargetStage  = 3;
    }
  }

  if (!m_stageTransitionPlaying) return;

  m_stageTransitionT += dt / kStageTransitionSeconds;
  if (m_stageTransitionT >= 1.0f) {
    m_stageTransitionT       = 1.0f;
    m_stageTransitionPlaying = false;
    if (m_transitionTargetStage == 3)
      m_emeraldTransitionDone = true;
    else
      m_glacierTransitionDone = true;
  }
}

Game::VisualStage Game::ResolveVisualStage() const {
  if (m_stageTransitionPlaying) {
    if (m_transitionTargetStage == 3)
      return m_stageTransitionT >= 0.5f ? VisualStage::Emerald : VisualStage::Glacier;
    return m_stageTransitionT >= 0.5f ? VisualStage::Glacier : VisualStage::Mars;
  }
  if (m_emeraldTransitionDone && m_distance >= kEmeraldStageStartM) return VisualStage::Emerald;
  if (m_glacierTransitionDone && m_distance >= kGlacierStageStartM) return VisualStage::Glacier;
  return VisualStage::Mars;
}

ObstacleStage Game::ResolveObstacleStage() const {
  switch (ResolveVisualStage()) {
  case VisualStage::Emerald: return ObstacleStage::Emerald;
  case VisualStage::Glacier: return ObstacleStage::Glacier;
  default: return ObstacleStage::Mars;
  }
}

void Game::DrawStageBackground(SDL_Renderer* r, float camX, float groundY, VisualStage stage) const {
  if (stage == VisualStage::Emerald) {
    if (!m_emerald.IsDrawReady()) return;
    m_emerald.DrawParallaxBackground(r, m_w, m_h, camX, groundY);
    m_emerald.DrawTerrain(r, m_w, m_h, camX, groundY);
    m_emerald.DrawParallaxNear(r, m_w, m_h, camX, groundY);
    m_emerald.DrawStageObjects(r, m_w, m_h, camX, groundY);
    return;
  }
  if (stage == VisualStage::Glacier) {
    if (!m_glacier.IsDrawReady()) return;
    m_glacier.DrawParallaxBackground(r, m_w, m_h, camX, groundY);
    m_glacier.DrawTerrain(r, m_w, m_h, camX, groundY);
    m_glacier.DrawParallaxNear(r, m_w, m_h, camX, groundY);
    m_glacier.DrawStageObjects(r, m_w, m_h, camX, groundY);
    return;
  }
  if (!m_stage.IsDrawReady()) {
    SDL_SetRenderDrawColor(r, 20, 20, 24, 255);
    SDL_Rect ground{0, m_h - 40, m_w, 40};
    SDL_RenderFillRect(r, &ground);
    return;
  }
  m_stage.DrawParallaxBackground(r, m_w, m_h, camX, groundY);
  m_stage.DrawTerrain(r, m_w, m_h, camX, groundY);
  m_stage.DrawParallaxNear(r, m_w, m_h, camX, groundY);
  m_stage.DrawStageObjects(r, m_w, m_h, camX, groundY);
}

void Game::DrawStageTransitionFade(SDL_Renderer* r) const {
  if (!m_stageTransitionPlaying) return;
  const float t     = std::clamp(m_stageTransitionT, 0.0f, 1.0f);
  float alpha = t < 0.5f ? t / 0.5f : 1.0f - (t - 0.5f) / 0.5f;
  const Uint8 a = static_cast<Uint8>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 255, 255, 255, a);
  SDL_Rect full{0, 0, m_w, m_h};
  SDL_RenderFillRect(r, &full);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::DrawStageNotify(SDL_Renderer* r) const {
  if (m_stageNotifyTimer <= 0.0f) return;
  float alpha = 1.0f;
  if (m_stageNotifyTimer < 0.5f)
    alpha = m_stageNotifyTimer / 0.5f;
  else if (m_stageNotifyTimer > kStageNotifyDuration - 0.5f)
    alpha = (kStageNotifyDuration - m_stageNotifyTimer) / 0.5f;
  alpha = std::clamp(alpha, 0.0f, 1.0f);
  const Uint8 a = static_cast<Uint8>(alpha * 255.0f);
  const std::string stageText = "Stage " + std::to_string(m_stageNotifyNum);
  m_ui.DrawCentered(r, m_w / 2, m_h / 2 - 20, stageText.c_str(), SDL_Color{255, 255, 255, a});
}

void Game::DrawHitEffect(SDL_Renderer* r) const {
  if (m_hitEffectTimer <= 0.0f) return;
  const float alpha = std::clamp(m_hitEffectTimer / 0.4f, 0.0f, 1.0f);
  const Uint8 a = static_cast<Uint8>(alpha * 100.0f);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 255, 0, 0, a);
  SDL_Rect full{0, 0, m_w, m_h};
  SDL_RenderFillRect(r, &full);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::RecordPlayerPositionHistory(Vec2 pos) {
  for (int i = static_cast<int>(m_playerPosHistory.size()) - 1; i > 0; --i) {
    m_playerPosHistory[static_cast<std::size_t>(i)] = m_playerPosHistory[static_cast<std::size_t>(i - 1)];
  }
  m_playerPosHistory[0] = pos;
  m_playerPosHistoryCount = std::min(m_playerPosHistoryCount + 1,
                                     static_cast<int>(m_playerPosHistory.size()));
}

void Game::ResetPlayerPositionHistory(Vec2 pos) {
  m_playerPosHistory.fill(pos);
  m_playerPosHistoryCount = 1;
}

void Game::SpawnRewindVfx(Vec2 playerCenter) {
  m_rewindFxTimer     = kRewindFxSeconds;
  m_rewindVortexCenter = playerCenter;
  m_rewindVortexAngle  = 0.0f;
  m_rewindGhosts.clear();
  m_rewindGhosts.reserve(static_cast<std::size_t>(m_playerPosHistoryCount));

  for (int i = 0; i < m_playerPosHistoryCount; ++i) {
    if (i == 0) continue;
    RewindGhost ghost;
    ghost.pos     = m_playerPosHistory[static_cast<std::size_t>(i)];
    const float age01 = static_cast<float>(i) / std::max(1, m_playerPosHistoryCount - 1);
    ghost.age01   = age01;
    ghost.maxLife = kRewindFxSeconds * (0.55f + 0.35f * (1.0f - age01));
    ghost.life    = ghost.maxLife;
    m_rewindGhosts.push_back(ghost);
  }
  ResetPlayerPositionHistory(playerCenter);

  for (int i = 0; i < kRewindConvergeParticles; ++i) {
    Particle pt;
    const float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
    const float dist  = 80.0f + static_cast<float>(std::rand() % 220);
    pt.pos = {playerCenter.x + std::cos(angle) * dist,
              playerCenter.y + std::sin(angle) * dist};

    Vec2 toCenter = playerCenter - pt.pos;
    const float len = toCenter.Len();
    if (len > 1e-4f) toCenter = toCenter / len;
    Vec2 tangent{-toCenter.y, toCenter.x};
    const float spinDir = (std::rand() % 2 == 0) ? 1.0f : -1.0f;
    const float speed = 280.0f + static_cast<float>(std::rand() % 260);
    pt.vel     = toCenter * speed + tangent * (spinDir * (160.0f + static_cast<float>(std::rand() % 150)));
    pt.gravity = 0.0f;
    pt.drag    = 0.78f;
    pt.maxLife = 0.62f + static_cast<float>(std::rand() % 35) / 100.0f;
    pt.life    = pt.maxLife;
    pt.size    = 1.0f + static_cast<float>(std::rand() % 4);
    pt.glow        = false;
    pt.rewindSpark = true;
    const int tone = std::rand() % 100;
    if (tone < 40) {
      pt.r = 255;
      pt.g = static_cast<Uint8>(195 + std::rand() % 50);
      pt.b = static_cast<Uint8>(60 + std::rand() % 80);
    } else if (tone < 70) {
      const Uint8 w = static_cast<Uint8>(210 + std::rand() % 45);
      pt.r = pt.g = pt.b = w;
    } else {
      pt.r = static_cast<Uint8>(70 + std::rand() % 70);
      pt.g = static_cast<Uint8>(165 + std::rand() % 80);
      pt.b = 255;
    }
    m_particles.push_back(pt);
  }

  for (int i = 0; i < kRewindRingParticles; ++i) {
    Particle pt;
    const float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
    const float dist = 120.0f + static_cast<float>(std::rand() % 200);
    pt.pos = {playerCenter.x + std::cos(angle) * dist, playerCenter.y + std::sin(angle) * dist};
    Vec2 toCenter = playerCenter - pt.pos;
    const float len = toCenter.Len();
    if (len > 1e-4f) toCenter = toCenter / len;
    pt.vel = toCenter * (320.0f + static_cast<float>(std::rand() % 220));
    pt.gravity = 0.0f;
    pt.drag = 0.42f;
    pt.maxLife = 0.85f;
    pt.life = pt.maxLife;
    pt.size = 1.0f + static_cast<float>(std::rand() % 4);
    pt.glow = false;
    pt.rewindSpark = true;
    pt.r = 255;
    pt.g = static_cast<Uint8>(185 + std::rand() % 60);
    pt.b = static_cast<Uint8>(70 + std::rand() % 80);
    m_particles.push_back(pt);
  }

  for (int i = 0; i < kRewindSparkParticles; ++i) {
    Particle pt;
    const float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
    const float dist = 100.0f + static_cast<float>(std::rand() % 180);
    pt.pos = {playerCenter.x + std::cos(angle) * dist, playerCenter.y + std::sin(angle) * dist};
    Vec2 toCenter = playerCenter - pt.pos;
    const float len = toCenter.Len();
    if (len > 1e-4f) toCenter = toCenter / len;
    Vec2 tangent{-toCenter.y, toCenter.x};
    const float spinDir = (std::rand() % 2 == 0) ? 1.0f : -1.0f;
    pt.vel = toCenter * (200.0f + static_cast<float>(std::rand() % 180)) +
             tangent * (spinDir * (170.0f + static_cast<float>(std::rand() % 140)));
    pt.gravity = 0.0f;
    pt.drag = 0.50f;
    pt.maxLife = 0.55f + static_cast<float>(std::rand() % 30) / 100.0f;
    pt.life = pt.maxLife;
    pt.size = 1.0f + static_cast<float>(std::rand() % 3);
    pt.glow = false;
    pt.rewindSpark = true;
    if (std::rand() % 2 == 0) {
      pt.r = static_cast<Uint8>(175 + std::rand() % 80);
      pt.g = static_cast<Uint8>(90 + std::rand() % 90);
      pt.b = 255;
    } else {
      pt.r = 255;
      pt.g = static_cast<Uint8>(210 + std::rand() % 40);
      pt.b = static_cast<Uint8>(90 + std::rand() % 70);
    }
    m_particles.push_back(pt);
  }

  m_rewindParticleAcc = 0.0f;
}

void Game::UpdateRewindVfx(float dt) {
  m_rewindFxTimer = std::max(0.0f, m_rewindFxTimer - dt);

  for (auto& ghost : m_rewindGhosts) {
    if (ghost.life <= 0.0f) continue;
    ghost.life = std::max(0.0f, ghost.life - dt);
  }
  m_rewindGhosts.erase(
      std::remove_if(m_rewindGhosts.begin(), m_rewindGhosts.end(),
                     [](const RewindGhost& g) { return g.life <= 0.0f; }),
      m_rewindGhosts.end());
}

void Game::UpdateRewindVisuals(float dt) {
  if (m_rewindFxTimer <= 0.0f) return;

  m_rewindVortexAngle += dt * kRewindSpinRadPerSec;
  m_rewindParticleAcc += dt;
  constexpr float kSpawnInterval = 0.006f;
  while (m_rewindParticleAcc >= kSpawnInterval) {
    m_rewindParticleAcc -= kSpawnInterval;
    SpawnRewindOrbitParticle(m_rewindVortexCenter);
    if (std::rand() % 2 == 0) {
      SpawnRewindOrbitParticle(m_rewindVortexCenter);
    }
  }
}

void Game::UpdateVisualEffects(float dt) {
  if (!m_started || m_paused || m_gameOver) return;

  m_fields.AdvanceSpin(dt);
  UpdateRewindVisuals(dt);
  UpdateBlackHoleParticles(dt);
}

void Game::DrawRewindGhosts(SDL_Renderer* r, float camX) const {
  if (m_rewindGhosts.empty()) return;

  const auto& player = m_world.Get(m_playerId);
  const float radius = player.circle.radius * 0.92f;
  auto& vfx = VfxLibrary::Instance();

  m_playerSprite.EnsureUploaded(r);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (auto it = m_rewindGhosts.rbegin(); it != m_rewindGhosts.rend(); ++it) {
    const RewindGhost& ghost = *it;
    if (ghost.life <= 0.0f || ghost.maxLife <= 0.0f) continue;
    const float t = ghost.life / ghost.maxLife;
    const float sx = ghost.pos.x - camX;
    const float footY = ghost.pos.y + radius;
    const float baseAlpha = 1.0f - ghost.age01 * 0.68f;
    const Uint8 a = static_cast<Uint8>(std::clamp(t, 0.0f, 1.0f) * 200.0f * baseAlpha);
    if (a == 0) continue;

    if (m_playerSprite.IsReady()) {
      m_playerSprite.DrawGhost(r, sx, footY, a);
    }

    if (vfx.IsReady() && ghost.age01 > 0.15f) {
      vfx.DrawRewindGhostAura(r, sx, ghost.pos.y, radius * 0.55f, static_cast<Uint8>(a * 0.35f),
                              ghost.age01 * 120.0f);
    }
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::DrawRewindVortex(SDL_Renderer* r, float camX) const {
  (void)camX;
  if (m_rewindFxTimer <= 0.0f) return;

  const float lifeT = std::clamp(m_rewindFxTimer / kRewindFxSeconds, 0.0f, 1.0f);
  const float fade = VfxLibrary::Envelope(lifeT, 0.20f, 0.28f);
  const float cx = static_cast<float>(m_w) * 0.5f;
  const float cy = static_cast<float>(m_h) * 0.5f;
  const float radius = std::sqrt(static_cast<float>(m_w * m_w + m_h * m_h)) * 0.54f;
  auto& vfx = VfxLibrary::Instance();

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 6, 10, 26, static_cast<Uint8>(fade * 245.0f));
  SDL_Rect full{0, 0, m_w, m_h};
  SDL_RenderFillRect(r, &full);

  vfx.DrawRewindStarfield(r, m_w, m_h, fade, m_rewindVortexAngle);
  vfx.DrawRewindVortex(r, cx, cy, radius, lifeT, m_rewindVortexAngle);

  static const char* kClockNums[] = {"XII", "XI", "X", "IX", "VIII", "VII",
                                     "VI",  "V",  "IV", "III", "II", "I"};
  constexpr float kPi = 3.14159265f;
  for (int i = 0; i < 12; ++i) {
    const float u = 0.10f + static_cast<float>(i) / 12.0f * 0.78f;
    const float rr = radius * std::pow(0.06f, u * 0.92f);
    const float ang = m_rewindVortexAngle * 0.55f + u * 9.5f * kPi;
    const int nx = static_cast<int>(cx + std::cos(ang) * rr);
    const int ny = static_cast<int>(cy + std::sin(ang) * rr);
    const Uint8 numA = static_cast<Uint8>(fade * 230.0f);
    m_ui.DrawCentered(r, nx, ny, kClockNums[i], SDL_Color{255, 215, 105, numA});
  }

  vfx.DrawVignette(r, m_w, m_h, static_cast<Uint8>(fade * 75.0f), 0, 0, 0);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::DrawRewindScreenFx(SDL_Renderer* r) const {
  if (m_rewindFxTimer <= 0.0f) return;

  const float t = std::clamp(m_rewindFxTimer / kRewindFxSeconds, 0.0f, 1.0f);
  const float fade = VfxLibrary::Envelope(t, 0.35f, 0.35f);
  const float pulse = 0.85f + 0.15f * std::sin((1.0f - t) * 24.0f);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  const Uint8 tintA = static_cast<Uint8>(fade * pulse * 90.0f);
  SDL_SetRenderDrawColor(r, 35, 110, 220, tintA);
  SDL_Rect full{0, 0, m_w, m_h};
  SDL_RenderFillRect(r, &full);

  const Uint8 chromaA = static_cast<Uint8>(fade * 48.0f);
  SDL_SetRenderDrawColor(r, 180, 80, 255, chromaA);
  SDL_Rect leftHalf{0, 0, m_w / 2, m_h};
  SDL_RenderFillRect(r, &leftHalf);
  SDL_SetRenderDrawColor(r, 60, 200, 255, chromaA);
  SDL_Rect rightHalf{m_w / 2, 0, m_w - m_w / 2, m_h};
  SDL_RenderFillRect(r, &rightHalf);

  VfxLibrary::Instance().DrawVignette(r, m_w, m_h, static_cast<Uint8>(fade * 110.0f), 6, 10, 30);

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::HandleInput(float dt, Input& input) {
  const InputState& in = input.State();
  m_uiBlinkPhase += dt;
  UpdateStageTransition(dt);

  if (in.quit) { m_quit = true; return; }

  if (!m_started) {
    if (in.resumePressed) {
      m_started          = true;
      m_startGraceLeft   = kStartGraceSeconds;
      m_nextSpawnX       = SpawnHorizonX();
      m_nextItemX        = m_nextSpawnX;
      m_stageNotifyNum   = 1;
      m_stageNotifyTimer = kStageNotifyDuration;
    }
    m_lastInput = in;
    return;
  }

  if (m_cleared) {
    m_lastInput = in;
    return;
  }

  if (in.pausePressed) {
    if (m_paused) m_quit = true;
    else m_paused = true;
  }
  if (m_paused && in.resumePressed) m_paused = false;

  if (!m_paused) {
    auto& p = m_world.Get(m_playerId);
    if (in.rewindPressed) {
      m_bombs.CancelCharge();
      m_throwReleasePoseLeft = 0.0f;
      m_jumpBuffer = 0.0f;
      if (m_rewindCooldownLeft <= 0.0f && m_staminaRewind >= kRewindStaminaCost) {
        m_rewindQueued = true;
      }
      input.ConsumeRewindPending();
      input.ConsumeRewindPending();
      if (in.slowPressed) {
        // 슬로우모션은 FixedUpdate에서 처리
      }
    } else {
      if (m_bombs.IsCharging() && in.throwReleased) m_throwReleasePoseLeft = 0.35f;
      m_bombs.UpdateThrow(dt, in, MouseWorldPos(in), m_world, p);
      if (in.throwReleased) input.ConsumeThrowReleasedPending();
    }
    m_throwReleasePoseLeft = std::max(0.0f, m_throwReleasePoseLeft - dt);
    m_rewindPoseLeft       = std::max(0.0f, m_rewindPoseLeft - dt);
  }
  m_lastInput = in;
}

void Game::FixedUpdate(float dt, const InputState& input, Input& inputDevice) {
  if (!m_started || m_paused) return;

  if (m_gameOver) {
    if (input.resumePressed) Restart();
    return;
  }

  if (m_cleared) {
    UpdateClearCelebration(dt);
    UpdateParticles(dt);
    if (input.resumePressed) ReturnToTitle();
    return;
  }

  if (m_distance >= kTotalMapLengthM) {
    EnterClearState();
    UpdateClearCelebration(dt);
    UpdateParticles(dt);
    return;
  }

  UpdateRewindVfx(dt);

  if (m_rewindFreezeLeft > 0.0f) {
    m_rewindFreezeLeft = std::max(0.0f, m_rewindFreezeLeft - dt);
    // Prevent buffered jumps from firing when time resumes.
    m_jumpBuffer = 0.0f;
    m_coyote = 0.0f;
    inputDevice.ClearGameplayPending();
    UpdateParticles(dt);
    return;
  }

  m_rewindCooldownLeft = std::max(0.0f, m_rewindCooldownLeft - dt);
  m_hitCooldown        = std::max(0.0f, m_hitCooldown - dt);
  m_blinkTimer         = std::max(0.0f, m_blinkTimer - dt);
  m_shieldTimer        = std::max(0.0f, m_shieldTimer - dt);
  m_stageNotifyTimer   = std::max(0.0f, m_stageNotifyTimer - dt);
  m_hitEffectTimer     = std::max(0.0f, m_hitEffectTimer - dt);
// 슬로우모션 (F키)
  if (input.slowPressed && m_staminaSlow >= 0.5f && !m_slowActive) {
    m_slowActive = true;
  }
  if (m_slowActive) {
    m_staminaSlow = std::max(0.0f, m_staminaSlow - dt * 0.5f);
    if (m_staminaSlow <= 0.0f) {
      m_slowActive = false;
    }
  }
  // 슬로우 스태미나 회복
  if (!m_slowActive) {
    m_staminaSlow = std::min(1.5f, m_staminaSlow + dt * 0.1f);
  }
  const bool rewindFrame = m_rewindQueued;
  bool rewindSucceeded = false;
  if (m_rewindQueued) {
    m_rewindQueued = false;
    if (m_rewindCooldownLeft <= 0.0f && m_staminaRewind >= kRewindStaminaCost &&
        m_rewind.Size() >= m_rewind.Capacity()) {
      bool any = false;
      const std::size_t frames = m_rewind.Capacity();
      for (std::size_t i = 0; i < frames; i++) {
        if (!m_rewind.PopFrame(m_snapshotScratch)) break;
        any = true;
      }
      if (any) {
        rewindSucceeded = true;
        const float staminaBeforeRewind = m_staminaRewind;
        m_snapshotScratch.Apply(m_world, m_playerId, m_jumpBuffer, m_coyote,
                                m_stamina, m_hp, m_bombs, m_fields, m_propIds);
        m_staminaRewind = std::max(0.0f, staminaBeforeRewind - kRewindStaminaCost);
        m_rewindCooldownLeft = kRewindStaminaCost;
        m_rewindPoseLeft     = kRewindPoseSeconds;
        m_hp = std::min(1.0f, m_hp + 0.1f);
        m_popups.clear();
        const auto& rewoundPlayer = m_world.Get(m_playerId);
        SpawnRewindVfx(rewoundPlayer.pos);
        // Invincible briefly after a successful rewind.
        m_hitCooldown = kRewindInvincibleTime;
        m_blinkTimer  = kRewindInvincibleTime;
        m_rewindFreezeLeft = kRewindFreezeSeconds;
        for (int id : m_fallingIds) m_world.Get(id).active = false;
        for (auto& item : m_items) {
          if (!item.collected) m_world.Get(item.bodyId).active = false;
          item.collected = true;
        }
        UpdateParticles(dt);
        return;
      }
    }
  }
  if (rewindFrame && !rewindSucceeded) {
    m_rewindPoseLeft = 0.0f;
  }

  if (!rewindFrame && inputDevice.ConsumeJumpPressForFixedStep()) m_jumpBuffer = 0.12f;
  m_jumpBuffer      = std::max(0.0f, m_jumpBuffer - dt);
  m_jumpGroundGrace = std::max(0.0f, m_jumpGroundGrace - dt);

  m_snapshotScratch.Capture(m_world, m_playerId, m_jumpBuffer, m_coyote,
                            m_stamina, m_hp, m_bombs, m_fields, m_propIds);
  m_rewind.PushFrame(m_snapshotScratch);

  auto& p = m_world.Get(m_playerId);
  if (input.debugPressed) m_fields.ToggleDebug();

  if (!rewindFrame && m_jumpBuffer > 0.0f && (p.onGround || m_coyote > 0.0f)) {
    p.vel.y = -520.0f;
    p.pos.y -= p.circle.radius * 0.55f + 4.0f;
    p.onGround        = false;
    m_jumpGroundGrace = 0.14f;
    m_jumpBuffer      = 0.0f;
    m_coyote          = 0.0f;
  }

  m_fields.ApplyForces(m_world, m_playerId, m_bombs.BodyIds(), m_propIds);

  const float laneXBeforeStep = p.pos.x;
  const float effectiveDt = m_slowActive ? dt * 0.3f : dt;
    m_world.Step(effectiveDt);
    m_fields.FixedUpdate(effectiveDt);
    std::vector<int> bombObstacleIds;
    bombObstacleIds.reserve(m_obstacles.size() + m_fallingIds.size());
    for (const auto& obs : m_obstacles) bombObstacleIds.push_back(obs.bodyId);
    for (int id : m_fallingIds) bombObstacleIds.push_back(id);
    m_bombs.FixedUpdate(effectiveDt, m_world, m_playerId, bombObstacleIds);

  p.pos.x = laneXBeforeStep + m_scrollSpeed * effectiveDt;
  p.vel.x = m_scrollSpeed;
  PreventPlayerObstacleClimb();
  ClampPlayerToGround();

  if (m_jumpGroundGrace > 0.0f) p.onGround = false;
  else if (p.onGround) { m_coyote = 0.10f; m_runAnimPhase += dt; }
  else m_coyote = std::max(0.0f, m_coyote - dt);

  if (m_startGraceLeft > 0.0f) {
    m_startGraceLeft = std::max(0.0f, m_startGraceLeft - dt);
  }
  const bool inStartGrace = m_startGraceLeft > 0.0f;

  if (!inStartGrace) {
    const float distStep = m_scrollSpeed * dt;
    m_distance    += distStep;
    m_elapsed     += dt;
    m_scrollSpeed  = 240.0f + m_elapsed * 1.5f;
    m_hp = std::max(0.0f, m_hp - distStep / kHpSurvivalDistanceM);
    if (m_hp <= 0.0f) TriggerGameOver();
  }

  m_score = static_cast<int>(m_distance / 10.0f) + m_bombKillCount * 50;

  if (!m_stage2Notified && m_distance >= kGlacierStageStartM) {
    m_stage2Notified   = true;
    m_stageNotifyNum   = 2;
    m_stageNotifyTimer = kStageNotifyDuration;
  }
  if (!m_stage3Notified && m_distance >= kEmeraldStageStartM) {
    m_stage3Notified   = true;
    m_stageNotifyNum   = 3;
    m_stageNotifyTimer = kStageNotifyDuration;
  }

  m_staminaRewind = std::min(1.5f, m_staminaRewind + dt * 0.15f);

  // 스테이지별 떨어지는 장애물 스폰 간격
  float fallingInterval = 999.0f; // 기본값 (안 나옴)
  if (m_distance > kGlacierStageStartM) {
    // 스테이지 2부터 등장
    fallingInterval = std::max(4.0f, 8.0f - m_elapsed * 0.05f);
  }
  if (m_distance > kEmeraldStageStartM) {
    // 스테이지 3에서 더 자주
    fallingInterval = std::max(2.0f, 5.0f - m_elapsed * 0.05f);
  }

  if (m_distance > kGlacierStageStartM) {
    m_fallingSpawnTimer += dt;
    if (m_fallingSpawnTimer >= fallingInterval) {
      m_fallingSpawnTimer = 0.0f;
      SpawnFallingObstacle();
    }
  }

  // Spawn ahead at the map horizon even during start grace so obstacles approach naturally.
  UpdateSpawn();
  UpdateFallingObstacles(dt);
  UpdateObstacles(dt);
  UpdateItems(dt);
  UpdateParticles(dt);
  UpdatePopups(dt);
  CheckCollision();
  CheckGameOver();
  RecordPlayerPositionHistory(p.pos);
}

void Game::DrawGameplayHud(SDL_Renderer* r) const {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  const int leftX = kHudMargin;
  const int topY = kHudMargin;
  const int barW = kHudBarW;
  const int badgeColW = (kHudHpBadgeR + 2) * 2 + 6;
  const float badgeCx = static_cast<float>(leftX + badgeColW / 2);
  const int hpBarX = leftX + badgeColW + 4;
  const int hpBarY = topY + 2;
  const int rowStride = (kHudHpBadgeR + 2) * 2 + 10;
  const int staminaBarY = hpBarY + rowStride;
  const int brandY = staminaBarY + rowStride + 2;
  const int rightX = m_w - kHudMargin - 120;
  const int line = m_ui.LineHeight();

  m_itemSprites.EnsureUploaded(r);

  DrawHpHeartBadge(r, badgeCx, static_cast<float>(hpBarY + kHudHpBarH / 2), &m_itemSprites);
  const SDL_Rect hpBg{hpBarX, hpBarY, barW, kHudHpBarH};
  DrawCookieRunHpBarShell(r, hpBg);
  FillCookieStripedCapsule(r, hpBg, m_hp, 255, 130, 35, 255, 210, 55);
  if (m_shieldTimer > 0.0f) {
    DrawCapsuleOutline(r, hpBg, 255, 220, 60, 255);
  }

  DrawStaminaBadge(r, badgeCx, static_cast<float>(staminaBarY + kHudHpBarH / 2), &m_itemSprites);
  const SDL_Rect staminaBg{hpBarX, staminaBarY, barW, kHudHpBarH};
  DrawCookieRunBarShell(r, staminaBg, 18, 32, 22, 8, 14, 10, 36, 58, 34);
  // 슬로우 스태미나 바 (왼쪽 절반 - 파랑)
  const SDL_Rect slowBg{staminaBg.x, staminaBg.y, staminaBg.w / 2 - 2, staminaBg.h};
  FillCookieStripedCapsule(r, slowBg, m_staminaSlow / 1.5f, 70, 130, 255, 70, 130, 255);

  // 역행 스태미나 바 (오른쪽 절반 - 보라)
  const SDL_Rect rewindBg{staminaBg.x + staminaBg.w / 2 + 2, staminaBg.y, staminaBg.w / 2 - 2, staminaBg.h};
  FillCookieStripedCapsule(r, rewindBg, m_staminaRewind / 1.5f, 180, 80, 255, 180, 80, 255);

  m_ui.Draw(r, leftX, brandY, "CHRONO RUSH", SDL_Color{120, 220, 255, 220});

  char scoreBuf[32];
  std::snprintf(scoreBuf, sizeof(scoreBuf), "%d", m_score);
  m_ui.Draw(r, rightX, topY, scoreBuf, SDL_Color{255, 255, 255, 255});

  const int totalSec = static_cast<int>(m_elapsed);
  char timeBuf[16];
  std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", totalSec / 60, totalSec % 60);
  m_ui.Draw(r, rightX, topY + line + 2, timeBuf, SDL_Color{170, 210, 255, 230});

  const SDL_Rect distBg{rightX, topY + line * 2 + 8, 120, kHudEnergyH};
  DrawHudBarShell(r, distBg);
  FillGradientBarH(r, distBg, std::clamp(m_distance / kTotalMapLengthM, 0.0f, 1.0f), 255, 170, 70, 255, 220,
                   120);
  char distBuf[24];
  std::snprintf(distBuf, sizeof(distBuf), "%dm", static_cast<int>(m_distance));
  m_ui.Draw(r, rightX, distBg.y + kHudEnergyH + 4, distBuf, SDL_Color{255, 190, 90, 230});

  DrawStageNotify(r);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::DrawTitleOverlay(SDL_Renderer* r) const {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 0, 0, 0, 140);
  SDL_Rect dim{0, 0, m_w, m_h};
  SDL_RenderFillRect(r, &dim);

  const SDL_Color white{255, 255, 255, 255};
  const SDL_Color body {210, 210, 220, 255};
  const SDL_Color accent{120, 220, 255, 255};
  const int line = m_ui.LineHeight();
  const int rowStride = line + 10;

  constexpr int kTopPad = 28;
  constexpr int kBottomPad = 24;
  constexpr int kTitleGap = 10;
  constexpr int kSectionGap = 18;
  constexpr int kControlLineCount = 6;
  constexpr int kTextInsetX = 32;

  const char* lines[] = {
      "C : 점프",
      "X 홀드 / 떼기 : 폭탄",
      "마우스 : 폭탄 조준",
      "Z : 시간 역행 (3초, 스태미나 3)",
      "번개 = 스태미나 · 하트 = 체력 · 별 = 무적",
      "Esc : 일시정지",
  };

  const int headerH = kTopPad + line + kTitleGap + line + kSectionGap;
  const int controlsH = (kControlLineCount - 1) * rowStride + line;
  const int panelH = headerH + controlsH + kBottomPad;

  const SDL_Rect panel{m_w / 2 - 260, m_h / 2 - panelH / 2, 520, panelH};
  SDL_SetRenderDrawColor(r, 24, 24, 30, 245);
  SDL_RenderFillRect(r, &panel);
  SDL_SetRenderDrawColor(r, 110, 110, 130, 255);
  SDL_RenderDrawRect(r, &panel);

  int y = panel.y + kTopPad;
  m_ui.DrawCentered(r, m_w / 2, y, "Chrono Rush", white);
  y += line + kTitleGap;
  m_ui.DrawCentered(r, m_w / 2, y, "조작법", accent);

  const int controlsTop = panel.y + headerH;
  const int controlsBottom = panel.y + panel.h - kBottomPad;
  const int span = controlsBottom - controlsTop - line;

  for (int i = 0; i < kControlLineCount; i++) {
    const int lineY = (kControlLineCount <= 1)
                          ? controlsTop
                          : controlsTop + (span * i) / (kControlLineCount - 1);
    m_ui.Draw(r, panel.x + kTextInsetX, lineY, lines[i], body);
  }

  m_ui.DrawCenteredBlink(r, m_w / 2, panel.y + panel.h + 24,
                         "SPACE를 눌러 시작하세요", white, m_uiBlinkPhase);
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

  const SDL_Color yellow{255, 220, 90,  255};
  const SDL_Color hint  {220, 220, 230, 255};
  const int line = m_ui.LineHeight();
  int y = panel.y + 28;

  m_ui.DrawCentered(r, m_w / 2, y, "일시정지", yellow); y += line + 20;
  m_ui.DrawCentered(r, m_w / 2, y, "ESC : 종료", hint); y += line;
  m_ui.DrawCentered(r, m_w / 2, y, "SPACE : 재개", hint);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::DrawGameOverOverlay(SDL_Renderer* r) const {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
  SDL_Rect dim{0, 0, m_w, m_h};
  SDL_RenderFillRect(r, &dim);

  const SDL_Rect panel{m_w / 2 - 220, m_h / 2 - 140, 440, 240};
  SDL_SetRenderDrawColor(r, 24, 24, 30, 245);
  SDL_RenderFillRect(r, &panel);
  SDL_SetRenderDrawColor(r, 180, 60, 60, 255);
  SDL_RenderDrawRect(r, &panel);

  const SDL_Color red {255,  80,  80, 255};
  const SDL_Color hint{220, 220, 230, 255};
  const SDL_Color gold{255, 200,  60, 255};
  const int line = m_ui.LineHeight();
  int y = panel.y + 24;

  m_ui.DrawCentered(r, m_w / 2, y, "GAME OVER", red);  y += line + 8;
  m_ui.DrawCentered(r, m_w / 2, y,
      (std::to_string(static_cast<int>(m_distance)) + "m").c_str(),
      SDL_Color{255, 180, 60, 255});                    y += line + 4;
  m_ui.DrawCentered(r, m_w / 2, y,
      ("Score: " + std::to_string(m_score)).c_str(), gold); y += line + 4;
  m_ui.DrawCentered(r, m_w / 2, y,
      ("Bomb kills: " + std::to_string(m_bombKillCount)).c_str(), hint); y += line + 12;
  m_ui.DrawCentered(r, m_w / 2, y, "SPACE : 재시작", hint);

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::Render(SDL_Renderer* r) const {
  const float camX    = CameraX();
  const float groundY = static_cast<float>(m_h - 40);
  const bool gameplayHud = m_started && !m_paused;

  m_stage.EnsureUploaded(r);
  if (m_glacier.IsLoaded()) m_glacier.EnsureUploaded(r);
  if (m_emerald.IsLoaded()) m_emerald.EnsureUploaded(r);
  m_itemSprites.EnsureUploaded(r);

  const VisualStage visualStage = ResolveVisualStage();
  const ObstacleStage obstacleStage = ResolveObstacleStage();
  m_obstacleSprites.EnsureUploaded(r, obstacleStage);

  float blackHoleBackdrop = 1.0f;
  if (visualStage != VisualStage::Mars) {
    blackHoleBackdrop = 0.0f;
  } else if (m_stageTransitionPlaying && m_transitionTargetStage == 2) {
    blackHoleBackdrop = 1.0f - std::clamp(m_stageTransitionT, 0.0f, 1.0f);
  }

  DrawStageBackground(r, camX, groundY, visualStage);

  DrawRewindGhosts(r, camX);
  DrawRewindVortex(r, camX);

  // 플레이어 (깜빡임)
  {
    const auto& p = m_world.Get(m_playerId);
    const float screenX   = p.pos.x - camX;
    const bool rewindPose = m_rewindPoseLeft > 0.0f;
    const bool airPose    = !p.onGround || m_jumpGroundGrace > 0.0f || m_jumpBuffer > 0.0f;
    const float footY     = rewindPose
        ? static_cast<float>(m_h) * 0.5f + kPlayerDisplayHeight * 0.5f
        : p.pos.y + p.circle.radius;
    const bool blink = m_blinkTimer > 0.0f && (static_cast<int>(m_blinkTimer * 10.0f) % 2 == 0);
    if (!blink) {
      m_playerSprite.EnsureUploaded(r);
      if (m_playerSprite.IsReady()) {
        m_playerSprite.Draw(r, screenX, footY, !airPose, m_paused || !m_started,
                            rewindPose, m_bombs.IsCharging(),
                            m_throwReleasePoseLeft > 0.0f,
                            m_bombs.Charge01(), m_runAnimPhase);
      } else {
        SDL_SetRenderDrawColor(r, 255, 80, 80, 255);
        SDL_Rect rc = RectFromCircle({screenX, p.pos.y}, p.circle.radius);
        SDL_RenderFillRect(r, &rc);
      }
    }
  }

  // 장애물 렌더링
  const bool obstacleArt = m_obstacleSprites.IsReady(obstacleStage);
  for (const auto& obs : m_obstacles) {
    const auto& body = m_world.Get(obs.bodyId);
    if (!body.active) continue;
    const float sx = body.pos.x - camX;
    const float sy = body.pos.y;
    const float rad = body.circle.radius;

    if (obstacleArt) {
      const ObstacleSpriteId spriteId = ObstacleSpriteFor(obs.type);
      if (obs.type == ObstacleType::Ceiling) {
        m_obstacleSprites.DrawFromTop(r, obstacleStage, spriteId, sx, 0.0f, sy + rad);
      } else {
        m_obstacleSprites.DrawGrounded(r, obstacleStage, spriteId, sx, sy + rad, ObstacleSpriteHeight(obs.type, rad));
      }
      continue;
    }

    switch (obs.type) {
      case ObstacleType::Normal: {
        SDL_SetRenderDrawColor(r, 150, 110, 80, 255);
        SDL_Rect rc = RectFromCircle({sx, sy}, rad);
        SDL_RenderFillRect(r, &rc);
        DrawWhiteRectOutline(r, rc);
        break;
      }
      case ObstacleType::Tall: {
        SDL_SetRenderDrawColor(r, 180, 60, 60, 255);
        SDL_Rect rc = RectFromCircle({sx, sy}, rad);
        SDL_RenderFillRect(r, &rc);
        DrawWhiteRectOutline(r, rc);
        break;
      }
      case ObstacleType::Bounce: {
        SDL_SetRenderDrawColor(r, 255, 150, 30, 255);
        SDL_Rect rc = RectFromCircle({sx, sy}, rad);
        SDL_RenderFillRect(r, &rc);
        DrawWhiteRectOutline(r, rc);
        break;
      }
      case ObstacleType::Spike: {
        SDL_SetRenderDrawColor(r, 80, 200, 255, 255);
        SDL_Rect rc = RectFromCircle({sx, sy}, rad);
        SDL_RenderFillRect(r, &rc);
        DrawWhiteRectOutline(r, rc);
        break;
      }
      case ObstacleType::Triangle:
        DrawTriangle(r, sx, sy, rad, 255, 220, 40);
        break;
      case ObstacleType::Ceiling: {
        SDL_SetRenderDrawColor(r, 160, 60, 220, 255);
        SDL_Rect ceilRect{
          static_cast<int>(sx - rad), 0,
          static_cast<int>(rad * 2.0f),
          static_cast<int>(sy + rad)
        };
        SDL_RenderFillRect(r, &ceilRect);
        DrawWhiteRectOutline(r, ceilRect);
        break;
      }
      case ObstacleType::Moving: {
        SDL_SetRenderDrawColor(r, 60, 200, 100, 255);
        SDL_Rect rc = RectFromCircle({sx, sy}, rad);
        SDL_RenderFillRect(r, &rc);
        DrawWhiteRectOutline(r, rc);
        break;
      }
    }
  }

  // 떨어지는 장애물
  for (int id : m_fallingIds) {
    const auto& crate = m_world.Get(id);
    if (!crate.active) continue;
    const float sx = crate.pos.x - camX;
    const float sy = crate.pos.y;
    const float rad = crate.circle.radius;

    if (obstacleArt) {
      m_obstacleSprites.DrawCentered(r, obstacleStage, ObstacleSpriteId::Falling, sx, sy, rad * 2.8f);
    } else {
      SDL_SetRenderDrawColor(r, 150, 80, 220, 255);
      SDL_Rect rc = RectFromCircle({sx, sy}, rad);
      SDL_RenderFillRect(r, &rc);
      DrawWhiteRectOutline(r, rc);
    }
  }

// 아이템
  for (const auto& item : m_items) {
    if (item.collected) continue;
    const auto& body = m_world.Get(item.bodyId);
    if (!body.active) continue;
    const float sx = body.pos.x - camX;
    const float sy = body.pos.y;
    const float rad = body.circle.radius;

    if (m_itemSprites.IsReady()) {
      m_itemSprites.Draw(r, ItemSpriteFor(item.type), sx, sy, kItemDisplaySize);
    } else if (item.type == ItemType::Health) {
      DrawHeart(r, sx, sy, rad, 80, 220, 80);
    } else if (item.type == ItemType::Stamina) {
      DrawLightning(r, sx, sy, rad, 80, 160, 255);
    } else {
      DrawStar(r, sx, sy, rad, 255, 220, 60);
    }
  }

  // 무적 테두리
  if (m_shieldTimer > 0.0f) {
    const auto& p = m_world.Get(m_playerId);
    SDL_SetRenderDrawColor(r, 255, 220, 60, 200);
    SDL_Rect shieldRc = RectFromCircle({p.pos.x - camX, p.pos.y}, p.circle.radius + 6.0f);
    SDL_RenderDrawRect(r, &shieldRc);
  }

  // 파티클
  auto& vfx = VfxLibrary::Instance();
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (const auto& pt : m_particles) {
    if (pt.life <= 0.0f || pt.maxLife <= 0.0f) continue;
    const float lifeT = pt.life / pt.maxLife;
    const Uint8 a = static_cast<Uint8>(lifeT * 240.0f);
    const float drawSize = pt.size * (0.45f + 0.55f * lifeT);
    const int px = static_cast<int>(pt.pos.x - camX);
    const int py = static_cast<int>(pt.pos.y);

    if (vfx.IsReady()) {
      if (pt.rewindSpark) {
        vfx.DrawRewindPixelSpark(r, static_cast<float>(px), static_cast<float>(py), drawSize, pt.r, pt.g, pt.b, a);
      } else if (pt.glow) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
        vfx.DrawSpark(r, static_cast<float>(px), static_cast<float>(py), drawSize * 1.4f, pt.r, pt.g, pt.b, a);
        vfx.DrawSoftGlow(r, static_cast<float>(px), static_cast<float>(py), drawSize * 2.6f, pt.r, pt.g, pt.b,
                         static_cast<Uint8>(a * 0.65f));
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
      } else {
        SDL_SetRenderDrawColor(r, pt.r, pt.g, pt.b, a);
        const int half = static_cast<int>(drawSize);
        SDL_Rect rc{px - half, py - half, half * 2, half * 2};
        SDL_RenderFillRect(r, &rc);
      }
    } else {
      SDL_SetRenderDrawColor(r, pt.r, pt.g, pt.b, a);
      const int half = static_cast<int>(drawSize);
      SDL_Rect rc{px - half, py - half, half * 2, half * 2};
      SDL_RenderFillRect(r, &rc);
    }
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  if (!m_cleared && !m_gameOver) {
    m_fields.Render(r, camX, blackHoleBackdrop);
  }
  const bool showAimGuide = m_started && m_rewindPoseLeft <= 0.0f && !m_lastInput.rewindHeld;
  m_bombs.Render(r, camX, groundY, m_world, m_world.Get(m_playerId),
                 MouseWorldPos(m_lastInput), showAimGuide);

  DrawStageTransitionFade(r);
  DrawRewindScreenFx(r);
  DrawHitEffect(r);

  // 팝업 텍스트
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (const auto& popup : m_popups) {
    if (popup.life <= 0.0f) continue;
    const Uint8 a = static_cast<Uint8>((popup.life / popup.maxLife) * 255.0f);
    m_ui.Draw(r, static_cast<int>(popup.x), static_cast<int>(popup.y),
              popup.text.c_str(), SDL_Color{popup.r, popup.g, popup.b, a});
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  if (gameplayHud && !m_cleared) {
    DrawGameplayHud(r);
  }

  if (!m_started)       DrawTitleOverlay(r);
  else if (m_cleared)   DrawClearOverlay(r);
  else if (m_gameOver)  DrawGameOverOverlay(r);
  else if (m_paused)    DrawPauseOverlay(r);
}

} // namespace cr