#pragma once

#include "math/Vec2.h"

namespace cr {

struct InputState {
  bool quit = false;

  bool jumpPressed = false;
  bool throwPressed = false;
  bool rewindPressed = false;

  bool mouseDown = false;
  bool mouseReleased = false;
  Vec2 mousePos{};
  Vec2 mouseDownPos{};
};

class Input {
public:
  void BeginFrame();
  void Pump();
  const InputState& State() const { return m_state; }

private:
  InputState m_state{};
};

} // namespace cr
