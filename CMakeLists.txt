cmake_minimum_required(VERSION 3.24)

project(chrono_rush_demo
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CR_BUILD_TESTS "Build tests" OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# SDL2 (non-engine window/input/2D rendering)
# Note: On Windows, this fetches SDL sources and builds them as part of the project.
FetchContent_Declare(
  SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.30.6
)
FetchContent_MakeAvailable(SDL2)

add_executable(chrono_rush_demo
  src/main.cpp
  src/app/App.cpp
  src/core/Clock.cpp
  src/core/Input.cpp
  src/core/Log.cpp
  src/math/Vec2.cpp
  src/physics/PhysicsWorld.cpp
  src/rewind/RewindBuffer.cpp
  src/game/Game.cpp
)

target_include_directories(chrono_rush_demo PRIVATE
  include
)

if(TARGET SDL2::SDL2-static)
  # Prefer static linking on Windows to avoid missing DLL issues.
  target_link_libraries(chrono_rush_demo PRIVATE SDL2::SDL2-static)
elseif(TARGET SDL2-static)
  target_link_libraries(chrono_rush_demo PRIVATE SDL2-static)
elseif(TARGET SDL2::SDL2)
  target_link_libraries(chrono_rush_demo PRIVATE SDL2::SDL2)
elseif(TARGET SDL2)
  target_link_libraries(chrono_rush_demo PRIVATE SDL2)
else()
  message(FATAL_ERROR "SDL2 target not found after FetchContent.")
endif()

if(MSVC)
  target_compile_options(chrono_rush_demo PRIVATE /W4 /permissive-)
else()
  target_compile_options(chrono_rush_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()
