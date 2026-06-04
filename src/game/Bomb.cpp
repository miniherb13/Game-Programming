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

Vec2 BombSystem::ComputeThrowVelocity(const Body& player, Vec2 mouseWorld) const {
  const Vec2 launch = LaunchPosition(player);
  Vec2 aim = mouseWorld - launch;

  if (aim.x < BombTuning::minAimForwardX) aim.x = BombTuning::minAimForwardX;

  const float dist = aim.Len();
  if (dist < BombTuning::minAimDistance) {
    return Normalize(aim) * BombTuning::minThrowSpeed;
  }

  const float speed = std::clamp(dist * BombTuning::powerPerDistance,
                                 BombTuning::minThrowSpeed,
                                 BombTuning::maxThrowSpeed);
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

  SDL_SetRenderDrawColor(r, 255, 220, 90, 210);

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
  SDL_SetRenderDrawColor(r, 255, 200, 60, 255);
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

void BombSystem::TryThrow(const InputState& input, Vec2 mouseWorld, PhysicsWorld& world, const Body& player) {
  if (!input.throwPressed) return;

  const Vec2 velocity = ComputeThrowVelocity(player, mouseWorld);
  if (velocity.LenSq() < 1.0f) return;

  const int slot = AllocateSlot();
  if (slot < 0) return;

  if (m_slots[static_cast<std::size_t>(slot)].phase == BombPhase::Flying) {
    auto& old = world.Get(m_slots[static_cast<std::size_t>(slot)].bodyId);
    old.active = false;
  }

  ArmSlot(slot, world, player, velocity);
}

void BombSystem::ApplyExplosionImpulse(PhysicsWorld& world,
                                       int playerId,
                                       Vec2 center,
                                       float radius,
                                       float impulse) {
  const float radiusSq = radius * radius;

  const auto applyTo = [&](int id) {
    auto& body = world.Get(id);
    if (!body.active || body.invMass <= 0.0f) return;

    const Vec2 delta = body.pos - center;
    const float distSq = delta.LenSq();
    if (distSq > radiusSq) return;

    const float dist = std::sqrt(std::max(distSq, 1.0f));
    const Vec2 dir = delta / dist;
    const float falloff = 1.0f - (dist / radius);
    body.vel += dir * (impulse * falloff);
  };

  applyTo(playerId);
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

void BombSystem::FixedUpdate(float dt, PhysicsWorld& world, int playerId) {
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

    slot.fuseLeft -= dt;

    const bool fuseExpired = slot.fuseLeft <= 0.0f;
    const bool hardImpact =
        !slot.wasOnGround && b.onGround && slot.prevVelY >= BombTuning::impactExplodeSpeed;

    slot.wasOnGround = b.onGround;
    slot.prevVelY = b.vel.y;

    if (fuseExpired || hardImpact) {
      Explode(static_cast<int>(i), world, playerId);
    }
  }
}

void BombSystem::Render(SDL_Renderer* r,
                        float cameraX,
                        float groundY,
                        const PhysicsWorld& world,
                        const Body& player,
                        Vec2 mouseWorld) const {
  const Vec2 velocity = ComputeThrowVelocity(player, mouseWorld);
  if (velocity.LenSq() > 1.0f) {
    const Vec2 launch = LaunchPosition(player);
    std::vector<Vec2> arc;
    SampleTrajectory(launch, velocity, world.Gravity().y, groundY, arc);
    DrawDashedTrajectory(r, cameraX, arc);

    const Vec2 mouseScreen{mouseWorld.x - cameraX, mouseWorld.y};
    SDL_SetRenderDrawColor(r, 255, 255, 255, 140);
    SDL_RenderDrawLine(r,
                       static_cast<int>(launch.x - cameraX),
                       static_cast<int>(launch.y),
                       static_cast<int>(mouseScreen.x),
                       static_cast<int>(mouseScreen.y));
  }

  for (const auto& slot : m_slots) {
    if (slot.phase == BombPhase::Flying) {
      const auto& b = world.Bodies().at(static_cast<std::size_t>(slot.bodyId));
      if (!b.active) continue;

      SDL_SetRenderDrawColor(r, 255, 220, 80, 255);
      SDL_Rect rc = RectFromCircleScreen({b.pos.x - cameraX, b.pos.y}, b.circle.radius);
      SDL_RenderFillRect(r, &rc);

      const float fuseT = slot.fuseLeft / BombTuning::fuseSeconds;
      SDL_SetRenderDrawColor(r, 255, 120, 60, 255);
      const int indicator = static_cast<int>(6.0f + (1.0f - fuseT) * 8.0f);
      SDL_Rect pulse{rc.x + rc.w / 2 - indicator / 2, rc.y - 6, indicator, 4};
      SDL_RenderFillRect(r, &pulse);
      continue;
    }

    if (slot.phase == BombPhase::Exploded && slot.visualLeft > 0.0f) {
      const float t = 1.0f - (slot.visualLeft / BombTuning::explosionVisualSeconds);
      const float radius = BombTuning::explosionRadius * (0.35f + 0.65f * t);
      SDL_SetRenderDrawColor(r, 255, 140, 50, static_cast<Uint8>(220 * (1.0f - t)));
      SDL_Rect rc = RectFromCircleScreen({slot.explosionCenter.x - cameraX, slot.explosionCenter.y}, radius);
      SDL_RenderDrawRect(r, &rc);
      SDL_RenderDrawRect(r, &rc);
    }
  }
}

} // namespace cr
