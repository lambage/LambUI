set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)

lambui_add_dependency(SDL TARGET SDL3::SDL3)

add_executable(lambui_example_sdl3
    src/main.cpp
    src/SDLExampleRenderer.cpp
)

target_link_libraries(lambui_example_sdl3 PRIVATE lambui SDL3::SDL3)
