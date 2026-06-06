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
constexpr float kRewindPoseSeconds   = 1.0f;
constexpr float kRewindStaminaCost   = 3.0f;
constexpr float kRewindFxSeconds     = 1.0f; // 0.5s fade-in + 0.5s fade-out
constexpr int   kRewindConvergeParticles = 36;
constexpr float kStageNotifyDuration = 3.0f;
constexpr float kHitInvincibleTime   = 0.5f;
constexpr float kRewindInvincibleTime = 2.0f;
constexpr float kRewindFreezeSeconds  = 0.5f;
constexpr float kStartGraceSeconds    = 1.0f;
constexpr float kItemMagnetRadius     = 110.0f;
constexpr float kItemMagnetPullSpeed  = 95.0f;

// In-game HUD layout (1280x720 baseline; avoids bar/key/score overlap).
constexpr int kHudMargin   = 12;
constexpr int kHudBarW     = 200;
constexpr int kHudBarH     = 10;
constexpr int kHudBarGap   = 4;
constexpr int kHudKeyStep  = 26;
constexpr int kHudDistBarW = 210;
constexpr float kClearFireworkInterval = 0.35f;

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
  m_bombs.SetExplosionHandler([this](Vec2 center) {
    m_fields.SpawnBlackHole(center);
    for (auto& obs : m_obstacles) {
      auto& body = m_world.Get(obs.bodyId);
      if (!body.active) continue;
      const float dx   = body.pos.x - center.x;
      const float dy   = body.pos.y - center.y;
      const float dist = std::sqrt(dx * dx + dy * dy);
      if (dist < 95.0f) {
        SpawnParticles(body.pos, 12, 255, 160, 40);
        m_score += 50;
        m_bombKillCount++;
        if (dist < 60.0f) {
          // 아주 가까우면 날아가다가 사라짐
          const float force = 800.0f / (dist + 1.0f);
          const float nx = dist > 0.0f ? dx / dist : 1.0f;
          const float ny = dist > 0.0f ? dy / dist : 0.0f;
          body.linearDamping = 0.1f;
          body.groundFriction = 0.0f;
          body.invMass = 1.0f / 1.8f;
          body.vel.x += nx * force;
          body.vel.y += ny * force - 200.0f;
          // 1초 후 사라지게 타이머 설정 (obs에 저장)
          obs.bounceTimer = -1.0f; // -1 = 날아가는 중
        } else {
          // 범위 안이지만 멀면 그냥 날아감
          const float force = 400.0f / (dist + 1.0f);
          body.linearDamping = 0.1f;
          body.groundFriction = 0.0f;
          const float nx = dist > 0.0f ? dx / dist : 1.0f;
          body.invMass = 1.0f / 1.8f;
          const float ny = dist > 0.0f ? dy / dist : 0.0f;
          body.vel.x += nx * force;
          body.vel.y += ny * force - 150.0f;
        }
      }
    }
    for (int id : m_fallingIds) {
      auto& crate = m_world.Get(id);
      if (!crate.active) continue;
      const float dx = crate.pos.x - center.x;
      const float dy = crate.pos.y - center.y;
      if (std::sqrt(dx * dx + dy * dy) < 95.0f) {
        SpawnParticles(crate.pos, 8, 150, 80, 220);
        crate.active = false;
        m_score += 30;
      }
    }
  });

  m_nextSpawnX = m_playerScreenX + static_cast<float>(width) + 400.0f;
  m_nextItemX  = m_nextSpawnX;

  if (!m_playerSprite.Load())  Log(LogLevel::Error, "Failed to load assets/player/*.png");
  if (!m_stage.LoadMars())     Log(LogLevel::Warn,  "Mars stage assets missing");
  if (!m_glacier.Load())       Log(LogLevel::Warn,  "Glacier stage assets missing");
}

Game::~Game() { m_ui.Shutdown(); }

void Game::InitRenderer(SDL_Renderer* /*renderer*/) {
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
  const float spawnX = player.pos.x + 400.0f + static_cast<float>(std::rand() % 200);
  const int id = m_world.CreateCircle(20.0f, {spawnX, -30.0f}, 1.5f, false);
  auto& crate = m_world.Get(id);
  crate.kind = BodyKind::Obstacle;
  crate.restitution = 0.3f; crate.linearDamping = 0.2f; crate.groundFriction = 0.5f;
  m_fallingIds.push_back(id);
}

