#include "core/Input.h"

#include "core/Log.h"

#include <SDL.h>
#include <cstdint>
#include <string>

namespace cr {

void Input::BeginFrame() {
  m_state.jumpPressed = false;
  m_state.throwPressed = false;
  m_state.rewindPressed = false;
  m_state.debugPressed = false;
  m_state.pausePressed = false;
  m_state.resumePressed = false;
  m_state.throwReleased = false;
  m_state.mouseReleased = false;
  // m_*Pending stay until consumed (see ApplyPending / FinishGameplayFrame).
}

void Input::ApplyPending() {
  if (m_jumpPending) m_state.jumpPressed = true;
  if (m_rewindPending) m_state.rewindPressed = true;
  if (m_debugPending) m_state.debugPressed = true;
  if (m_throwReleasedPending) m_state.throwReleased = true;
}

void Input::FinishGameplayFrame(bool hadFixedStep) {
  if (hadFixedStep) {
    m_jumpPending = false;
    m_debugPending = false;
  }
}

void Input::ClearGameplayPending() {
  m_jumpPending = false;
  m_debugPending = false;
  m_rewindPending = false;
  m_throwReleasedPending = false;
}

void Input::ConsumeRewindPending() {
  m_rewindPending = false;
}

void Input::ConsumeThrowReleasedPending() {
  m_throwReleasedPending = false;
}

void Input::Pump() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    switch (e.type) {
      case SDL_QUIT:
        m_state.quit = true;
        break;
      case SDL_KEYDOWN:
        if (e.key.repeat) break;
        if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) m_state.pausePressed = true;
        if (e.key.keysym.scancode == SDL_SCANCODE_SPACE) m_state.resumePressed = true;
        if (e.key.keysym.scancode == SDL_SCANCODE_C) {
          m_state.jumpPressed = true;
          m_jumpPending = true;
        }
        if (e.key.keysym.scancode == SDL_SCANCODE_X) m_state.throwPressed = true;
        if (e.key.keysym.scancode == SDL_SCANCODE_Z) {
          m_state.rewindPressed = true;
          m_rewindPending = true;
        }
        if (e.key.keysym.scancode == SDL_SCANCODE_G) {
          m_state.debugPressed = true;
          m_debugPending = true;
        }
        if (e.key.keysym.scancode == SDL_SCANCODE_C ||
            e.key.keysym.scancode == SDL_SCANCODE_X ||
            e.key.keysym.scancode == SDL_SCANCODE_Z ||
            e.key.keysym.scancode == SDL_SCANCODE_G) {
          Log(LogLevel::Info, std::string("KEYDOWN scancode=") + std::to_string(static_cast<int>(e.key.keysym.scancode)));
        }
        break;
      case SDL_KEYUP:
        if (e.key.keysym.scancode == SDL_SCANCODE_X) {
          m_state.throwReleased = true;
          m_throwReleasedPending = true;
        }
        if (e.key.keysym.scancode == SDL_SCANCODE_C ||
            e.key.keysym.scancode == SDL_SCANCODE_X ||
            e.key.keysym.scancode == SDL_SCANCODE_Z) {
          Log(LogLevel::Info, std::string("KEYUP   scancode=") + std::to_string(static_cast<int>(e.key.keysym.scancode)));
        }
        break;
      case SDL_MOUSEMOTION:
        m_state.mousePos = {static_cast<float>(e.motion.x), static_cast<float>(e.motion.y)};
        break;
      case SDL_MOUSEBUTTONDOWN:
        if (e.button.button == SDL_BUTTON_LEFT) {
          m_state.mouseDown = true;
          m_state.mouseDownPos = {static_cast<float>(e.button.x), static_cast<float>(e.button.y)};
          m_state.mousePos = m_state.mouseDownPos;
        }
        break;
      case SDL_MOUSEBUTTONUP:
        if (e.button.button == SDL_BUTTON_LEFT) {
          m_state.mouseDown = false;
          m_state.mouseReleased = true;
          m_state.mousePos = {static_cast<float>(e.button.x), static_cast<float>(e.button.y)};
        }
        break;
      default:
        break;
    }
  }

  const std::uint8_t* keys = SDL_GetKeyboardState(nullptr);
  m_state.jumpHeld = keys[SDL_SCANCODE_C] != 0;
  m_state.throwHeld = keys[SDL_SCANCODE_X] != 0;
  m_state.rewindHeld = keys[SDL_SCANCODE_Z] != 0;
}

} // namespace cr
