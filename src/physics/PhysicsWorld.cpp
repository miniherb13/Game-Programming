#include "physics/PhysicsWorld.h"

#include <algorithm>

namespace cr {

PhysicsWorld::PhysicsWorld(WorldBounds bounds) : m_bounds(bounds) {}

int PhysicsWorld::CreateCircle(float radius, Vec2 pos, float mass, bool isStatic) {
  Body b{};
  b.isStatic = isStatic;
  b.shapeType = ShapeType::Circle;
  b.circle.radius = radius;
  b.pos = pos;
  if (isStatic || mass <= 0.0f) {
    b.invMass = 0.0f;
  } else {
    b.invMass = 1.0f / mass;
  }

  m_bodies.push_back(b);
  return static_cast<int>(m_bodies.size() - 1);
}

void PhysicsWorld::Integrate(Body& b, float dt) {
  if (!b.active || b.invMass <= 0.0f) return;

  const Vec2 acc = m_gravity + (b.force * b.invMass);
  b.vel += acc * dt;

  if (b.linearDamping > 0.0f) {
    const float d = std::max(0.0f, 1.0f - b.linearDamping * dt);
    b.vel *= d;
  }

  b.pos += b.vel * dt;
}

void PhysicsWorld::SolveBounds(Body& b) {
  if (!b.active || b.invMass <= 0.0f) return;

  const float r = b.circle.radius;

  // ground
  if (b.pos.y + r > m_bounds.groundY) {
    b.pos.y = m_bounds.groundY - r;
    if (b.vel.y > 0.0f) b.vel.y = -b.vel.y * b.restitution;
  }

  // ceiling
  if (b.pos.y - r < m_bounds.ceilingY) {
    b.pos.y = m_bounds.ceilingY + r;
    if (b.vel.y < 0.0f) b.vel.y = -b.vel.y * b.restitution;
  }

  // walls
  if (b.pos.x - r < m_bounds.left) {
    b.pos.x = m_bounds.left + r;
    if (b.vel.x < 0.0f) b.vel.x = -b.vel.x * b.restitution;
  } else if (b.pos.x + r > m_bounds.right) {
    b.pos.x = m_bounds.right - r;
    if (b.vel.x > 0.0f) b.vel.x = -b.vel.x * b.restitution;
  }
}

void PhysicsWorld::Step(float dt) {
  for (auto& b : m_bodies) {
    Integrate(b, dt);
    SolveBounds(b);
    b.ClearForces();
  }
}

} // namespace cr