void Game::UpdateFallingObstacles() {
  const float camLeft = CameraX() - 200.0f;
  for (int id : m_fallingIds) {
    auto& crate = m_world.Get(id);
    if (!crate.active) continue;
    if (crate.pos.x < camLeft) crate.active = false;
  }
}

void Game::UpdateObstacles(float dt) {
  const float camLeft = CameraX() - 200.0f;
  for (auto& obs : m_obstacles) {
    auto& body = m_world.Get(obs.bodyId);
    if (!body.active) continue;
    if (body.pos.x < camLeft) { body.active = false; continue; }

    if (obs.type == ObstacleType::Bounce) {
      obs.bounceTimer += dt;
      if (obs.bounceTimer > 1.2f && body.onGround) {
        body.vel.y = -320.0f;
        obs.bounceTimer = 0.0f;
      }
    } else if (obs.type == ObstacleType::Moving) {
      obs.moveTimer += dt;
      // 사인파로 앞뒤 이동
      body.pos.x = obs.moveOriginX + std::sin(obs.moveTimer * 2.0f) * obs.moveRange;
      body.vel.x = 0.0f;
    }
    // 날아가는 상자 처리 (bounceTimer == -1)
    if (obs.bounceTimer < 0.0f) {
      obs.bounceTimer -= dt;
      if (obs.bounceTimer < -1.5f) {
        SpawnParticles(body.pos, 6, 255, 160, 40);
        body.active = false;
      }
    }
  }
}

void Game::SpawnParticles(Vec2 center, int count, Uint8 r, Uint8 g, Uint8 b) {
  for (int i = 0; i < count; i++) {
    Particle pt;
    pt.pos = center;
    const float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
    const float speed = 80.0f + static_cast<float>(std::rand() % 160);
    pt.vel    = {std::cos(angle) * speed, std::sin(angle) * speed - 100.0f};
    pt.maxLife = 0.4f + static_cast<float>(std::rand() % 40) / 100.0f;
    pt.life   = pt.maxLife;
    pt.r = r; pt.g = g; pt.b = b;
    m_particles.push_back(pt);
  }
}

void Game::UpdateParticles(float dt) {
  for (auto& pt : m_particles) {
    if (pt.life <= 0.0f) continue;
    pt.life   -= dt;
    pt.pos.x  += pt.vel.x * dt;
    pt.pos.y  += pt.vel.y * dt;
    pt.vel.y  += pt.gravity * dt;
  }
  m_particles.erase(
    std::remove_if(m_particles.begin(), m_particles.end(),
                   [](const Particle& p) { return p.life <= 0.0f; }),
    m_particles.end());
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
        m_stamina = std::min(3.0f, m_stamina + 1.5f);
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
  m_spawnGap = std::max(80.0f, 200.0f - m_elapsed * 0.5f);

  // 난이도별 패턴 확장
  // 0~300m   : 패턴 0 (낮은 상자)
  // 300~600m : + 패턴 1 (높은 상자)
  // 600~1000m: + 패턴 2,3 (연속/조합)
  // 1000~1500m: + 패턴 4,5 (바운스/삼각형)
  // 1500~2000m: + 패턴 6 (천장)
  // 2000m~   : + 패턴 7 (움직이는 장애물)
  int maxPattern = 0;
  if (m_distance > 300.0f)  maxPattern = 1;
  if (m_distance > 600.0f)  maxPattern = 3;
  if (m_distance > 1000.0f) maxPattern = 5;
  if (m_distance > 1500.0f) maxPattern = 6;
  if (m_distance > 2000.0f) maxPattern = 7;

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
      if (m_hp <= 0.0f) m_gameOver = true;
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
      if (m_hp <= 0.0f) m_gameOver = true;
      return;
    }
  }
}

void Game::CheckGameOver() {
  const auto& p = m_world.Get(m_playerId);
  if (p.pos.y > static_cast<float>(m_h + 100)) m_gameOver = true;
}

