include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.30
)
FetchContent_MakeAvailable(SDL3)

add_executable(lambui_example_sdl3
    src/main.cpp
    src/SDLExampleRenderer.cpp
)

target_link_libraries(lambui_example_sdl3 PRIVATE lambui SDL3::SDL3)
