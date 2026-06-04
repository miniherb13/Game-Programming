#pragma once

#include "math/Vec2.h"

namespace cr {

struct InputState {
  bool quit = false;
  bool pausePressed = false;
  bool resumePressed = false;

  bool jumpPressed = false;
  bool throwPressed = false;
  bool rewindPressed = false;
  bool debugPressed = false;

  bool jumpHeld = false;
  bool throwHeld = false;
  bool throwReleased = false;
  bool rewindHeld = false;

  bool mouseDown = false;
  bool mouseReleased = false;
  Vec2 mousePos{};
  Vec2 mouseDownPos{};
};

class Input {
public:
  void BeginFrame();
  void Pump();
  // Merge latched key-down events into State (survives 0 fixed-timestep ticks this frame).
  void ApplyPending();
  // After gameplay fixed updates: drop jump/debug latch if physics ran.
  void FinishGameplayFrame(bool hadFixedStep);
  void ClearGameplayPending();
  void ConsumeRewindPending();
  void ConsumeThrowReleasedPending();

  const InputState& State() const { return m_state; }

private:
  InputState m_state{};
  bool m_jumpPending = false;
  bool m_rewindPending = false;
  bool m_debugPending = false;
  bool m_throwReleasedPending = false;
};

} // namespace cr
