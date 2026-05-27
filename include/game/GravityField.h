#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "math/Vec2.h"
#include "physics/PhysicsWorld.h"

struct SDL_Renderer;

namespace cr {

enum class FieldMode : std::uint8_t {
  Attract = 0,
  Repel,
};

struct GravityFieldTuning {
  static constexpr float minDistSq = 36.0f; // avoids singularity at center
  static constexpr float strengthK = 1'850'000.0f;
  static constexpr float maxForce = 26'000.0f;

  static constexpr float defaultRadius = 175.0f;
  static constexpr float defaultDuration = 0.95f;

  static constexpr float explosionRadius = 165.0f;
  static constexpr float explosionDuration = 0.85f;

  static constexpr float manualRadius = 185.0f;
  static constexpr float manualDuration = 1.15f;
  static constexpr float manualCooldown = 0.35f;

  static constexpr int maxFields = 6;
};

struct FieldSlot {
  bool active = false;
  FieldMode mode = FieldMode::Attract;
  Vec2 center{};
  float radius = GravityFieldTuning::defaultRadius;
  float timeLeft = 0.0f;
};

struct GravityFieldSnapshot {
  bool active = false;
  FieldMode mode = FieldMode::Attract;
  Vec2 center{};
  float radius = 0.0f;
  float timeLeft = 0.0f;

  static GravityFieldSnapshot Save(const FieldSlot& slot);
  void Load(FieldSlot& slot) const;
};

class GravityFieldSystem {
public:
  GravityFieldSystem();

  void Spawn(Vec2 center, FieldMode mode, float duration, float radius);
  void SpawnFromExplosion(Vec2 center);
  void TrySpawnManual(Vec2 worldPos, FieldMode mode);

  void FixedUpdate(float dt);
  void ApplyForces(PhysicsWorld& world,
                   int playerId,
                   const std::vector<int>& bombIds,
                   const std::vector<int>& propIds) const;

  void Render(SDL_Renderer* r, float cameraX) const;

  bool ToggleDebug();
  bool DebugEnabled() const { return m_debug; }

  void SaveSnapshots(std::array<GravityFieldSnapshot, GravityFieldTuning::maxFields>& out,
                     float& manualCooldown) const;
  void LoadSnapshots(const std::array<GravityFieldSnapshot, GravityFieldTuning::maxFields>& in,
                     float manualCooldown);

private:
  int AllocateSlot() const;
  Vec2 ComputeForce(Vec2 bodyPos, const FieldSlot& field) const;
  float EdgeFalloff(float dist, float radius) const;

  std::vector<FieldSlot> m_slots;
  float m_manualCooldown = 0.0f;
  bool m_debug = false;
};

} // namespace cr
