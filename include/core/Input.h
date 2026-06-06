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
  bool slowPressed  = false;  // F키 슬로우모션

  bool jumpHeld    = false;
  bool throwHeld   = false;
  bool throwReleased = false;
  bool rewindHeld  = false;
  bool slowHeld    = false;  // F키 홀드

  bool mouseDown = false;
  bool mouseReleased = false;
  Vec2 mousePos{};
  Vec2 mouseDownPos{};
};

class Input {
public:
  void BeginFrame();
  void Pump();
  void ApplyPending();
  void FinishGameplayFrame(bool hadFixedStep);
  void ClearGameplayPending();
  void ConsumeRewindPending();
  void ConsumeThrowReleasedPending();
  bool ConsumeJumpPressForFixedStep();

  const InputState& State() const { return m_state; }

private:
  InputState m_state{};
  bool m_jumpPending          = false;
  bool m_rewindPending        = false;
  bool m_debugPending         = false;
  bool m_throwReleasedPending = false;
  bool m_slowPending          = false;  // F키
};

} // namespace cr