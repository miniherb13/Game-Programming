#include "rewind/BodySnapshot.h"

namespace cr {

BodySnapshot BodySnapshot::Save(const Body& body) {
  return {body.active, body.pos, body.vel, body.onGround};
}

void BodySnapshot::Load(Body& body) const {
  body.active = active;
  body.pos = pos;
  body.vel = vel;
  body.onGround = onGround;
  body.ClearForces();
}

} // namespace cr
