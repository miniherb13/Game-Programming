#include "game/Game.h"

#include "core/Log.h"
#include "math/Vec2.h"

#include <SDL.h>
#include <algorithm>
#include <string>

namespace cr {

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
  p.linearDamping = 0.8f;

  m_bombs.InitPool(m_world);
  m_bombs.SetExplosionHandler([this](Vec2 center) { m_fields.SpawnFromExplosion(center); });

  SpawnProps();

}

void Game::SpawnProps() {
  const float groundY = static_cast<float>(m_h - 40);
  constexpr int propCount = 14;
  m_propIds.reserve(propCount);

  for (int i = 0; i < propCount; i++) {
    const float x = 320.0f + static_cast<float>(i) * 110.0f;
    const int id = m_world.CreateCircle(18.0f, {x, groundY - 18.0f}, 1.8f, false);
    auto& crate = m_world.Get(id);
    crate.restitution = 0.25f;
    crate.linearDamping = 1.2f;
    crate.groundFriction = 0.85f;
    m_propIds.push_back(id);
  }
}

Vec2 Game::MouseWorldPos(const InputState& input) const {
  return {input.mousePos.x + CameraX(), input.mousePos.y};
}

void Game::FixedUpdate(float dt, const InputState& input) {
  if (input.quit) m_quit = true;

  m_lastInput = input;

  // Keep jump intent for longer (runner-friendly).
  if (input.jumpPressed) m_jumpBuffer = 0.50f;
  if (input.jumpHeld) m_jumpBuffer = std::max(m_jumpBuffer, 0.10f);
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

  if (input.rewindPressed && m_stamina >= 3.0f) {
    bool any = false;
    const std::size_t frames = m_rewind.Capacity();
    for (std::size_t i = 0; i < frames; i++) {
      if (!m_rewind.PopFrame(m_snapshotScratch)) break;
      any = true;
    }

    if (any) {
      m_snapshotScratch.Apply(m_world,
                              m_playerId,
                              m_jumpBuffer,
                              m_coyote,
                              m_stamina,
                              m_bombs,
                              m_fields,
                              m_propIds);
      m_stamina = std::max(0.0f, m_stamina - 3.0f);
    }
  }

  auto& p = m_world.Get(m_playerId);
  p.vel.x = m_scrollSpeed;

  m_bombs.TryThrow(input, m_world, p);

  if (input.debugPressed) m_fields.ToggleDebug();

  if (input.fieldPressed && !m_bombs.IsAiming(input)) {
    const FieldMode mode = input.shiftHeld ? FieldMode::Repel : FieldMode::Attract;
    m_fields.TrySpawnManual(MouseWorldPos(input), mode);
  }

  m_fields.ApplyForces(m_world, m_playerId, m_bombs.BodyIds(), m_propIds);

  m_world.Step(dt);
  m_fields.FixedUpdate(dt);
  m_bombs.FixedUpdate(dt, m_world, m_playerId);

  // coyote time (allow jump slightly after leaving ground)
  if (p.onGround) {
    m_coyote = 0.10f;
  } else {
    m_coyote = std::max(0.0f, m_coyote - dt);
  }

  // Apply jump AFTER physics so "landing frame" isn't delayed.
  if (m_jumpBuffer > 0.0f && (p.onGround || m_coyote > 0.0f)) {
    Log(LogLevel::Info,
        std::string("JUMP! onGround=") + (p.onGround ? "1" : "0") +
            " coyote=" + std::to_string(m_coyote) +
            " buffer=" + std::to_string(m_jumpBuffer) +
            " posY=" + std::to_string(p.pos.y) +
            " velY=" + std::to_string(p.vel.y));
    p.vel.y = -520.0f;
    p.pos.y -= 1.0f; // ensure we visually leave ground this frame
    p.onGround = false;
    m_jumpBuffer = 0.0f;
    m_coyote = 0.0f;
  }

  m_stamina = std::min(3.0f, m_stamina + dt * 0.15f);
}

void Game::Render(SDL_Renderer* r) const {
  const float camX = CameraX();

  // Input debug indicators (C / X / Z)
  // Bright = Held, dim = not held. Border flash = pressed this frame.
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

  drawKey(16, 34, m_lastInput.jumpHeld, m_lastInput.jumpPressed, SDL_Color{90, 220, 255, 255});   // C
  drawKey(42, 34, m_lastInput.throwHeld, m_lastInput.throwPressed, SDL_Color{255, 220, 80, 255}); // X
  drawKey(68, 34, m_lastInput.rewindHeld, m_lastInput.rewindPressed, SDL_Color{180, 80, 255, 255}); // Z
  drawKey(94, 34, m_lastInput.fieldHeld, m_lastInput.fieldPressed, SDL_Color{150, 90, 255, 255});   // V

  // Ground
  SDL_SetRenderDrawColor(r, 20, 20, 24, 255);
  SDL_Rect ground{0, m_h - 40, m_w, 40};
  SDL_RenderFillRect(r, &ground);

  // Player
  {
    const auto& p = m_world.Bodies().at(static_cast<std::size_t>(m_playerId));
    SDL_SetRenderDrawColor(r, 60, 200, 255, 255);
    SDL_Rect rc = RectFromCircle({p.pos.x - camX, p.pos.y}, p.circle.radius);
    SDL_RenderFillRect(r, &rc);
  }

  for (int id : m_propIds) {
    const auto& crate = m_world.Get(id);
    if (!crate.active) continue;
    SDL_SetRenderDrawColor(r, 150, 110, 80, 255);
    SDL_Rect rc = RectFromCircle({crate.pos.x - camX, crate.pos.y}, crate.circle.radius);
    SDL_RenderFillRect(r, &rc);
  }

  m_fields.Render(r, camX);
  m_bombs.Render(r, camX, m_world, m_world.Get(m_playerId), m_lastInput);

  if (m_fields.DebugEnabled()) {
    SDL_SetRenderDrawColor(r, 255, 255, 120, 255);
    SDL_Rect tag{120, 34, 56, 22};
    SDL_RenderDrawRect(r, &tag);
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
