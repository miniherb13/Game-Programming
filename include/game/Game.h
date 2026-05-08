#pragma once

#include "core/Input.h"
#include "physics/PhysicsWorld.h"
#include "rewind/RewindBuffer.h"

#include <vector>

struct SDL_Renderer;

namespace cr {

class Game {
public:
  Game(int width, int height);

  void FixedUpdate(float dt, const InputState& input);
  void Render(SDL_Renderer* r) const;

  bool WantsQuit() const { return m_quit; }

private:
  void SpawnBomb(const InputState& input);
  void ApplyGravityField();

  int m_w = 0;
  int m_h = 0;
  bool m_quit = false;

  PhysicsWorld m_world;

  int m_playerId = -1;
  std::vector<int> m_bombIds;

  bool m_fieldActive = false;
  bool m_fieldAttract = true;
  float m_fieldTimeLeft = 0.0f;
  Vec2 m_fieldCenter{};
  float m_fieldRadius = 180.0f;
  float m_fieldK = 2200000.0f;

  RewindBuffer m_rewind;
  std::vector<RewindState> m_frameStates;
  float m_stamina = 3.0f; // seconds of rewind budget
};

} // namespace cr
