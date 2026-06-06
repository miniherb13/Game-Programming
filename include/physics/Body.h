#pragma once

#include "math/Vec2.h"

#include <cstdint>

namespace cr {

enum class ShapeType : std::uint8_t {
  Circle = 0,
};

enum class BodyKind : std::uint8_t {
  Default = 0,
  Player,
  Bomb,
  Obstacle,
  Item,
};

enum class MotionType : std::uint8_t {
  Dynamic,
  Kinematic,
  Static,
};

struct CircleShape {
  float radius = 12.0f;
};

struct Body {
  bool active = true;
  bool isStatic = false;
  bool onGround = false;

  BodyKind kind = BodyKind::Default;
  MotionType motion = MotionType::Dynamic;
  bool lockVelX = false;
  bool ignoreObstacleContact = false;

  ShapeType shapeType = ShapeType::Circle;
  CircleShape circle{};

  Vec2 pos{};
  Vec2 vel{};
  Vec2 force{};

  float invMass = 1.0f;
  float restitution = 0.4f;
  float linearDamping = 0.0f;
  float groundFriction = 0.0f; // 0..1, horizontal speed loss on ground contact

  int rewindId = -1;

  void ClearForces() { force = {0.0f, 0.0f}; }

  bool ReceivesCollisionImpulse() const { return motion == MotionType::Dynamic; }
  float SeparationWeight() const {
    if (!active || motion != MotionType::Dynamic) return 0.0f;
    return invMass;
  }
};

} // namespace cr
