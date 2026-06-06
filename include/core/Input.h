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
  bool slowPressed  = false;  // Shift — 토글 모드용
  bool slowModeTogglePressed = false;  // H — 슬로우 홀드/토글 전환

  bool jumpHeld    = false;
  bool throwHeld   = false;
  bool throwReleased = false;
  bool rewindHeld  = false;
  bool slowHeld    = false;  // Shift 홀드

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
  void ConsumeSlowPending();
  bool ConsumeJumpPressForFixedStep();

  const InputState& State() const { return m_state; }

private:
  InputState m_state{};
  bool m_jumpPending          = false;
  bool m_rewindPending        = false;
  bool m_debugPending         = false;
  bool m_throwReleasedPending = false;
  bool m_slowPending          = false;
};

} // namespace cr