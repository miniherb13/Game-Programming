#include "app/App.h"

#include "core/Clock.h"
#include "core/Input.h"
#include "core/Log.h"
#include "game/Game.h"

#include <SDL.h>

namespace cr {

int App::Run() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
    Log(LogLevel::Error, SDL_GetError());
    return 1;
  }

  constexpr int W = 1280;
  constexpr int H = 720;

  SDL_Window* window = SDL_CreateWindow("Chrono Rush (demo skeleton)",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        W,
                                        H,
                                        SDL_WINDOW_SHOWN);
  if (!window) {
    Log(LogLevel::Error, SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    Log(LogLevel::Error, SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  Clock clock;
  clock.Reset();

  Input input;
  Game game(W, H);

  constexpr float fixedDt = 1.0f / 60.0f;
  double acc = 0.0;

  while (!game.WantsQuit()) {
    const double dt = clock.TickSeconds();
    acc += dt;

    input.BeginFrame();
    input.Pump();

    // Fixed update
    while (acc >= fixedDt) {
      game.FixedUpdate(fixedDt, input.State());
      acc -= fixedDt;
    }

    SDL_SetRenderDrawColor(renderer, 12, 12, 16, 255);
    SDL_RenderClear(renderer);
    game.Render(renderer);
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

} // namespace cr
