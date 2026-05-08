#pragma once

#include "math/Vec2.h"
#include "physics/Body.h"

#include <vector>

namespace cr {

struct WorldBounds {
  float left = 0.0f;
  float right = 1280.0f;
  float groundY = 680.0f;
  float ceilingY = 0.0f;
};

class PhysicsWorld {
public:
  explicit PhysicsWorld(WorldBounds bounds);

  int CreateCircle(float radius, Vec2 pos, float mass, bool isStatic);
  Body& Get(int id) { return m_bodies.at(static_cast<std::size_t>(id)); }
  const std::vector<Body>& Bodies() const { return m_bodies; }

  void Step(float dt);
  void SetGravity(Vec2 g) { m_gravity = g; }
  Vec2 Gravity() const { return m_gravity; }

private:
  void Integrate(Body& b, float dt);
  void SolveBounds(Body& b);

  WorldBounds m_bounds{};
  Vec2 m_gravity{0.0f, 1400.0f}; // screen-space: +y is down
  std::vector<Body> m_bodies{};
};

} // namespace cr
