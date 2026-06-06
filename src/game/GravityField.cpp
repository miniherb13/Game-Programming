#include "game/GravityField.h"

#include "render/VfxLibrary.h"

#include <SDL.h>
#include <algorithm>
#include <cmath>

namespace cr {

GravityFieldSnapshot GravityFieldSnapshot::Save(const FieldSlot& slot) {
  GravityFieldSnapshot out{};
  out.active = slot.active;
  out.mode = slot.mode;
  out.center = slot.center;
  out.radius = slot.radius;
  out.timeLeft = slot.timeLeft;
  out.spinAngle = slot.spinAngle;
  out.affectsPlayer = slot.affectsPlayer;
  out.affectsBombs = slot.affectsBombs;
  out.affectsProps = slot.affectsProps;
  return out;
}

void GravityFieldSnapshot::Load(FieldSlot& slot) const {
  slot.active = active;
  slot.mode = mode;
  slot.center = center;
  slot.radius = radius;
  slot.timeLeft = timeLeft;
  slot.spinAngle = spinAngle;
  slot.affectsPlayer = affectsPlayer;
  slot.affectsBombs = affectsBombs;
  slot.affectsProps = affectsProps;
}

void GravityFieldSystem::SaveSnapshots(
    std::array<GravityFieldSnapshot, GravityFieldTuning::maxFields>& out) const {
  for (std::size_t i = 0; i < m_slots.size(); i++) {
    out[i] = GravityFieldSnapshot::Save(m_slots[i]);
  }
}

void GravityFieldSystem::LoadSnapshots(
    const std::array<GravityFieldSnapshot, GravityFieldTuning::maxFields>& in) {
  for (std::size_t i = 0; i < m_slots.size(); i++) {
    in[i].Load(m_slots[i]);
  }
}

GravityFieldSystem::GravityFieldSystem()
    : m_slots(static_cast<std::size_t>(GravityFieldTuning::maxFields)) {}

void GravityFieldSystem::ClearAll() {
  for (auto& field : m_slots) {
    field.active = false;
    field.timeLeft = 0.0f;
  }
}

void GravityFieldSystem::SpawnBlackHole(Vec2 center) {
  const int slot = AllocateSlot();
  auto& f = m_slots[static_cast<std::size_t>(slot)];
  f.active = true;
  f.mode = FieldMode::Attract;
  f.center = center;
  f.radius = GravityFieldTuning::explosionRadius;
  f.timeLeft = GravityFieldTuning::explosionDuration;
  f.spinAngle = 0.0f;
  f.affectsPlayer = false;
  f.affectsBombs = false;
  f.affectsProps = false;
}

int GravityFieldSystem::AllocateSlot() const {
  for (std::size_t i = 0; i < m_slots.size(); i++) {
    if (!m_slots[i].active) return static_cast<int>(i);
  }

  int oldest = 0;
  float oldestTime = 1e9f;
  for (std::size_t i = 0; i < m_slots.size(); i++) {
    if (m_slots[i].timeLeft < oldestTime) {
      oldestTime = m_slots[i].timeLeft;
      oldest = static_cast<int>(i);
    }
  }
  return oldest;
}

float GravityFieldSystem::EdgeFalloff(float dist, float radius) const {
  const float inner = radius * 0.72f;
  if (dist <= inner) return 1.0f;
  const float t = std::clamp((dist - inner) / std::max(radius - inner, 1.0f), 0.0f, 1.0f);
  return 1.0f - t * t;
}

Vec2 GravityFieldSystem::ComputeForce(Vec2 bodyPos, const FieldSlot& field) const {
  const Vec2 delta = bodyPos - field.center;
  const float distSq = std::max(delta.LenSq(), GravityFieldTuning::minDistSq);
  const float dist = std::sqrt(distSq);
  if (dist > field.radius) return {};

  const Vec2 dir = delta / dist;
  float mag = GravityFieldTuning::strengthK / distSq;
  mag = std::min(mag, GravityFieldTuning::maxForce);
  mag *= EdgeFalloff(dist, field.radius);

  const float sign = (field.mode == FieldMode::Attract) ? -1.0f : 1.0f;
  return dir * (sign * mag);
}

void GravityFieldSystem::FixedUpdate(float dt) {
  for (auto& field : m_slots) {
    if (!field.active) continue;
    field.timeLeft -= dt;
    if (field.timeLeft <= 0.0f) field.active = false;
  }
}

void GravityFieldSystem::AdvanceSpin(float dt) {
  constexpr float kSpinRadPerSec = 5.8f;
  for (auto& field : m_slots) {
    if (!field.active) continue;
    field.spinAngle += dt * kSpinRadPerSec;
  }
}

void GravityFieldSystem::ApplyForces(PhysicsWorld& world,
                                     int playerId,
                                     const std::vector<int>& bombIds,
                                     const std::vector<int>& propIds) const {
  const auto applyTo = [&](int id) {
    auto& body = world.Get(id);
    if (!body.active || body.invMass <= 0.0f) return;

    for (const auto& field : m_slots) {
      if (!field.active) continue;
      if (id == playerId && !field.affectsPlayer) continue;
      if (body.kind == BodyKind::Bomb && !field.affectsBombs) continue;
      if (body.kind == BodyKind::Obstacle && !field.affectsProps) continue;
      Vec2 f = ComputeForce(body.pos, field);
      // Airborne player rising: ignore downward pull (bomb + jump overlap, black holes).
      if (id == playerId && body.vel.y < -60.0f && f.y > 0.0f) {
        f.y = 0.0f;
      } else if (id == playerId && !body.onGround && body.vel.y < 0.0f && f.y > 0.0f) {
        f.y *= 0.25f;
      }
      body.force += f;
    }
  };

  applyTo(playerId);
  for (int id : bombIds) applyTo(id);
  for (int id : propIds) applyTo(id);
}

void GravityFieldSystem::AppendActiveVisuals(std::vector<ActiveVisual>& out) const {
  for (const auto& field : m_slots) {
    if (!field.active) continue;
    ActiveVisual v{};
    v.center = field.center;
    v.radius = field.radius;
    v.lifeT = std::clamp(field.timeLeft / GravityFieldTuning::explosionDuration, 0.0f, 1.0f);
    v.spinAngle = field.spinAngle;
    out.push_back(v);
  }
}

bool GravityFieldSystem::ToggleDebug() {
  m_debug = !m_debug;
  return m_debug;
}

void GravityFieldSystem::Render(SDL_Renderer* r, float cameraX, float darkBackdropStrength) const {
  for (const auto& field : m_slots) {
    if (!field.active) continue;

    const Vec2 screen{field.center.x - cameraX, field.center.y};
    const float lifeT =
        std::clamp(field.timeLeft / GravityFieldTuning::explosionDuration, 0.0f, 1.0f);

    VfxLibrary::Instance().DrawBlackHole(r, screen.x, screen.y, field.radius, lifeT, field.spinAngle,
                                         darkBackdropStrength);

    if (m_debug) {
      SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(r, 255, 255, 120, 200);
      SDL_Rect centerDot{static_cast<int>(screen.x) - 3, static_cast<int>(screen.y) - 3, 6, 6};
      SDL_RenderFillRect(r, &centerDot);

      constexpr int samples = 6;
      for (int sy = 0; sy < samples; sy++) {
        for (int sx = 0; sx < samples; sx++) {
          const float fx = (static_cast<float>(sx) + 0.5f) / static_cast<float>(samples);
          const float fy = (static_cast<float>(sy) + 0.5f) / static_cast<float>(samples);
          const Vec2 sampleWorld{
              field.center.x + (fx * 2.0f - 1.0f) * field.radius * 0.85f,
              field.center.y + (fy * 2.0f - 1.0f) * field.radius * 0.85f};
          const Vec2 f = ComputeForce(sampleWorld, field);
          if (f.LenSq() < 1.0f) continue;

          const Vec2 sampleScreen{sampleWorld.x - cameraX, sampleWorld.y};
          const Vec2 arrow = Normalize(f) * 14.0f;
          SDL_RenderDrawLine(r,
                             static_cast<int>(sampleScreen.x),
                             static_cast<int>(sampleScreen.y),
                             static_cast<int>(sampleScreen.x + arrow.x),
                             static_cast<int>(sampleScreen.y + arrow.y));
        }
      }
      SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
  }
}

} // namespace cr
