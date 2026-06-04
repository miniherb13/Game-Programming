#include "game/GravityField.h"

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

void DrawCircleFilled(SDL_Renderer* r, Vec2 screenCenter, float radius) {
  const int cx = static_cast<int>(screenCenter.x);
  const int cy = static_cast<int>(screenCenter.y);
  const int ir = static_cast<int>(radius);
  for (int y = -ir; y <= ir; y++) {
    const float wy = static_cast<float>(y);
    const float halfW = std::sqrt(std::max(0.0f, radius * radius - wy * wy));
    const int x0 = cx - static_cast<int>(halfW);
    const int x1 = cx + static_cast<int>(halfW);
    SDL_RenderDrawLine(r, x0, cy + y, x1, cy + y);
  }
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

} // namespace

GravityFieldSnapshot GravityFieldSnapshot::Save(const FieldSlot& slot) {
  GravityFieldSnapshot out{};
  out.active = slot.active;
  out.mode = slot.mode;
  out.center = slot.center;
  out.radius = slot.radius;
  out.timeLeft = slot.timeLeft;
  return out;
}

void GravityFieldSnapshot::Load(FieldSlot& slot) const {
  slot.active = active;
  slot.mode = mode;
  slot.center = center;
  slot.radius = radius;
  slot.timeLeft = timeLeft;
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

void GravityFieldSystem::SpawnBlackHole(Vec2 center) {
  const int slot = AllocateSlot();
  auto& f = m_slots[static_cast<std::size_t>(slot)];
  f.active = true;
  f.mode = FieldMode::Attract;
  f.center = center;
  f.radius = GravityFieldTuning::explosionRadius;
  f.timeLeft = GravityFieldTuning::explosionDuration;
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

void GravityFieldSystem::ApplyForces(PhysicsWorld& world,
                                     int playerId,
                                     const std::vector<int>& bombIds,
                                     const std::vector<int>& propIds) const {
  const auto applyTo = [&](int id) {
    auto& body = world.Get(id);
    if (!body.active || body.invMass <= 0.0f) return;

    for (const auto& field : m_slots) {
      if (!field.active) continue;
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

bool GravityFieldSystem::ToggleDebug() {
  m_debug = !m_debug;
  return m_debug;
}

void GravityFieldSystem::Render(SDL_Renderer* r, float cameraX) const {
  for (const auto& field : m_slots) {
    if (!field.active) continue;

    const Vec2 screen{field.center.x - cameraX, field.center.y};
    const float lifeT =
        std::clamp(field.timeLeft / GravityFieldTuning::explosionDuration, 0.0f, 1.0f);
    const float pulse = 0.88f + 0.12f * std::sin((1.0f - lifeT) * 18.0f);

    const bool attract = field.mode == FieldMode::Attract;
    const SDL_Color ring = attract ? SDL_Color{150, 70, 255, 220} : SDL_Color{70, 210, 255, 220};
    const SDL_Color fill = attract ? SDL_Color{90, 40, 120, 45} : SDL_Color{40, 100, 140, 45};

    SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
    DrawCircleFilled(r, screen, field.radius * pulse);

    SDL_SetRenderDrawColor(r, ring.r, ring.g, ring.b, ring.a);
    DrawCircleOutline(r, screen, field.radius * pulse);

    // Radial hint lines
    constexpr int spokes = 8;
    for (int i = 0; i < spokes; i++) {
      const float a = static_cast<float>(i) / static_cast<float>(spokes) * 6.2831853f;
      const Vec2 dir{std::cos(a), std::sin(a)};
      const float inner = field.radius * 0.25f * pulse;
      const float outer = field.radius * (attract ? 0.55f : 0.82f) * pulse;
      const Vec2 p0 = screen + dir * inner;
      const Vec2 p1 = screen + dir * (attract ? outer : outer * 0.95f);
      SDL_RenderDrawLine(r,
                         static_cast<int>(p0.x),
                         static_cast<int>(p0.y),
                         static_cast<int>(p1.x),
                         static_cast<int>(p1.y));
    }

    if (m_debug) {
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
    }
  }
}

} // namespace cr
