#pragma once

#include "game/Bomb.h"
#include "game/GravityField.h"
#include "rewind/BodySnapshot.h"

#include <array>
#include <cstddef>
#include <vector>

namespace cr {

class BombSystem;
class GravityFieldSystem;
class PhysicsWorld;

// Rewind rule: restore player, bombs, fields, and props for that frame.
// Rewinding past an explosion removes the field and revives in-flight bombs.
struct GameSnapshot {
  static constexpr std::size_t kMaxBombs = 16;
  static constexpr std::size_t kMaxFields = static_cast<std::size_t>(GravityFieldTuning::maxFields);
  static constexpr std::size_t kMaxProps = 96;

  BodySnapshot player{};
  float jumpBuffer = 0.0f;
  float coyote = 0.0f;
  float stamina = 0.0f;

  std::array<BombSlotSnapshot, kMaxBombs> bombs{};
  std::array<GravityFieldSnapshot, kMaxFields> fields{};
  std::size_t propCount = 0;
  std::array<BodySnapshot, kMaxProps> props{};

  void Capture(const PhysicsWorld& world,
               int playerId,
               float jumpBuffer_,
               float coyote_,
               float stamina_,
               const BombSystem& bombs,
               const GravityFieldSystem& fields,
               const std::vector<int>& propIds);

  void Apply(PhysicsWorld& world,
             int playerId,
             float& jumpBuffer_,
             float& coyote_,
             float& stamina_,
             BombSystem& bombs,
             GravityFieldSystem& fields,
             const std::vector<int>& propIds) const;
};

} // namespace cr
