#include "core/Clock.h"

#include <SDL.h>

namespace cr {

void Clock::Reset() {
  m_freq = static_cast<double>(SDL_GetPerformanceFrequency());
  m_last = SDL_GetPerformanceCounter();
}

double Clock::TickSeconds() {
  const std::uint64_t now = SDL_GetPerformanceCounter();
  const std::uint64_t delta = now - m_last;
  m_last = now;
  return (m_freq > 0.0) ? (static_cast<double>(delta) / m_freq) : 0.0;
}

} // namespace cr
