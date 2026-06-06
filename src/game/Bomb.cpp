#include "game/Bomb.h"

#include "rewind/BodySnapshot.h"

#include <SDL.h>
#include <algorithm>
#include <cmath>

namespace cr {

namespace {

SDL_Rect RectFromCircleScreen(Vec2 screenPos, float r) {
  SDL_Rect out{};
  out.x = static_cast<int>(screenPos.x - r);
  out.y = static_cast<int>(screenPos.y - r);
  out.w = static_cast<int>(r * 2.0f);
  out.h = static_cast<int>(r * 2.0f);
  return out;
}

void DrawCircleOutline(SDL_Renderer* r, Vec2 screenCenter, float radius) {
  constexpr int segments = 48;
  int px = static_cast<int>(screenCenter.x + radius);
  int py = static_cast<int>(screenCenter.y);
  for (int i = 1; i <= segments; i++) {
    const float t = static_cast<float>(i) / static_cast<float>(segments) * 6.2831853f;
    const int nx = static_cast<int>(screenCenter.x + std::cos(t) * radius);
    const int ny = static_cast<int>(screenCenter.y + std::sin(t) * radius);
    SDL_RenderDrawLine(r, px, py, nx, ny);
    px = nx;
    py = ny;
  }
}

void FillDisc(SDL_Renderer* r, int cx, int cy, int radius, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 alpha) {
  if (radius <= 0 || alpha == 0) return;
  SDL_SetRenderDrawColor(r, cr, cg, cb, alpha);
  for (int y = -radius; y <= radius; ++y) {
    const int halfW = static_cast<int>(std::sqrt(static_cast<float>(std::max(0, radius * radius - y * y))));
    SDL_RenderDrawLine(r, cx - halfW, cy + y, cx + halfW, cy + y);
  }
}

void DrawLensArc(SDL_Renderer* r, int cx, int cy, int rx, int ry, float arcStart, float arcEnd, Uint8 cr, Uint8 cg,
                 Uint8 cb, Uint8 alpha) {
  if (alpha == 0 || rx <= 0 || ry <= 0) return;
  SDL_SetRenderDrawColor(r, cr, cg, cb, alpha);
  int px = 0;
  int py = 0;
  bool hasPrev = false;
  constexpr int kSegs = 20;
  for (int i = 0; i <= kSegs; ++i) {
    const float u = static_cast<float>(i) / static_cast<float>(kSegs);
    const float a = arcStart + (arcEnd - arcStart) * u;
    const int nx = cx + static_cast<int>(std::cos(a) * static_cast<float>(rx));
    const int ny = cy + static_cast<int>(std::sin(a) * static_cast<float>(ry));
    if (hasPrev) SDL_RenderDrawLine(r, px, py, nx, ny);
    px = nx;
    py = ny;
    hasPrev = true;
  }
}

void DrawFlyingBomb(SDL_Renderer* r, Vec2 center, float radius, float fuseT) {
  const int cx = static_cast<int>(center.x);
  const int cy = static_cast<int>(center.y);
  const int ir = std::max(4, static_cast<int>(radius));
  const float spin = (1.0f - fuseT) * 6.2831853f;

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  FillDisc(r, cx, cy, ir + 3, 70, 28, 105, 40);
  FillDisc(r, cx, cy, ir + 1, 42, 16, 68, 85);
  FillDisc(r, cx, cy, ir, 38, 14, 62, 255);
  FillDisc(r, cx, cy, static_cast<int>(static_cast<float>(ir) * 0.90f), 95, 32, 98, 255);
  FillDisc(r, cx, cy, static_cast<int>(static_cast<float>(ir) * 0.74f), 145, 48, 125, 255);
  FillDisc(r, cx, cy, static_cast<int>(static_cast<float>(ir) * 0.58f), 205, 95, 155, 255);

  const int bandHalfH = std::max(2, ir / 4);
  const int bandHalfW = ir + 1;
  const int rSq = ir * ir;
  for (int dy = -bandHalfH; dy <= bandHalfH; ++dy) {
    for (int dx = -bandHalfW; dx <= bandHalfW; ++dx) {
      if (dx * dx + dy * dy > rSq) continue;
      const float ringU = std::abs(static_cast<float>(dy)) / static_cast<float>(bandHalfH + 1);
      Uint8 rr = 170;
      Uint8 gg = 55;
      Uint8 bb = 150;
      if (ringU < 0.40f) {
        rr = 255;
        gg = 242;
        bb = 215;
      } else if (ringU < 0.72f) {
        rr = 240;
        gg = 145;
        bb = 185;
      }
      SDL_SetRenderDrawColor(r, rr, gg, bb, 235);
      SDL_RenderDrawPoint(r, cx + dx, cy + dy);
    }
  }

  DrawLensArc(r, cx, cy - ir / 5, ir + 1, std::max(2, ir / 5), 3.14159f * 0.12f, 3.14159f * 0.88f, 120, 55,
              150, 150);
  DrawLensArc(r, cx, cy + ir / 5, ir + 1, std::max(2, ir / 5), 3.14159f * 1.12f, 3.14159f * 1.88f, 95, 45, 130,
              120);

  FillDisc(r, cx, cy, std::max(2, ir / 3), 4, 1, 10, 255);

  const int hx = cx + static_cast<int>(std::cos(spin) * static_cast<float>(ir) * 0.22f);
  const int hy = cy + static_cast<int>(std::sin(spin) * static_cast<float>(ir) * 0.14f) - 1;
  FillDisc(r, hx, hy, std::max(2, ir / 5), 255, 248, 225, 110);

  SDL_SetRenderDrawColor(r, 255, 228, 195, 190);
  DrawCircleOutline(r, center, static_cast<float>(ir) * 0.92f);
  SDL_SetRenderDrawColor(r, 150, 60, 140, 220);
  DrawCircleOutline(r, center, static_cast<float>(ir));
}

} // namespace

BombSlotSnapshot BombSlotSnapshot::Save(const BombSlot& slot, const Body& body) {
  BombSlotSnapshot out{};
  out.phase = slot.phase;
  out.fuseLeft = slot.fuseLeft;
  out.visualLeft = slot.visualLeft;
  out.explosionCenter = slot.explosionCenter;
  out.wasOnGround = slot.wasOnGround;
  out.prevVelY = slot.prevVelY;
  out.physics = BodySnapshot::Save(body);
  return out;
}

void BombSlotSnapshot::Load(BombSlot& slot, Body& body) const {
  slot.phase = phase;
  slot.fuseLeft = fuseLeft;
  slot.visualLeft = visualLeft;
  slot.explosionCenter = explosionCenter;
  slot.wasOnGround = wasOnGround;
  slot.prevVelY = prevVelY;
  physics.Load(body);
}

void BombSystem::SaveSnapshots(std::array<BombSlotSnapshot, 16>& out, const PhysicsWorld& world) const {
  for (std::size_t i = 0; i < m_slots.size(); i++) {
    out[i] = BombSlotSnapshot::Save(m_slots[i], world.Get(m_slots[i].bodyId));
  }
}

void BombSystem::LoadSnapshots(const std::array<BombSlotSnapshot, 16>& in, PhysicsWorld& world) {
  for (std::size_t i = 0; i < m_slots.size(); i++) {
    in[i].Load(m_slots[i], world.Get(m_slots[i].bodyId));
  }
}

BombSystem::BombSystem(int maxBombs) : m_slots(static_cast<std::size_t>(maxBombs)) {}

void BombSystem::SetExplosionHandler(std::function<void(Vec2 center)> handler) {
  m_onExplosion = std::move(handler);
}

void BombSystem::InitPool(PhysicsWorld& world) {
  m_bodyIds.clear();
  m_bodyIds.reserve(m_slots.size());

  for (auto& slot : m_slots) {
    const int id =
        world.CreateCircle(BombTuning::radius, {}, BombTuning::mass, false);
    auto& b = world.Get(id);
    b.active = false;
    b.kind = BodyKind::Bomb;
    b.restitution = BombTuning::restitution;
    b.linearDamping = BombTuning::linearDamping;
    b.groundFriction = BombTuning::groundFriction;

    slot.bodyId = id;
    slot.phase = BombPhase::Inactive;
    m_bodyIds.push_back(id);
  }
}

bool BombSystem::IsAiming(const InputState& input) const {
  return input.throwHeld;
}

Vec2 BombSystem::LaunchPosition(const Body& player) const {
  return player.pos + Vec2{BombTuning::spawnOffsetX, BombTuning::spawnOffsetY};
}

float BombSystem::Charge01() const {
  return std::clamp(m_chargeTime / BombTuning::maxChargeTime, 0.0f, 1.0f);
}

void BombSystem::CancelCharge() {
  m_charging = false;
  m_chargeTime = 0.0f;
}

Vec2 BombSystem::ComputeThrowVelocity(const Body& player, Vec2 mouseWorld, float charge01) const {
  const Vec2 launch = LaunchPosition(player);
  Vec2 aim = mouseWorld - launch;

  if (aim.x < BombTuning::minAimForwardX) aim.x = BombTuning::minAimForwardX;
  if (aim.LenSq() < 64.0f) aim = {80.0f, -50.0f};

  const float t = std::clamp(charge01, 0.0f, 1.0f);
  const float speed = BombTuning::minThrowSpeed + (BombTuning::maxThrowSpeed - BombTuning::minThrowSpeed) * t;
  return Normalize(aim) * speed;
}

void BombSystem::SampleTrajectory(Vec2 spawnPos,
                                  Vec2 velocity,
                                  float gravityY,
                                  float groundY,
                                  std::vector<Vec2>& out) const {
  out.clear();
  out.reserve(static_cast<std::size_t>(BombTuning::trajectoryMaxSteps));

  Vec2 pos = spawnPos;
  Vec2 vel = velocity;
  const float dt = BombTuning::trajectoryDt;
  const float r = BombTuning::radius;

  for (int i = 0; i < BombTuning::trajectoryMaxSteps; i++) {
    out.push_back(pos);

    vel.y += gravityY * dt;
    pos += vel * dt;

    if (pos.y + r >= groundY) {
      const Vec2 hit{pos.x, groundY - r};
      if (out.empty() || (hit - out.back()).LenSq() > 4.0f) out.push_back(hit);
      break;
    }
    if (pos.y < -80.0f) break;
  }
}

void BombSystem::DrawDashedTrajectory(SDL_Renderer* r,
                                      float cameraX,
                                      const std::vector<Vec2>& points) const {
  if (points.size() < 2) return;

  SDL_SetRenderDrawColor(r, 255, 255, 255, 210);

  float dashLeft = 0.0f;
  bool drawing = true;

  for (std::size_t i = 1; i < points.size(); i++) {
    const Vec2 a{points[i - 1].x - cameraX, points[i - 1].y};
    const Vec2 b{points[i].x - cameraX, points[i].y};
    const Vec2 seg = b - a;
    const float segLen = seg.Len();
    if (segLen < 0.5f) continue;

    const Vec2 dir = seg / segLen;
    float traveled = 0.0f;

    while (traveled < segLen) {
      const float chunk = drawing ? BombTuning::trajectoryDashLen : BombTuning::trajectoryGapLen;
      const float remain = segLen - traveled;
      const float step = std::min(chunk - dashLeft, remain);

      const Vec2 p0 = a + dir * traveled;
      const Vec2 p1 = a + dir * (traveled + step);

      if (drawing) {
        SDL_RenderDrawLine(r,
                           static_cast<int>(p0.x),
                           static_cast<int>(p0.y),
                           static_cast<int>(p1.x),
                           static_cast<int>(p1.y));
      }

      traveled += step;
      dashLeft += step;
      if (dashLeft >= chunk) {
        dashLeft = 0.0f;
        drawing = !drawing;
      }
    }
  }

  const Vec2 launchScreen{points.front().x - cameraX, points.front().y};
  SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
  SDL_Rect dot{static_cast<int>(launchScreen.x) - 3, static_cast<int>(launchScreen.y) - 3, 6, 6};
  SDL_RenderFillRect(r, &dot);
}

int BombSystem::AllocateSlot() {
  for (std::size_t i = 0; i < m_slots.size(); i++) {
    if (m_slots[i].phase == BombPhase::Inactive) return static_cast<int>(i);
  }

  int oldest = 0;
  float oldestFuse = 1e9f;
  for (std::size_t i = 0; i < m_slots.size(); i++) {
    if (m_slots[i].phase == BombPhase::Flying && m_slots[i].fuseLeft < oldestFuse) {
      oldestFuse = m_slots[i].fuseLeft;
      oldest = static_cast<int>(i);
    }
  }
  return oldest;
}

void BombSystem::ArmSlot(int slotIndex, PhysicsWorld& world, const Body& player, Vec2 velocity) {
  auto& slot = m_slots[static_cast<std::size_t>(slotIndex)];
  auto& b = world.Get(slot.bodyId);

  const Vec2 dir = velocity.LenSq() > 1.0f ? Normalize(velocity) : Vec2{1.0f, -0.35f};
  const Vec2 spawn = LaunchPosition(player) + dir * (BombTuning::radius + player.circle.radius + 2.0f);

  b.active = true;
  b.pos = spawn;
  b.vel = velocity;
  b.onGround = false;
  b.ClearForces();

  slot.phase = BombPhase::Flying;
  slot.fuseLeft = BombTuning::fuseSeconds;
  slot.visualLeft = 0.0f;
  slot.wasOnGround = false;
  slot.prevVelY = 0.0f;
}

void BombSystem::Fire(const Body& player, Vec2 mouseWorld, float charge01, PhysicsWorld& world) {
  const Vec2 velocity = ComputeThrowVelocity(player, mouseWorld, charge01);
  if (velocity.LenSq() < 1.0f) return;

  const int slot = AllocateSlot();
  if (slot < 0) return;

  if (m_slots[static_cast<std::size_t>(slot)].phase == BombPhase::Flying) {
    auto& old = world.Get(m_slots[static_cast<std::size_t>(slot)].bodyId);
    old.active = false;
  }

  ArmSlot(slot, world, player, velocity);
}

void BombSystem::UpdateThrow(float dt,
                             const InputState& input,
                             Vec2 mouseWorld,
                             PhysicsWorld& world,
                             const Body& player) {
  if (input.throwHeld) {
    m_charging = true;
    m_chargeTime = std::min(BombTuning::maxChargeTime, m_chargeTime + dt);
    return;
  }

  if (m_charging && input.throwReleased) {
    const float charge01 = std::max(Charge01(), BombTuning::minCharge01OnRelease);
    Fire(player, mouseWorld, charge01, world);
  }

  m_charging = false;
  m_chargeTime = 0.0f;
}

void BombSystem::ApplyExplosionImpulse(PhysicsWorld& world,
                                       int playerId,
                                       Vec2 center,
                                       float radius,
                                       float impulse) {
  (void)playerId;
  const float radiusSq = radius * radius;

  const auto applyTo = [&](int id) {
    auto& body = world.Get(id);
    if (!body.active || body.invMass <= 0.0f) return;
    if (body.kind == BodyKind::Bomb) return;

    const Vec2 delta = body.pos - center;
    const float distSq = delta.LenSq();
    if (distSq > radiusSq) return;

    const float dist = std::sqrt(std::max(distSq, 1.0f));
    const Vec2 dir = delta / dist;
    const float falloff = 1.0f - (dist / radius);
    body.vel += dir * (impulse * falloff);
  };

  // Do not apply physical knockback to the player.
  // (It reads as an unintended jump / jitter when throwing or exploding bombs.)
  for (int id : m_bodyIds) applyTo(id);
}

void BombSystem::Explode(int slotIndex, PhysicsWorld& world, int playerId) {
  auto& slot = m_slots[static_cast<std::size_t>(slotIndex)];
  auto& b = world.Get(slot.bodyId);
  if (!b.active) return;

  slot.explosionCenter = b.pos;
  slot.visualLeft = BombTuning::explosionVisualSeconds;
  slot.phase = BombPhase::Exploded;

  ApplyExplosionImpulse(world,
                        playerId,
                        slot.explosionCenter,
                        BombTuning::explosionRadius,
                        BombTuning::explosionImpulse);

  if (m_onExplosion) m_onExplosion(slot.explosionCenter);

  b.active = false;
  b.vel = {};
}

void BombSystem::FixedUpdate(float dt, PhysicsWorld& world, int playerId,
                             const std::vector<int>& obstacleBodyIds) {
  for (std::size_t i = 0; i < m_slots.size(); i++) {
    auto& slot = m_slots[i];

    if (slot.phase == BombPhase::Exploded) {
      slot.visualLeft = std::max(0.0f, slot.visualLeft - dt);
      if (slot.visualLeft <= 0.0f) slot.phase = BombPhase::Inactive;
      continue;
    }

    if (slot.phase != BombPhase::Flying) continue;

    auto& b = world.Get(slot.bodyId);
    if (!b.active) {
      slot.phase = BombPhase::Inactive;
      continue;
    }

    bool hitObstacle = false;
    for (int obstacleId : obstacleBodyIds) {
      const auto& obstacle = world.Get(obstacleId);
      if (!obstacle.active) continue;
      const float dx = b.pos.x - obstacle.pos.x;
      const float dy = b.pos.y - obstacle.pos.y;
      const float dist = std::sqrt(dx * dx + dy * dy);
      // Physics collision resolution may separate bodies before we get here,
      // so allow a small contact margin to still count as a hit.
      const float contact = b.circle.radius + obstacle.circle.radius + 8.0f;
      if (dist <= contact) {
        hitObstacle = true;
        break;
      }
    }

    slot.wasOnGround = b.onGround;
    slot.prevVelY = b.vel.y;

    if (hitObstacle) {
      Explode(static_cast<int>(i), world, playerId);
      continue;
    }

    slot.fuseLeft -= dt;
    if (slot.fuseLeft <= 0.0f) {
      Explode(static_cast<int>(i), world, playerId);
    }
  }
}

void BombSystem::Render(SDL_Renderer* r,
                        float cameraX,
                        float groundY,
                        const PhysicsWorld& world,
                        const Body& player,
                        Vec2 mouseWorld,
                        bool showAimGuide) const {
  if (showAimGuide) {
    const float charge01 = IsCharging() ? Charge01() : 0.0f;
    const Vec2 velocity = ComputeThrowVelocity(player, mouseWorld, charge01);
    if (velocity.LenSq() > 1.0f) {
      const Vec2 launch = LaunchPosition(player);
      std::vector<Vec2> arc;
      SampleTrajectory(launch, velocity, world.Gravity().y, groundY, arc);
      DrawDashedTrajectory(r, cameraX, arc);

      if (IsCharging()) {
        const Vec2 launchScreen{launch.x - cameraX, launch.y};
        const int barW = 56;
        const int fill = static_cast<int>(barW * charge01);
        SDL_Rect bg{static_cast<int>(launchScreen.x) - 8, static_cast<int>(launchScreen.y) - 28, barW, 8};
        SDL_SetRenderDrawColor(r, 60, 60, 70, 255);
        SDL_RenderFillRect(r, &bg);
        SDL_Rect fg{bg.x, bg.y, fill, bg.h};
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        SDL_RenderFillRect(r, &fg);
      }
    }
  }

  for (const auto& slot : m_slots) {
    if (slot.phase == BombPhase::Flying) {
      const auto& b = world.Bodies().at(static_cast<std::size_t>(slot.bodyId));
      if (!b.active) continue;

      const Vec2 bombScreen{b.pos.x - cameraX, b.pos.y};
      const float fuseT = slot.fuseLeft / BombTuning::fuseSeconds;
      DrawFlyingBomb(r, bombScreen, b.circle.radius, fuseT);

      SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
      const Uint8 fusePulse = static_cast<Uint8>(160.0f + (1.0f - fuseT) * 95.0f);
      SDL_SetRenderDrawColor(r, 255, 210, 120, fusePulse);
      const int indicator = static_cast<int>(5.0f + (1.0f - fuseT) * 7.0f);
      SDL_Rect pulse{static_cast<int>(bombScreen.x) - indicator / 2,
                     static_cast<int>(bombScreen.y - b.circle.radius) - 7,
                     indicator,
                     3};
      SDL_RenderFillRect(r, &pulse);
    }
  }
}

} // namespace cr
