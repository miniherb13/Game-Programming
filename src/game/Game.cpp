#include "game/Game.h"

#include "core/Log.h"
#include "math/Vec2.h"

#include <SDL.h>
#include <algorithm>

namespace cr {

static SDL_Rect RectFromCircle(const Vec2& p, float r) {
  SDL_Rect out{};
  out.x = static_cast<int>(p.x - r);
  out.y = static_cast<int>(p.y - r);
  out.w = static_cast<int>(r * 2.0f);
  out.h = static_cast<int>(r * 2.0f);
  return out;
}

Game::Game(int width, int height)
    : m_w(width),
      m_h(height),
      m_world(WorldBounds{0.0f, static_cast<float>(width), static_cast<float>(height - 40), 0.0f}),
      m_rewind(180) {
  m_playerId = m_world.CreateCircle(16.0f, {140.0f, static_cast<float>(height - 80)}, 1.0f, false);
  auto& p = m_world.Get(m_playerId);
  p.restitution = 0.0f;
  p.linearDamping = 0.8f;

  // player + up to 16 bombs (demo)
  m_rewind.SetObjectCount(1 + 16);
  m_frameStates.resize(1 + 16);
}

void Game::SpawnBomb(const InputState& input) {
  if (!input.throwPressed) return;
  if (!input.mouseDown) return;

  // Drag vector: down-right is positive in screen coords.
  const Vec2 drag = input.mouseDownPos - input.mousePos;
  const float strength = 6.5f;
  Vec2 v = strength * drag;

  const int id = m_world.CreateCircle(12.0f, m_world.Get(m_playerId).pos + Vec2{24.0f, -10.0f}, 0.7f, false);
  auto& b = m_world.Get(id);
  b.restitution = 0.65f;
  b.vel = v;

  m_bombIds.push_back(id);
  if (m_bombIds.size() > 16) {
    m_bombIds.erase(m_bombIds.begin());
  }

  // Arm: trigger a field shortly after spawn (demo stand-in for "explosion")
  m_fieldActive = true;
  m_fieldAttract = true;
  m_fieldTimeLeft = 0.65f;
  m_fieldCenter = b.pos;
}

void Game::ApplyGravityField() {
  if (!m_fieldActive) return;
  if (m_fieldTimeLeft <= 0.0f) {
    m_fieldActive = false;
    return;
  }

  const auto applyTo = [&](Body& body) {
    if (body.invMass <= 0.0f || !body.active) return;
    const Vec2 d = body.pos - m_fieldCenter;
    const float distSq = std::max(d.LenSq(), 36.0f);
    const float dist = std::sqrt(distSq);
    if (dist > m_fieldRadius) return;

    const Vec2 dir = d / dist;
    const float mag = m_fieldK / distSq;
    const Vec2 f = (m_fieldAttract ? -1.0f : 1.0f) * mag * dir;
    body.force += f;
  };

  // apply to player + bombs (demo)
  applyTo(m_world.Get(m_playerId));
  for (int id : m_bombIds) applyTo(m_world.Get(id));
}

void Game::FixedUpdate(float dt, const InputState& input) {
  if (input.quit) m_quit = true;

  // Save states first (for rewind)
  {
    auto& p = m_world.Get(m_playerId);
    m_frameStates[0] = {p.active, p.pos, p.vel};

    for (std::size_t i = 0; i < 16; i++) {
      if (i < m_bombIds.size()) {
        auto& b = m_world.Get(m_bombIds[i]);
        m_frameStates[1 + i] = {b.active, b.pos, b.vel};
      } else {
        m_frameStates[1 + i] = {false, {}, {}};
      }
    }
    m_rewind.PushFrame(m_frameStates);
  }

  // Rewind mode (toggle)
  if (input.rewindToggledOn && m_stamina > 0.0f) {
    std::vector<RewindState> s;
    if (m_rewind.PopFrame(s)) {
      auto& p = m_world.Get(m_playerId);
      p.active = s[0].active;
      p.pos = s[0].pos;
      p.vel = s[0].vel;

      for (std::size_t i = 0; i < 16 && i < m_bombIds.size(); i++) {
        auto& b = m_world.Get(m_bombIds[i]);
        b.active = s[1 + i].active;
        b.pos = s[1 + i].pos;
        b.vel = s[1 + i].vel;
      }
    }
    m_stamina = std::max(0.0f, m_stamina - dt);
    return; // skip physics while rewinding
  }

  // simple run forward
  m_world.Get(m_playerId).vel.x = 240.0f;
  if (input.jumpPressed) {
    auto& p = m_world.Get(m_playerId);
    // allow jump if near ground (demo heuristic)
    if (p.pos.y > (m_h - 40.0f - p.circle.radius - 2.0f)) {
      p.vel.y = -520.0f;
    }
  }

  SpawnBomb(input);
  ApplyGravityField();

  m_world.Step(dt);

  if (m_fieldActive) m_fieldTimeLeft = std::max(0.0f, m_fieldTimeLeft - dt);
  m_stamina = std::min(3.0f, m_stamina + dt * 0.15f);
}

void Game::Render(SDL_Renderer* r) const {
  // Ground
  SDL_SetRenderDrawColor(r, 20, 20, 24, 255);
  SDL_Rect ground{0, m_h - 40, m_w, 40};
  SDL_RenderFillRect(r, &ground);

  // Player
  {
    const auto& p = m_world.Bodies().at(static_cast<std::size_t>(m_playerId));
    SDL_SetRenderDrawColor(r, 60, 200, 255, 255);
    SDL_Rect rc = RectFromCircle(p.pos, p.circle.radius);
    SDL_RenderFillRect(r, &rc);
  }

  // Bombs
  SDL_SetRenderDrawColor(r, 255, 220, 80, 255);
  for (int id : m_bombIds) {
    const auto& b = m_world.Bodies().at(static_cast<std::size_t>(id));
    if (!b.active) continue;
    SDL_Rect rc = RectFromCircle(b.pos, b.circle.radius);
    SDL_RenderFillRect(r, &rc);
  }

  // Field visualization
  if (m_fieldActive) {
    SDL_SetRenderDrawColor(r, 180, 80, 255, 255);
    SDL_Rect rc = RectFromCircle(m_fieldCenter, m_fieldRadius);
    SDL_RenderDrawRect(r, &rc);
  }

  // Stamina bar (very simple)
  {
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
  }
}

} // namespace cr
