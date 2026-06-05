#include "physics/PhysicsWorld.h"

#include "math/Vec2.h"

#include <algorithm>
#include <cmath>

namespace cr {

namespace {

constexpr float kCollisionSlop = 0.5f;
constexpr float kCollisionPercent = 0.6f;
constexpr int kCollisionIterations = 3;

bool CirclesOverlap(const Body& a, const Body& b, Vec2& outNormal, float& outPenetration) {
  if (!a.active || !b.active) return false;
  if (a.shapeType != ShapeType::Circle || b.shapeType != ShapeType::Circle) return false;

  const Vec2 delta = b.pos - a.pos;
  const float distSq = delta.LenSq();
  const float minDist = a.circle.radius + b.circle.radius;
  if (distSq >= minDist * minDist) return false;

  float dist = std::sqrt(distSq);
  if (dist < 1e-5f) {
    outNormal = {1.0f, 0.0f};
    dist = 1e-5f;
  } else {
    outNormal = delta / dist;
  }

  outPenetration = minDist - dist;
  return outPenetration > 0.0f;
}

} // namespace

PhysicsWorld::PhysicsWorld(WorldBounds bounds) : m_bounds(bounds) {}

int PhysicsWorld::CreateCircle(float radius, Vec2 pos, float mass, bool isStatic) {
  return CreateCircle(radius, pos, mass, isStatic ? MotionType::Static : MotionType::Dynamic);
}

int PhysicsWorld::CreateCircle(float radius, Vec2 pos, float mass, MotionType motion) {
  Body b{};
  b.motion = motion;
  b.isStatic = motion == MotionType::Static;
  b.shapeType = ShapeType::Circle;
  b.circle.radius = radius;
  b.pos = pos;

  if (motion == MotionType::Static || mass <= 0.0f) {
    b.invMass = 0.0f;
  } else {
    b.invMass = 1.0f / mass;
  }

  m_bodies.push_back(b);
  return static_cast<int>(m_bodies.size() - 1);
}

void PhysicsWorld::Integrate(Body& b, float dt) {
  if (!b.active || b.motion == MotionType::Static) return;

  const Vec2 acc = m_gravity + (b.force * b.invMass);
  b.vel += acc * dt;

  if (b.linearDamping > 0.0f) {
    const float d = std::max(0.0f, 1.0f - b.linearDamping * dt);
    if (b.lockVelX) {
      b.vel.y *= d;
    } else {
      b.vel *= d;
    }
  }

  b.pos += b.vel * dt;
}

void PhysicsWorld::SolveBounds(Body& b) {
  if (!b.active || b.motion == MotionType::Static) return;

  const float r = b.circle.radius;
  constexpr float eps = 0.01f;

  if (b.pos.y + r >= m_bounds.groundY - eps) {
    b.pos.y = m_bounds.groundY - r;
    if (b.vel.y > 0.0f) b.vel.y = -b.vel.y * b.restitution;
    if (b.groundFriction > 0.0f) {
      const float keep = std::clamp(1.0f - b.groundFriction, 0.0f, 1.0f);
      b.vel.x *= keep;
    }
    b.onGround = true;
  }

  if (b.pos.y - r < m_bounds.ceilingY) {
    b.pos.y = m_bounds.ceilingY + r;
    if (b.vel.y < 0.0f) b.vel.y = -b.vel.y * b.restitution;
  }
}

void PhysicsWorld::ResolveCirclePair(Body& a, Body& b) {
  if (!a.active || !b.active) return;

  // Ignore player<->bomb collisions (prevents excessive bounce / self-hit physics).
  if ((a.kind == BodyKind::Player && b.kind == BodyKind::Bomb) ||
      (a.kind == BodyKind::Bomb && b.kind == BodyKind::Player)) {
    return;
  }

  // Bombs should only interact with obstacles (and world bounds).
  if ((a.kind == BodyKind::Bomb && b.kind != BodyKind::Obstacle) ||
      (b.kind == BodyKind::Bomb && a.kind != BodyKind::Obstacle)) {
    return;
  }

  const bool bombObstaclePair =
      (a.kind == BodyKind::Bomb && b.kind == BodyKind::Obstacle) ||
      (a.kind == BodyKind::Obstacle && b.kind == BodyKind::Bomb);
  if (bombObstaclePair) {
    Body& bomb = a.kind == BodyKind::Bomb ? a : b;
    Body& obs = a.kind == BodyKind::Obstacle ? a : b;

    Vec2 delta = bomb.pos - obs.pos;
    const float minDist = bomb.circle.radius + obs.circle.radius;
    const float distSq = delta.LenSq();
    if (distSq >= minDist * minDist) return;

    float dist = std::sqrt(distSq);
    Vec2 n{1.0f, 0.0f};
    if (dist > 1e-5f) n = delta / dist;

    const float correction =
        std::max(minDist - dist - kCollisionSlop, 0.0f) * kCollisionPercent;
    if (correction <= 0.0f) return;

    bomb.pos += n * correction;
    return;
  }

  const bool playerObstaclePair =
      (a.kind == BodyKind::Player && b.kind == BodyKind::Obstacle) ||
      (a.kind == BodyKind::Obstacle && b.kind == BodyKind::Player);
  if (playerObstaclePair) {
    Body& player = a.kind == BodyKind::Player ? a : b;
    Body& obs = a.kind == BodyKind::Obstacle ? a : b;

    Vec2 delta = player.pos - obs.pos;
    const float minDist = player.circle.radius + obs.circle.radius;
    const float distSq = delta.LenSq();
    if (distSq >= minDist * minDist) return;

    float dist = std::sqrt(distSq);
    Vec2 n{1.0f, 0.0f};
    if (dist > 1e-5f) n = delta / dist;

    const float correction =
        std::max(minDist - dist - kCollisionSlop, 0.0f) * kCollisionPercent;
    if (correction <= 0.0f) return;

    // Only adjust Y. Player X is driven by map scroll; pushing X cancels movement.
    if (n.y < 0.0f || player.pos.y < obs.pos.y - obs.circle.radius * 0.15f) {
      player.pos.y += correction * 0.85f;
    } else {
      player.pos.y += correction * std::max(n.y, 0.2f);
    }
    return;
  }

  Vec2 normal{};
  float penetration = 0.0f;
  if (!CirclesOverlap(a, b, normal, penetration)) return;

  const bool aKin = a.motion == MotionType::Kinematic;
  const bool bKin = b.motion == MotionType::Kinematic;
  const bool aDyn = a.motion == MotionType::Dynamic;
  const bool bDyn = b.motion == MotionType::Dynamic;

  const float correction =
      std::max(penetration - kCollisionSlop, 0.0f) * kCollisionPercent;
  if (correction > 0.0f) {
    if (aKin && bDyn) {
      b.pos += normal * correction;
    } else if (bKin && aDyn) {
      a.pos -= normal * correction;
    } else {
      const float invA = a.SeparationWeight();
      const float invB = b.SeparationWeight();
      const float invSum = invA + invB;
      if (invSum > 0.0f) {
        const float sep = correction / invSum;
        a.pos -= normal * (sep * invA);
        b.pos += normal * (sep * invB);
      }
    }
  }

  if ((aKin && bDyn) || (bKin && aDyn)) return;

  Vec2 relVel = b.vel - a.vel;
  const float velAlongNormal = Dot(relVel, normal);
  if (velAlongNormal > 0.0f) return;

  float invMassSum = 0.0f;
  if (a.ReceivesCollisionImpulse()) invMassSum += a.invMass;
  if (b.ReceivesCollisionImpulse()) invMassSum += b.invMass;
  if (invMassSum <= 0.0f) return;

  const float e = std::min(a.restitution, b.restitution);
  const float impulseMag = -(1.0f + e) * velAlongNormal / invMassSum;
  const Vec2 impulse = normal * impulseMag;

  if (a.ReceivesCollisionImpulse()) a.vel -= impulse * a.invMass;
  if (b.ReceivesCollisionImpulse()) b.vel += impulse * b.invMass;
}

void PhysicsWorld::SolveCircleCollisions() {
  const std::size_t count = m_bodies.size();
  for (int iter = 0; iter < kCollisionIterations; ++iter) {
    for (std::size_t i = 0; i < count; ++i) {
      for (std::size_t j = i + 1; j < count; ++j) {
        ResolveCirclePair(m_bodies[i], m_bodies[j]);
      }
    }
  }
}

void PhysicsWorld::Step(float dt) {
  for (auto& b : m_bodies) {
    b.onGround = false;
    Integrate(b, dt);
    SolveBounds(b);
  }

  SolveCircleCollisions();

  for (auto& b : m_bodies) {
    if (b.motion != MotionType::Static) SolveBounds(b);
  }

  for (auto& b : m_bodies) {
    b.ClearForces();
  }
}

} // namespace cr
