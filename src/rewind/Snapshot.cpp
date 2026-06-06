#include "rewind/Snapshot.h"

#include "game/Bomb.h"
#include "game/GravityField.h"
#include "physics/PhysicsWorld.h"

#include <algorithm>

namespace cr {

void GameSnapshot::Capture(const PhysicsWorld& world,
                           int playerId,
                           float jumpBuffer_,
                           float coyote_,
                           float stamina_,
                           float hp_,
                           const BombSystem& bombSystem,
                           const GravityFieldSystem& fieldSystem,
                           const std::vector<int>& propIds) {
  player = BodySnapshot::Save(world.Get(playerId));
  jumpBuffer = jumpBuffer_;
  coyote = coyote_;
  stamina = stamina_;
  hp = hp_;

  bombSystem.SaveSnapshots(bombs, world);
  fieldSystem.SaveSnapshots(fields);

  propCount = std::min(propIds.size(), kMaxProps);
  for (std::size_t i = 0; i < propCount; i++) {
    props[i] = BodySnapshot::Save(world.Get(propIds[i]));
  }
}

void GameSnapshot::Apply(PhysicsWorld& world,
                         int playerId,
                         float& jumpBuffer_,
                         float& coyote_,
                         float& stamina_,
                         float& hp_,
                         BombSystem& bombSystem,
                         GravityFieldSystem& fieldSystem,
                         const std::vector<int>& propIds) const {
  player.Load(world.Get(playerId));
  jumpBuffer_ = jumpBuffer;
  coyote_ = coyote;
  stamina_ = stamina;
  hp_ = hp;

  bombSystem.LoadSnapshots(bombs, world);
  fieldSystem.LoadSnapshots(fields);

  const std::size_t n = std::min(propIds.size(), propCount);
  for (std::size_t i = 0; i < n; i++) {
    props[i].Load(world.Get(propIds[i]));
  }
  for (std::size_t i = n; i < propIds.size(); i++) {
    world.Get(propIds[i]).active = false;
  }
}

} // namespace cr
