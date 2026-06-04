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
constexpr float kStageNotifyDuration = 3.0f;
constexpr float kHitInvincibleTime   = 0.5f;

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
  p.restitution   = 0.0f;
  p.linearDamping = 0.15f;

  m_bombs.InitPool(m_world);
  m_bombs.SetExplosionHandler([this](Vec2 center) {
    m_fields.SpawnBlackHole(center);
    for (auto& obs : m_obstacles) {
      auto& body = m_world.Get(obs.bodyId);
      if (!body.active) continue;
      const float dx = body.pos.x - center.x;
      const float dy = body.pos.y - center.y;
      if (std::sqrt(dx * dx + dy * dy) < 120.0f) {
        SpawnParticles(body.pos, 12, 255, 160, 40);
        body.active = false;
        m_score += 50;
        m_bombKillCount++;
      }
    }
    for (int id : m_fallingIds) {
      auto& crate = m_world.Get(id);
      if (!crate.active) continue;
      const float dx = crate.pos.x - center.x;
      const float dy = crate.pos.y - center.y;
      if (std::sqrt(dx * dx + dy * dy) < 120.0f) {
        SpawnParticles(crate.pos, 8, 150, 80, 220);
        crate.active = false;
        m_score += 30;
      }
    }
  });

  SpawnProps();

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
    b.restitution = 0.25f; b.linearDamping = 1.2f; b.groundFriction = 0.85f;
    addObs(id, ObstacleType::Normal);

  } else if (pattern == 1) {
    // 높은 상자 (폭탄)
    const int id = m_world.CreateCircle(36.0f, {x, groundY - 36.0f}, 1.8f, false);
    auto& b = m_world.Get(id);
    b.restitution = 0.1f; b.linearDamping = 1.5f; b.groundFriction = 0.9f;
    addObs(id, ObstacleType::Tall);

  } else if (pattern == 2) {
    // 낮은 상자 2개 연속
    for (int i = 0; i < 2; i++) {
      const int id = m_world.CreateCircle(18.0f,
          {x + static_cast<float>(i) * 50.0f, groundY - 18.0f}, 1.8f, false);
      auto& b = m_world.Get(id);
      b.restitution = 0.25f; b.linearDamping = 1.2f; b.groundFriction = 0.85f;
      addObs(id, ObstacleType::Normal);
    }

  } else if (pattern == 3) {
    // 낮은 + 높은 조합
    const int id1 = m_world.CreateCircle(18.0f, {x, groundY - 18.0f}, 1.8f, false);
    auto& c1 = m_world.Get(id1);
    c1.restitution = 0.25f; c1.linearDamping = 1.2f; c1.groundFriction = 0.85f;
    addObs(id1, ObstacleType::Normal);

    const int id2 = m_world.CreateCircle(36.0f, {x + 120.0f, groundY - 36.0f}, 1.8f, false);
    auto& c2 = m_world.Get(id2);
    c2.restitution = 0.1f; c2.linearDamping = 1.5f; c2.groundFriction = 0.9f;
    addObs(id2, ObstacleType::Tall);

  } else if (pattern == 4) {
    // 튀어오르는 장애물
    const int id = m_world.CreateCircle(20.0f, {x, groundY - 20.0f}, 1.5f, false);
    auto& b = m_world.Get(id);
    b.restitution = 0.9f; b.linearDamping = 0.3f; b.groundFriction = 0.1f;
    b.vel.y = -300.0f;
    addObs(id, ObstacleType::Bounce);

  } else if (pattern == 5) {
    // 삼각형 가시 (Triangle) — 중반부터
    const int id = m_world.CreateCircle(22.0f, {x, groundY - 22.0f}, 1.8f, false);
    auto& b = m_world.Get(id);
    b.restitution = 0.1f; b.linearDamping = 2.0f; b.groundFriction = 0.95f;
    addObs(id, ObstacleType::Triangle);

  } else if (pattern == 6) {
    // 천장 가로막이 (Ceiling) — 점프하면 맞음, 숙여야 함
    const int id = m_world.CreateCircle(20.0f, {x, 80.0f}, 1.8f, true); // 천장에 고정
    auto& b = m_world.Get(id);
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
    b.restitution = 0.2f; b.linearDamping = 0.0f; b.groundFriction = 0.0f;
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
    pt.vel.y  += 400.0f * dt;
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
  Item item;
  item.bodyId    = id;
  item.type      = type;
  item.collected = false;
  m_items.push_back(item);
}