void Game::EnterClearState() {
  if (m_cleared) return;
  m_cleared     = true;
  m_distance    = kTotalMapLengthM;
  m_scrollSpeed = 0.0f;
  m_bombs.CancelCharge();
  m_rewindQueued = false;

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
  m_nextItemX              = 300.0f;
  m_stageNotifyTimer       = 0.0f;
  m_stageNotifyNum         = 0;
  m_stage2Notified         = false;
  m_glacierTransitionDone  = false;
  m_stageTransitionPlaying = false;
  m_stageTransitionT       = 0.0f;
  m_cleared                = false;
  m_fireworkCooldown       = 0.0f;
  m_clearPulse             = 0.0f;
  m_rewindFxTimer          = 0.0f;
  m_rewindVortexAngle      = 0.0f;
  m_playerPosHistoryCount  = 0;
  m_rewindGhosts.clear();

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
}

Vec2 Game::MouseWorldPos(const InputState& input) const {
  return {input.mousePos.x + CameraX(), input.mousePos.y};
}

void Game::UpdateStageTransition(float dt) {
  if (!m_started || !m_glacier.IsLoaded()) return;

  if (m_distance < kGlacierStageStartM - 100.0f) {
    m_glacierTransitionDone  = false;
    m_stageTransitionPlaying = false;
    m_stageTransitionT       = 0.0f;
    return;
  }

  if (!m_glacierTransitionDone && m_distance >= kGlacierStageStartM && !m_stageTransitionPlaying) {
    m_stageTransitionPlaying = true;
    m_stageTransitionT       = 0.0f;
  }

  if (!m_stageTransitionPlaying) return;

  m_stageTransitionT += dt / kStageTransitionSeconds;
  if (m_stageTransitionT >= 1.0f) {
    m_stageTransitionT       = 1.0f;
    m_stageTransitionPlaying = false;
    m_glacierTransitionDone  = true;
  }
}

