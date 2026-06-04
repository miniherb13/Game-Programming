#pragma once

#include <SDL.h>

namespace cr {

class UiText {
public:
  bool Init(int fontPtSize = 20);
  void Shutdown();

  bool IsReady() const { return m_ready; }
  int LineHeight() const;

  void Draw(SDL_Renderer* renderer, int x, int y, const char* text, SDL_Color color) const;
  void DrawCentered(SDL_Renderer* renderer, int centerX, int y, const char* text, SDL_Color color) const;
  void DrawCenteredBlink(SDL_Renderer* renderer,
                         int centerX,
                         int y,
                         const char* text,
                         SDL_Color color,
                         float blinkPhaseSeconds,
                         float periodSeconds = 0.85f) const;

private:
  bool m_ready = false;
  int m_fontPtSize = 20;
};

} // namespace cr
