#pragma once

#include "core/Input.h"
#include "game/Bomb.h"
#include "game/GravityField.h"
#include "game/PlayerSprite.h"
#include "physics/PhysicsWorld.h"
#include "render/UiText.h"
#include "rewind/RewindBuffer.h"

#include <vector>

struct SDL_Renderer;

namespace cr {

class Game {
public:
  Game(int width, int height);
  ~Game();

  void InitRenderer(SDL_Renderer* renderer);
  void HandleInput(float dt, const InputState& input);
  void FixedUpdate(float dt, const InputState& input);
  void Render(SDL_Renderer* r) const;

  bool WantsQuit() const { return m_quit; }
  bool IsPaused() const { return m_paused; }
  bool IsStarted() const { return m_started; }
  bool IsGameplayActive() const { return m_started && !m_paused; }

private:
  void SpawnProps();
  float CameraX() const;
  Vec2 MouseWorldPos(const InputState& input) const;
  void DrawTitleOverlay(SDL_Renderer* r) const;
  void DrawPauseOverlay(SDL_Renderer* r) const;

  int m_w = 0;
  int m_h = 0;
  bool m_quit = false;
  bool m_paused = false;
  bool m_started = false;
  float m_uiBlinkPhase = 0.0f;

  UiText m_ui;

  PhysicsWorld m_world;

  int m_playerId = -1;
  BombSystem m_bombs{16};
  GravityFieldSystem m_fields;
  PlayerSprite m_playerSprite;
  std::vector<int> m_propIds;

  float m_scrollSpeed = 240.0f;
  float m_playerScreenX = 140.0f;
  float m_jumpBuffer = 0.0f;
  float m_coyote = 0.0f;
  float m_runAnimPhase = 0.0f;
  float m_throwReleasePoseLeft = 0.0f;
  float m_rewindPoseLeft = 0.0f;

  RewindBuffer m_rewind;
  GameSnapshot m_snapshotScratch{};
  float m_stamina = 3.0f; // seconds of rewind budget

  InputState m_lastInput{};
};

} // namespace cr