void Game::DrawStageBackground(SDL_Renderer* r, float camX, float groundY, bool useGlacier) const {
  if (useGlacier) {
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
  // Vortex should be screen-space centered.
  m_rewindVortexCenter = playerCenter;
  m_rewindVortexAngle  = 0.0f;
  m_rewindGhosts.clear();
  m_rewindGhosts.reserve(static_cast<std::size_t>(m_playerPosHistoryCount));

  for (int i = 0; i < m_playerPosHistoryCount; ++i) {
    RewindGhost ghost;
    ghost.pos     = m_playerPosHistory[static_cast<std::size_t>(i)];
    const float age01 = static_cast<float>(i) / std::max(1, m_playerPosHistoryCount - 1);
    ghost.maxLife = kRewindFxSeconds * (0.55f + 0.35f * (1.0f - age01));
    ghost.life    = ghost.maxLife;
    m_rewindGhosts.push_back(ghost);
  }
  ResetPlayerPositionHistory(playerCenter);

  for (int i = 0; i < kRewindConvergeParticles; ++i) {
    Particle pt;
    const float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
    const float dist  = 70.0f + static_cast<float>(std::rand() % 110);
    pt.pos = {playerCenter.x + std::cos(angle) * dist,
              playerCenter.y + std::sin(angle) * dist};

    Vec2 toCenter = playerCenter - pt.pos;
    const float len = toCenter.Len();
    if (len > 1e-4f) toCenter = toCenter / len;
    Vec2 tangent{-toCenter.y, toCenter.x};
    const float spinDir = (std::rand() % 2 == 0) ? 1.0f : -1.0f;
    const float speed = 220.0f + static_cast<float>(std::rand() % 160);
    pt.vel     = toCenter * speed + tangent * (spinDir * (80.0f + static_cast<float>(std::rand() % 90)));
    pt.gravity = 0.0f;
    pt.maxLife = 0.45f + static_cast<float>(std::rand() % 25) / 100.0f;
    pt.life    = pt.maxLife;
    pt.r       = static_cast<Uint8>(90 + std::rand() % 70);
    pt.g       = static_cast<Uint8>(150 + std::rand() % 80);
    pt.b       = 255;
    m_particles.push_back(pt);
  }
}

void Game::UpdateRewindVfx(float dt) {
  m_rewindFxTimer = std::max(0.0f, m_rewindFxTimer - dt);
  if (m_rewindFxTimer > 0.0f) m_rewindVortexAngle += dt * 14.0f;
  for (auto& ghost : m_rewindGhosts) {
    if (ghost.life <= 0.0f) continue;
    ghost.life = std::max(0.0f, ghost.life - dt);
  }
  m_rewindGhosts.erase(
      std::remove_if(m_rewindGhosts.begin(), m_rewindGhosts.end(),
                     [](const RewindGhost& g) { return g.life <= 0.0f; }),
      m_rewindGhosts.end());
}

void Game::DrawRewindGhosts(SDL_Renderer* r, float camX) const {
  if (m_rewindGhosts.empty()) return;

  const auto& player = m_world.Get(m_playerId);
  const float radius = player.circle.radius * 0.92f;

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (const RewindGhost& ghost : m_rewindGhosts) {
    if (ghost.life <= 0.0f || ghost.maxLife <= 0.0f) continue;
    const float t = ghost.life / ghost.maxLife;
    const Uint8 a = static_cast<Uint8>(std::clamp(t, 0.0f, 1.0f) * 150.0f);
    SDL_SetRenderDrawColor(r, 120, 210, 255, a);
    const float sx = ghost.pos.x - camX;
    SDL_Rect rc = RectFromCircle({sx, ghost.pos.y}, radius);
    SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 180, 120, 255, static_cast<Uint8>(a * 0.7f));
    SDL_RenderDrawRect(r, &rc);
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::DrawRewindVortex(SDL_Renderer* r, float camX) const {
  (void)camX;
  if (m_rewindFxTimer <= 0.0f) return;

  const float t = std::clamp(m_rewindFxTimer / kRewindFxSeconds, 0.0f, 1.0f);
  const float cx = static_cast<float>(m_w) * 0.5f;
  const float cy = static_cast<float>(m_h) * 0.5f;
  constexpr float kPi = 3.14159265f;
  const float maxR = std::sqrt(static_cast<float>(m_w * m_w + m_h * m_h)) * 2.4f;

  // 0.5s fade-in, 0.5s fade-out.
  float fade = 0.0f;
  if (t < 0.5f) fade = t / 0.5f;
  else fade = (1.0f - t) / 0.5f;
  fade = std::clamp(fade, 0.0f, 1.0f);

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  // Darken the screen slightly to boost contrast.
  const Uint8 dimA = static_cast<Uint8>(fade * 120.0f);
  SDL_SetRenderDrawColor(r, 0, 0, 0, dimA);
  SDL_Rect full{0, 0, m_w, m_h};
  SDL_RenderFillRect(r, &full);

  constexpr int kArms = 6;
  constexpr int kSegments = 80;
  for (int arm = 0; arm < kArms; ++arm) {
    const float armBase = static_cast<float>(arm) * (2.0f * kPi / static_cast<float>(kArms));
    for (int s = 0; s < kSegments; ++s) {
      const float u0 = static_cast<float>(s) / static_cast<float>(kSegments);
      const float u1 = static_cast<float>(s + 1) / static_cast<float>(kSegments);
      const float r0 = 8.0f + u0 * maxR;
      const float r1 = 8.0f + u1 * maxR;
      const float a0 = armBase + m_rewindVortexAngle + u0 * 8.0f * kPi;
      const float a1 = armBase + m_rewindVortexAngle + u1 * 8.0f * kPi;
      const Uint8 a = static_cast<Uint8>(fade * (1.0f - u0) * 255.0f);
      // Thick line: draw 3 parallel lines.
      for (int oy = -1; oy <= 1; ++oy) {
        SDL_SetRenderDrawColor(r, 255, 255, 255, static_cast<Uint8>(a * 0.35f));
        SDL_RenderDrawLine(r,
                           static_cast<int>(cx + std::cos(a0) * r0),
                           static_cast<int>(cy + std::sin(a0) * r0) + oy,
                           static_cast<int>(cx + std::cos(a1) * r1),
                           static_cast<int>(cy + std::sin(a1) * r1) + oy);
        SDL_SetRenderDrawColor(r, 60, 190, 255, a);
        SDL_RenderDrawLine(r,
                           static_cast<int>(cx + std::cos(a0) * r0),
                           static_cast<int>(cy + std::sin(a0) * r0) + oy,
                           static_cast<int>(cx + std::cos(a1) * r1),
                           static_cast<int>(cy + std::sin(a1) * r1) + oy);
      }
    }
  }

  const Uint8 coreA = static_cast<Uint8>(fade * 255.0f);
  SDL_SetRenderDrawColor(r, 190, 120, 255, coreA);
  SDL_Rect core = RectFromCircle({cx, cy}, 44.0f * fade + 14.0f);
  SDL_RenderFillRect(r, &core);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::DrawRewindScreenFx(SDL_Renderer* r) const {
  if (m_rewindFxTimer <= 0.0f) return;

  const float t = std::clamp(m_rewindFxTimer / kRewindFxSeconds, 0.0f, 1.0f);
  const float ease = t * t;
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  const Uint8 tintA = static_cast<Uint8>(ease * 72.0f);
  SDL_SetRenderDrawColor(r, 40, 120, 220, tintA);
  SDL_Rect full{0, 0, m_w, m_h};
  SDL_RenderFillRect(r, &full);

  const Uint8 edgeA = static_cast<Uint8>(ease * 130.0f);
  SDL_SetRenderDrawColor(r, 8, 10, 28, edgeA);
  constexpr int band = 72;
  SDL_Rect top{0, 0, m_w, band};
  SDL_Rect bottom{0, m_h - band, m_w, band};
  SDL_Rect left{0, 0, band, m_h};
  SDL_Rect right{m_w - band, 0, band, m_h};
  SDL_RenderFillRect(r, &top);
  SDL_RenderFillRect(r, &bottom);
  SDL_RenderFillRect(r, &left);
  SDL_RenderFillRect(r, &right);

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
      if (m_rewindCooldownLeft <= 0.0f && m_stamina >= kRewindStaminaCost) {
        m_rewindQueued = true;
      }
      input.ConsumeRewindPending();
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
    if (inputDevice.ConsumeJumpPressForFixedStep() || input.jumpPressed || input.jumpHeld) Restart();
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

  const bool rewindFrame = m_rewindQueued;
  bool rewindSucceeded = false;
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
        rewindSucceeded = true;
        const float staminaBeforeRewind = m_stamina;
        m_snapshotScratch.Apply(m_world, m_playerId, m_jumpBuffer, m_coyote,
                                m_stamina, m_hp, m_bombs, m_fields, m_propIds);
        m_stamina = std::max(0.0f, staminaBeforeRewind - kRewindStaminaCost);
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
  m_world.Step(dt);
  m_fields.FixedUpdate(dt);
  std::vector<int> bombObstacleIds;
  bombObstacleIds.reserve(m_obstacles.size() + m_fallingIds.size());
  for (const auto& obs : m_obstacles) bombObstacleIds.push_back(obs.bodyId);
  for (int id : m_fallingIds) bombObstacleIds.push_back(id);
  m_bombs.FixedUpdate(dt, m_world, m_playerId, bombObstacleIds);

  p.pos.x = laneXBeforeStep + m_scrollSpeed * dt;
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
    m_distance    += m_scrollSpeed * dt;
    m_elapsed     += dt;
    m_scrollSpeed  = 240.0f + m_elapsed * 1.5f;
  }
  m_score = static_cast<int>(m_distance / 10.0f) + m_bombKillCount * 50;

  if (!m_stage2Notified && m_distance >= kGlacierStageStartM) {
    m_stage2Notified   = true;
    m_stageNotifyNum   = 2;
    m_stageNotifyTimer = kStageNotifyDuration;
  }

  m_hp = std::max(0.0f, m_hp - dt * 0.02f);
  if (m_hp <= 0.0f) m_gameOver = true;

  m_stamina = std::min(3.0f, m_stamina + dt * 0.15f);

  if (m_distance > 300.0f) {
    m_fallingSpawnInterval = std::max(3.0f, 8.0f - m_elapsed * 0.1f);
    m_fallingSpawnTimer += dt;
    if (m_fallingSpawnTimer >= m_fallingSpawnInterval) {
      m_fallingSpawnTimer = 0.0f;
      SpawnFallingObstacle();
    }
  }

  // Spawn ahead at the map horizon even during start grace so obstacles approach naturally.
  UpdateSpawn();
  UpdateFallingObstacles();
  UpdateObstacles(dt);
  UpdateItems(dt);
  UpdateParticles(dt);
  UpdatePopups(dt);
  CheckCollision();
  CheckGameOver();
  RecordPlayerPositionHistory(p.pos);
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
  m_ui.DrawCentered(r, m_w / 2, y, "C : 재시작", hint);

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::Render(SDL_Renderer* r) const {
  const float camX    = CameraX();
  const float groundY = static_cast<float>(m_h - 40);
  const bool gameplayHud = m_started && !m_paused;

  m_stage.EnsureUploaded(r);
  if (m_glacier.IsLoaded()) m_glacier.EnsureUploaded(r);

  bool useGlacier = false;
  if (m_stageTransitionPlaying && m_glacier.IsDrawReady())
    useGlacier = m_stageTransitionT >= 0.5f;
  else if (m_glacierTransitionDone && m_glacier.IsDrawReady() && m_distance >= kGlacierStageStartM)
    useGlacier = true;

  DrawStageBackground(r, camX, groundY, useGlacier);

  auto drawKey = [&](int x, int y, bool held, bool pressed, SDL_Color base) {
    SDL_Rect bg{x, y, 22, 22};
    SDL_SetRenderDrawColor(r, 18, 18, 22, 255);
    SDL_RenderFillRect(r, &bg);
    SDL_Color c = base;
    if (!held) { c.r /= 3; c.g /= 3; c.b /= 3; }
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    SDL_Rect fg{x + 3, y + 3, 16, 16};
    SDL_RenderFillRect(r, &fg);
    if (pressed) { SDL_SetRenderDrawColor(r, 255, 255, 255, 255); SDL_RenderDrawRect(r, &bg); }
  };

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
  for (const auto& obs : m_obstacles) {
    const auto& body = m_world.Get(obs.bodyId);
    if (!body.active) continue;
    const float sx = body.pos.x - camX;
    const float sy = body.pos.y;
    const float rad = body.circle.radius;

    switch (obs.type) {
      case ObstacleType::Normal: {
        SDL_SetRenderDrawColor(r, 150, 110, 80, 255);
        SDL_Rect rc = RectFromCircle({sx, sy}, rad);
        SDL_RenderFillRect(r, &rc);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
        SDL_RenderDrawRect(r, &rc);
        break;
      }
      case ObstacleType::Tall: {
        SDL_SetRenderDrawColor(r, 180, 60, 60, 255);
        SDL_Rect rc = RectFromCircle({sx, sy}, rad);
        SDL_RenderFillRect(r, &rc);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
        SDL_RenderDrawRect(r, &rc);
        break;
      }
      case ObstacleType::Bounce: {
        SDL_SetRenderDrawColor(r, 255, 150, 30, 255);
        SDL_Rect rc = RectFromCircle({sx, sy}, rad);
        SDL_RenderFillRect(r, &rc);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
        SDL_RenderDrawRect(r, &rc);
        break;
      }
      case ObstacleType::Spike: {
        SDL_SetRenderDrawColor(r, 80, 200, 255, 255);
        SDL_Rect rc = RectFromCircle({sx, sy}, rad);
        SDL_RenderFillRect(r, &rc);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
        SDL_RenderDrawRect(r, &rc);
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
        SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
        SDL_RenderDrawRect(r, &ceilRect);
        break;
      }
      case ObstacleType::Moving: {
        SDL_SetRenderDrawColor(r, 60, 200, 100, 255);
        SDL_Rect rc = RectFromCircle({sx, sy}, rad);
        SDL_RenderFillRect(r, &rc);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
        SDL_RenderDrawRect(r, &rc);
        break;
      }
    }
  }

  // 떨어지는 장애물 (보라색)
  for (int id : m_fallingIds) {
    const auto& crate = m_world.Get(id);
    if (!crate.active) continue;
    SDL_SetRenderDrawColor(r, 150, 80, 220, 255);
    SDL_Rect rc = RectFromCircle({crate.pos.x - camX, crate.pos.y}, crate.circle.radius);
    SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
    SDL_RenderDrawRect(r, &rc);
  }

// 아이템
  for (const auto& item : m_items) {
    if (item.collected) continue;
    const auto& body = m_world.Get(item.bodyId);
    if (!body.active) continue;
    const float sx  = body.pos.x - camX;
    const float sy  = body.pos.y;
    const float rad = body.circle.radius;

    if (item.type == ItemType::Health) {
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
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (const auto& pt : m_particles) {
    if (pt.life <= 0.0f) continue;
    const Uint8 a = static_cast<Uint8>((pt.life / pt.maxLife) * 220.0f);
    SDL_SetRenderDrawColor(r, pt.r, pt.g, pt.b, a);
    const int px = static_cast<int>(pt.pos.x - camX);
    const int py = static_cast<int>(pt.pos.y);
    SDL_Rect rc{px - 3, py - 3, 6, 6};
    SDL_RenderFillRect(r, &rc);
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  m_fields.Render(r, camX);
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
    const int leftX  = kHudMargin;
    const int topY   = kHudMargin;
    const int hpY    = topY + kHudBarH + kHudBarGap;
    const int keysY  = hpY + kHudBarH + kHudBarGap + 6;
    const int distX  = m_w - kHudMargin - kHudDistBarW;

    // 좌상: 스태미나 → 체력 (세로 스택)
    SDL_Rect stamBg{leftX, topY, kHudBarW, kHudBarH};
    SDL_SetRenderDrawColor(r, 40, 40, 48, 255);
    SDL_RenderFillRect(r, &stamBg);
    SDL_Rect stamFg{leftX, topY, static_cast<int>((m_stamina / 3.0f) * kHudBarW), kHudBarH};
    SDL_SetRenderDrawColor(r, 110, 255, 140, 255);
    SDL_RenderFillRect(r, &stamFg);

    SDL_Rect hpBg{leftX, hpY, kHudBarW, kHudBarH};
    SDL_SetRenderDrawColor(r, 40, 40, 48, 255);
    SDL_RenderFillRect(r, &hpBg);
    SDL_Rect hpFg{leftX, hpY, static_cast<int>(m_hp * kHudBarW), kHudBarH};
    SDL_SetRenderDrawColor(r,
      static_cast<Uint8>(255 * (1.0f - m_hp)),
      static_cast<Uint8>(255 * m_hp), 60, 255);
    SDL_RenderFillRect(r, &hpFg);
    if (m_shieldTimer > 0.0f) {
      SDL_SetRenderDrawColor(r, 255, 220, 60, 255);
      SDL_RenderDrawRect(r, &hpBg);
    }

    // 좌하(바 아래): C / X / Z 키 힌트
    drawKey(leftX, keysY, m_lastInput.jumpHeld, m_lastInput.jumpPressed, {90, 220, 255, 255});
    drawKey(leftX + kHudKeyStep, keysY, m_lastInput.throwHeld, m_lastInput.throwPressed, {255, 220, 80, 255});
    drawKey(leftX + kHudKeyStep * 2, keysY, m_lastInput.rewindHeld, m_lastInput.rewindPressed,
            {180, 80, 255, 255});

    // 상단 중앙: 점수 (좌·우 컬럼과 분리)
    m_ui.DrawCentered(r, m_w / 2, topY,
                      ("Score: " + std::to_string(m_score)).c_str(),
                      SDL_Color{255, 220, 60, 255});

    // 우상: 진행 거리 바 + 숫자 (체력 바와 같은 두 번째 줄)
    SDL_Rect distBg{distX, topY, kHudDistBarW, kHudBarH};
    SDL_SetRenderDrawColor(r, 40, 40, 48, 255);
    SDL_RenderFillRect(r, &distBg);
    const int distFillW = static_cast<int>(
        std::clamp(m_distance / kTotalMapLengthM, 0.0f, 1.0f) * kHudDistBarW);
    SDL_Rect distFg{distX, topY, distFillW, kHudBarH};
    SDL_SetRenderDrawColor(r, 255, 180, 60, 255);
    SDL_RenderFillRect(r, &distFg);
    m_ui.DrawCentered(r, distX + kHudDistBarW / 2, hpY,
                      (std::to_string(static_cast<int>(m_distance)) + "m").c_str(),
                      SDL_Color{255, 180, 60, 255});

    DrawStageNotify(r);
  }

  if (!m_started)       DrawTitleOverlay(r);
  else if (m_cleared)   DrawClearOverlay(r);
  else if (m_gameOver)  DrawGameOverOverlay(r);
  else if (m_paused)    DrawPauseOverlay(r);
}

} // namespace cr