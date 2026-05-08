#pragma once

#include "math/Vec2.h"

#include <cstdint>

namespace cr {

enum class ShapeType : std::uint8_t {
  Circle = 0,
};

struct CircleShape {
  float radius = 12.0f;
};

struct Body {
  bool active = true;
  bool isStatic = false;
  bool onGround = false;

  ShapeType shapeType = ShapeType::Circle;
  CircleShape circle{};

  Vec2 pos{};
  Vec2 vel{};
  Vec2 force{};

  float invMass = 1.0f;
  float restitution = 0.4f;
  float linearDamping = 0.0f;

  int rewindId = -1;

  void ClearForces() { force = {0.0f, 0.0f}; }
};

} // namespace cr
