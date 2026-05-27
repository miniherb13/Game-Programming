#pragma once

#include "math/Vec2.h"
#include "physics/Body.h"

namespace cr {

struct BodySnapshot {
  bool active = true;
  Vec2 pos{};
  Vec2 vel{};
  bool onGround = false;

  static BodySnapshot Save(const Body& body);
  void Load(Body& body) const;
};

} // namespace cr
