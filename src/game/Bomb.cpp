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

Vec2 ThrowVelocityFromDrag(Vec2 dragScreen) {
  const float dragLen = dragScreen.Len();
  if (dragLen < BombTuning::minDragPixels) return {};

  const float speed =
      std::clamp(dragLen * BombTuning::throwStrength, BombTuning::minThrowSpeed, BombTuning::maxThrowSpeed);
  return Normalize(dragScreen) * speed;
}

bool ShouldThrowNow(const InputState& input) {
  const Vec2 drag = input.mouseDownPos - input.mousePos;
  if (drag.LenSq() < BombTuning::minDragPixels * BombTuning::minDragPixels) return false;

  if (input.throwPressed) return true;
  return input.throwHeld && input.mouseReleased;
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
  return input.throwHeld && input.mouseDown;
}

Vec2 BombSystem::PreviewVelocity(const InputState& input) const {
  if (!IsAiming(input)) return {};
  return ThrowVelocityFromDrag(input.mouseDownPos - input.mousePos);
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
  const Vec2 spawn = player.pos + Vec2{BombTuning::spawnOffsetX, BombTuning::spawnOffsetY} +
                   dir * (BombTuning::radius + player.circle.radius + 2.0f);

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

void BombSystem::TryThrow(const InputState& input, PhysicsWorld& world, const Body& player) {
  if (!ShouldThrowNow(input)) return;

  const Vec2 velocity = ThrowVelocityFromDrag(input.mouseDownPos - input.mousePos);
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
                        const PhysicsWorld& world,
                        const Body& player,
                        const InputState& input) const {
  const Vec2 playerScreen{player.pos.x - cameraX, player.pos.y};

  if (IsAiming(input)) {
    const Vec2 vel = PreviewVelocity(input);
    if (vel.LenSq() > 1.0f) {
      const Vec2 end = playerScreen + Normalize(vel) * 72.0f;
      SDL_SetRenderDrawColor(r, 255, 200, 60, 200);
      SDL_RenderDrawLine(r,
                         static_cast<int>(playerScreen.x),
                         static_cast<int>(playerScreen.y),
                         static_cast<int>(end.x),
                         static_cast<int>(end.y));
    }

    SDL_SetRenderDrawColor(r, 255, 220, 80, 120);
    SDL_RenderDrawLine(r,
                       static_cast<int>(input.mouseDownPos.x),
                       static_cast<int>(input.mouseDownPos.y),
                       static_cast<int>(input.mousePos.x),
                       static_cast<int>(input.mousePos.y));
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
