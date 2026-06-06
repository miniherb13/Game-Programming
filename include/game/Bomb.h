#pragma once

#include <cstdint>

#include "core/Input.h"
#include "math/Vec2.h"
#include "physics/PhysicsWorld.h"

#include "rewind/BodySnapshot.h"

#include <array>
#include <functional>
#include <vector>

struct SDL_Renderer;

namespace cr {

enum class BombPhase : std::uint8_t {
  Inactive = 0,
  Flying,
  Exploded,
};

struct BombTuning {
  static constexpr float maxChargeTime = 1.2f;
  static constexpr float minChargeToFire = 0.0f;
  static constexpr float minCharge01OnRelease = 0.18f;
  static constexpr float minThrowSpeed = 280.0f;
  static constexpr float maxThrowSpeed = 1350.0f;
  static constexpr float minAimForwardX = 24.0f;

  static constexpr float trajectoryDt = 1.0f / 60.0f;
  static constexpr int trajectoryMaxSteps = 200;
  static constexpr float trajectoryDashLen = 10.0f;
  static constexpr float trajectoryGapLen = 8.0f;

  static constexpr float spawnOffsetX = 26.0f;
  static constexpr float spawnOffsetY = -12.0f;

  static constexpr float radius = 12.0f;
  static constexpr float mass = 0.55f;
  static constexpr float restitution = 0.32f;
  static constexpr float linearDamping = 0.25f;
  static constexpr float groundFriction = 0.22f;

  static constexpr float fuseSeconds = 6.5f;

  static constexpr float explosionRadius = 95.0f;
  static constexpr float explosionImpulse = 420.0f;
  static constexpr float explosionVisualSeconds = 0.28f;
};

struct BombSlot {
  int bodyId = -1;
  BombPhase phase = BombPhase::Inactive;
  float fuseLeft = 0.0f;
  float visualLeft = 0.0f;
  Vec2 explosionCenter{};
  bool wasOnGround = false;
  float prevVelY = 0.0f;
};

struct BombSlotSnapshot {
  BombPhase phase = BombPhase::Inactive;
  float fuseLeft = 0.0f;
  float visualLeft = 0.0f;
  Vec2 explosionCenter{};
  bool wasOnGround = false;
  float prevVelY = 0.0f;
  BodySnapshot physics{};

  static BombSlotSnapshot Save(const BombSlot& slot, const Body& body);
  void Load(BombSlot& slot, Body& body) const;
};

class BombSystem {
public:
  explicit BombSystem(int maxBombs);

  void InitPool(PhysicsWorld& world);

  bool IsAiming(const InputState& input) const;

  bool UpdateThrow(float dt, const InputState& input, Vec2 mouseWorld, PhysicsWorld& world, const Body& player);
  void FixedUpdate(float dt, PhysicsWorld& world, int playerId,
                   const std::vector<int>& obstacleBodyIds);

  float Charge01() const;
  bool IsCharging() const { return m_charging; }
  void CancelCharge();

  void SetExplosionHandler(std::function<void(Vec2 center, int hitObstacleId)> handler);

  void Render(SDL_Renderer* r,
              float cameraX,
              float groundY,
              const PhysicsWorld& world,
              const Body& player,
              Vec2 mouseWorld,
              bool showAimGuide = true) const;

  const std::vector<int>& BodyIds() const { return m_bodyIds; }
  std::size_t MaxBombs() const { return m_slots.size(); }

  void SaveSnapshots(std::array<BombSlotSnapshot, 16>& out, const PhysicsWorld& world) const;
  void LoadSnapshots(const std::array<BombSlotSnapshot, 16>& in, PhysicsWorld& world);

private:
  Vec2 LaunchPosition(const Body& player) const;
  Vec2 ComputeThrowVelocity(const Body& player, Vec2 mouseWorld, float charge01) const;
  void Fire(const Body& player, Vec2 mouseWorld, float charge01, PhysicsWorld& world);
  bool TryFire(const Body& player, Vec2 mouseWorld, float charge01, PhysicsWorld& world);
  void SampleTrajectory(Vec2 spawnPos,
                        Vec2 velocity,
                        float gravityY,
                        float groundY,
                        std::vector<Vec2>& out) const;
  void DrawDashedTrajectory(SDL_Renderer* r, float cameraX, const std::vector<Vec2>& points) const;

  int AllocateSlot();
  void ArmSlot(int slotIndex, PhysicsWorld& world, const Body& player, Vec2 velocity);
  void Explode(int slotIndex, PhysicsWorld& world, int playerId, int hitObstacleId = -1);
  void ApplyExplosionImpulse(PhysicsWorld& world, int playerId, Vec2 center, float radius, float impulse);

  std::vector<BombSlot> m_slots;
  std::vector<int> m_bodyIds;
  std::function<void(Vec2 center, int hitObstacleId)> m_onExplosion;

  float m_chargeTime = 0.0f;
  bool m_charging = false;
};

} // namespace cr