void Game::UpdateItems() {
  const auto& p    = m_world.Get(m_playerId);
  const float camLeft = CameraX() - 200.0f;
  const float camX    = CameraX();

  for (auto& item : m_items) {
    if (item.collected) continue;
    auto& body = m_world.Get(item.bodyId);
    if (!body.active) continue;
    if (body.pos.x < camLeft) { body.active = false; item.collected = true; continue; }

    const float dx = p.pos.x - body.pos.x;
    const float dy = p.pos.y - body.pos.y;
    if (std::sqrt(dx * dx + dy * dy) < p.circle.radius + body.circle.radius + 8.0f) {
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
  const float camRight = CameraX() + static_cast<float>(m_w) + 400.0f;
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

void Game::CheckCollision() {
  const auto& p = m_world.Get(m_playerId);
  if (m_hitCooldown > 0.0f || m_shieldTimer > 0.0f) return;

  for (const auto& obs : m_obstacles) {
    const auto& body = m_world.Get(obs.bodyId);
    if (!body.active) continue;
    const float dx   = p.pos.x - body.pos.x;
    const float dy   = p.pos.y - body.pos.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < p.circle.radius + body.circle.radius) {
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

  SpawnProps();
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

void Game::HandleInput(float dt, Input& input) {
  const InputState& in = input.State();
  m_uiBlinkPhase += dt;
  UpdateStageTransition(dt);

  if (in.quit) { m_quit = true; return; }

  if (!m_started) {
    if (in.resumePressed) {
      m_started          = true;
      m_stageNotifyNum   = 1;
      m_stageNotifyTimer = kStageNotifyDuration;
    }
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
        m_rewindQueued   = true;
        m_rewindPoseLeft = kRewindPoseSeconds;
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
    if (inputDevice.ConsumeJumpPressForFixedStep() || input.jumpPressed) Restart();
    return;
  }

  m_rewindCooldownLeft = std::max(0.0f, m_rewindCooldownLeft - dt);
  m_hitCooldown        = std::max(0.0f, m_hitCooldown - dt);
  m_blinkTimer         = std::max(0.0f, m_blinkTimer - dt);
  m_shieldTimer        = std::max(0.0f, m_shieldTimer - dt);
  m_stageNotifyTimer   = std::max(0.0f, m_stageNotifyTimer - dt);
  m_hitEffectTimer     = std::max(0.0f, m_hitEffectTimer - dt);

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
        m_snapshotScratch.Apply(m_world, m_playerId, m_jumpBuffer, m_coyote,
                                m_stamina, m_bombs, m_fields, m_propIds);
        m_stamina = std::max(0.0f, staminaBeforeRewind - kRewindStaminaCost);
        m_rewindCooldownLeft = kRewindStaminaCost;
        m_rewindPoseLeft     = kRewindPoseSeconds;
        m_hp = std::min(1.0f, m_hp + 0.1f);
        m_particles.clear();
        m_popups.clear();
        return;
      }
    }
  }

  if (!rewindFrame && inputDevice.ConsumeJumpPressForFixedStep()) m_jumpBuffer = 0.12f;
  m_jumpBuffer      = std::max(0.0f, m_jumpBuffer - dt);
  m_jumpGroundGrace = std::max(0.0f, m_jumpGroundGrace - dt);

  m_snapshotScratch.Capture(m_world, m_playerId, m_jumpBuffer, m_coyote,
                            m_stamina, m_bombs, m_fields, m_propIds);
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
  m_bombs.FixedUpdate(dt, m_world, m_playerId);

  p.pos.x = laneXBeforeStep + m_scrollSpeed * dt;
  p.vel.x = m_scrollSpeed;

  if (m_jumpGroundGrace > 0.0f) p.onGround = false;
  else if (p.onGround) { m_coyote = 0.10f; m_runAnimPhase += dt; }
  else m_coyote = std::max(0.0f, m_coyote - dt);

  m_distance    += m_scrollSpeed * dt;
  m_elapsed     += dt;
  m_scrollSpeed  = 240.0f + m_elapsed * 1.5f;
  m_score        = static_cast<int>(m_distance / 10.0f) + m_bombKillCount * 50;

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

  UpdateSpawn();
  UpdateFallingObstacles();
  UpdateObstacles(dt);
  UpdateItems();
  UpdateParticles(dt);
  UpdatePopups(dt);
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

  const SDL_Color white{255, 255, 255, 255};
  const SDL_Color body {210, 210, 220, 255};
  const SDL_Color accent{120, 220, 255, 255};
  const int line = m_ui.LineHeight();
  int y = panel.y + 24;

  m_ui.DrawCentered(r, m_w / 2, y, "Chrono Rush", white);  y += line + 4;
  m_ui.DrawCentered(r, m_w / 2, y, "조작법", accent);       y += line + 10;

  const char* lines[] = {
      "C : 점프",
      "X 홀드 / 떼기 : 폭탄 충전·발사 (폭발=블랙홀)",
      "마우스 : 폭탄 조준 (점선 궤도)",
      "Z : 시간 역행 (3초, 스태미나 3)",
      "Esc : 일시정지",
  };
  for (const char* text : lines) { m_ui.Draw(r, panel.x + 28, y, text, body); y += line; }

  m_ui.DrawCenteredBlink(r, m_w / 2, panel.y + panel.h + 36,
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

  if (gameplayHud) {
    drawKey(16, 34, m_lastInput.jumpHeld,   m_lastInput.jumpPressed,   {90,  220, 255, 255});
    drawKey(42, 34, m_lastInput.throwHeld,  m_lastInput.throwPressed,  {255, 220, 80,  255});
    drawKey(68, 34, m_lastInput.rewindHeld, m_lastInput.rewindPressed, {180, 80,  255, 255});
  }

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

  if (gameplayHud) {
    // 스태미나 바
    const int barW = 240, barH = 12, x = 16, y = 16;
    SDL_Rect bg{x, y, barW, barH};
    SDL_SetRenderDrawColor(r, 40, 40, 48, 255);
    SDL_RenderFillRect(r, &bg);
    SDL_Rect fg{x, y, static_cast<int>((m_stamina / 3.0f) * barW), barH};
    SDL_SetRenderDrawColor(r, 110, 255, 140, 255);
    SDL_RenderFillRect(r, &fg);

    // 체력 바
    SDL_Rect hpBg{x, y + 16, barW, barH};
    SDL_SetRenderDrawColor(r, 40, 40, 48, 255);
    SDL_RenderFillRect(r, &hpBg);
    SDL_Rect hpFg{x, y + 16, static_cast<int>(m_hp * barW), barH};
    SDL_SetRenderDrawColor(r,
      static_cast<Uint8>(255 * (1.0f - m_hp)),
      static_cast<Uint8>(255 * m_hp), 60, 255);
    SDL_RenderFillRect(r, &hpFg);
    if (m_shieldTimer > 0.0f) {
      SDL_SetRenderDrawColor(r, 255, 220, 60, 255);
      SDL_RenderDrawRect(r, &hpBg);
    }

    // 점수
    m_ui.Draw(r, m_w / 2 - 60, 16,
              ("Score: " + std::to_string(m_score)).c_str(),
              SDL_Color{255, 220, 60, 255});

    // 거리 바 + 숫자
    SDL_Rect distBg{m_w - 260, 16, 244, 12};
    SDL_SetRenderDrawColor(r, 40, 40, 48, 255);
    SDL_RenderFillRect(r, &distBg);
    SDL_Rect distFg{m_w - 260, 16, static_cast<int>(m_distance / 10.0f) % 244, 12};
    SDL_SetRenderDrawColor(r, 255, 180, 60, 255);
    SDL_RenderFillRect(r, &distFg);
    m_ui.Draw(r, m_w - 260, 32,
              (std::to_string(static_cast<int>(m_distance)) + "m").c_str(),
              SDL_Color{255, 180, 60, 255});

    DrawStageNotify(r);
  }

  if (!m_started)       DrawTitleOverlay(r);
  else if (m_gameOver)  DrawGameOverOverlay(r);
  else if (m_paused)    DrawPauseOverlay(r);
}

} // namespace cr