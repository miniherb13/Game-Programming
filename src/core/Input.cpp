#include "core/Input.h"

#include <SDL.h>

namespace cr {

void Input::BeginFrame() {
  m_state.jumpPressed = false;
  m_state.throwPressed = false;
  m_state.rewindTogglePressed = false;
  m_state.mouseReleased = false;
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
        if (e.key.keysym.sym == SDLK_ESCAPE) m_state.quit = true;
        if (e.key.keysym.sym == SDLK_c) m_state.jumpPressed = true;
        if (e.key.keysym.sym == SDLK_x) m_state.throwPressed = true;
        if (e.key.keysym.sym == SDLK_z) {
          m_state.rewindToggledOn = !m_state.rewindToggledOn;
          m_state.rewindTogglePressed = true;
        }
        break;
      case SDL_KEYUP:
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
}

} // namespace cr
