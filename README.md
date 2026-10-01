# LambUI

An engine-agnostic, retained-mode UI library for games, in the style of
World of Warcraft's Point-and-Anchor frame system. LambUI is pure C++17 with
zero graphics-API dependencies; a companion Lua binding library and a set of
example renderer backends live alongside it in this same repository.

See [goals.txt](goals.txt) for the original design discussion this project is
based on.

## Directory layout

```
LambUI/
├── CMakeLists.txt          # top-level build, options, install/export
├── cmake/                  # install/export helper modules
├── include/LambUI/         # public headers (the library's entire public API)
├── src/                    # library implementation (UIWidget, UIManager, ...)
├── lua/                    # optional sol2-based Lua scripting companion library
├── examples/               # renderer backend showcases (OpenGL, Vulkan, SDL3)
└── tests/                  # GoogleTest unit tests (layout solver, events, ...)
```

## Architecture: the Dual-Tree Pattern

- **Logical tree** (`UIWidget` and subclasses): parent/child hierarchy,
  anchors, visibility, event callbacks. Knows nothing about any graphics API.
- **Render tree** (`UIRenderCommand`): a flat vector of draw instructions
  (`DrawQuad`, `DrawString`, `PushScissor`/`PopScissor`, `CustomCallback`)
  produced once per frame by `UIManager::Render()`.

Your engine implements `LambUI::IRenderer::SubmitRenderCommands()` and reads
that flat vector to issue native draw calls. The library never calls into
Vulkan/OpenGL/D3D12/etc. itself — see `examples/` for four different
implementations of that one interface.

### How SOLID maps onto this codebase

- **Single Responsibility**: layout math lives in the pure, dependency-free
  `ResolveAnchoredRect()` function ([include/LambUI/UILayoutSolver.h](include/LambUI/UILayoutSolver.h)),
  separate from tree ownership (`UIWidget`) and separate from input
  routing/frame orchestration (`UIManager`).
- **Open/Closed**: new widget types are added by subclassing `UIWidget`/`UIControl`
  and overriding `OnGenerateRenderCommands`/`OnEvent` — `UIManager` never
  needs to change to support a new widget type.
- **Liskov Substitution**: every concrete widget (`UIButton`, `UISlider`, ...)
  is a fully substitutable `UIWidget`; `UIManager` and the layout solver only
  ever operate through that base interface.
- **Interface Segregation**: `IRenderer` (drawing) and `ITextMeasurer` (text
  metrics) are separate small interfaces, as are `IDraggable`/`IFocusable`
  (input capture) — a widget only implements what it actually needs.
- **Dependency Inversion**: `UIManager` depends on the `IRenderer` abstraction,
  never on a concrete backend; `UISlider`/`UIInputBox` are discovered via
  `dynamic_cast<IDraggable*>`/`dynamic_cast<IFocusable*>` rather than
  `UIManager` hardcoding those concrete types.

### Known limitations (by design, for now)

- Anchors must reference a widget that has *already* been positioned this
  pass (typically its parent or an earlier sibling) — there's no
  general dependency-graph solver for arbitrary forward references.
- Text rendering uses a single-channel SDF atlas for printable ASCII, not
  full MSDF or Unicode shaping. The OpenGL 3.3 example samples it with
  derivative-based `smoothstep`; legacy OpenGL and SDL3 use approximations.
- The Vulkan example only brings up a window/swapchain/clear-color loop; it
  does not yet translate `UIRenderCommand`s into an actual pipeline (see the
  TODO in `examples/vulkan/src/VulkanExampleRenderer.cpp`). The OpenGL and
  SDL3 examples do draw the real widget tree.

## Building

Requires CMake 3.20+ and a C++17 compiler. All third-party dependencies
(GoogleTest, GLFW, GLAD, SDL3, sol2, Lua) are fetched on demand via `FetchContent` —
no vcpkg/Conan setup required.
The OpenGL 3.3 example also requires Python 3 to generate its GLAD loader
from the pinned, bundled OpenGL specification.

```powershell
cmake -S . -B build -DLAMBUI_BUILD_EXAMPLES=ON -DLAMBUI_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

On Windows with MSVC, run this from a "Developer PowerShell for VS" (or after
calling `vcvarsall.bat`) so `cl.exe` is on `PATH`, or pass `-G "Visual Studio 17 2022"`
to let CMake drive `msbuild` itself instead of Ninja.

### Build options

| Option | Default | Description |
|---|---|---|
| `LAMBUI_BUILD_EXAMPLES` | `ON` if top-level | Build `examples/` (OpenGL, Vulkan, SDL3) |
| `LAMBUI_BUILD_TESTS` | `ON` if top-level | Build the GoogleTest suite in `tests/` |
| `LAMBUI_BUILD_LUA_BINDINGS` | `OFF` | Build `lua/` (sol2 + Lua, fetched on demand) |
| `LAMBUI_INSTALL` | `ON` if top-level | Generate install/export targets |
| `LAMBUI_EXAMPLE_OPENGL` / `_OPENGL33` / `_VULKAN` / `_SDL3` | `ON` | Toggle individual examples (Vulkan auto-skips if the SDK isn't found) |

### Consuming LambUI from another CMake project

```cmake
include(FetchContent)
FetchContent_Declare(LambUI GIT_REPOSITORY <this-repo-url> GIT_TAG main)
set(LAMBUI_BUILD_EXAMPLES OFF)
set(LAMBUI_BUILD_TESTS OFF)
FetchContent_MakeAvailable(LambUI)

target_link_libraries(my_game PRIVATE LambUI::lambui)
```

Or, after `cmake --install`, via `find_package(LambUI REQUIRED)` and linking
`LambUI::lambui`.

## Examples

Each example under `examples/` builds a `LambUI::UIManager` demo widget tree
with its own `IRenderer` implementation and windowing/input glue:

- `lambui_example_opengl` — GLFW + legacy OpenGL 1.1 immediate mode (no
  loader dependency needed).
- `lambui_example_opengl33` — GLFW + OpenGL 3.3 core, GLAD, VAO/VBO triangles,
  and GLSL 330 shaders. An animated contour canvas has a distortion slider
  and pause/resume button; labels use antialiased SDF text.
- `lambui_example_sdl3` — SDL3 + its built-in 2D `SDL_Renderer` API.
- `lambui_example_vulkan` — GLFW + Vulkan window/swapchain bring-up; drawing
  the widgets themselves is still TODO (see limitations above).

### OpenGL 3.3 shader showcase

```powershell
cmake --build build --target lambui_example_opengl33
.\build\examples\opengl33_glfw\lambui_example_opengl33.exe
```

Requires a driver supporting OpenGL 3.3 core. The legacy example remains
available independently. The demo finds a common system font on Windows,
Linux, or macOS; use `--font "path/to/font.ttf"` to select another font.
Escape closes the window. Shader sources are embedded in
[GL33ExampleRenderer.cpp](examples/opengl33_glfw/src/GL33ExampleRenderer.cpp).

All graphics resources and input callbacks belong to the example. The canvas
is drawn through `CustomCallback` in LambUI's command bucket; neither shader
logic nor GLFW polling enters the core library. Text uses `GL_R8` sampling
and `fwidth`/`smoothstep`. Nested scissors convert logical window coordinates
to framebuffer pixels for high-DPI displays. Texture handles in this backend
encode a `GLuint` through `uintptr_t`; one font atlas is supported.

Run the bounded native GPU checks (requires a working desktop GL context):

```powershell
.\build\examples\opengl33_glfw\lambui_example_opengl33.exe --smoke-test --screenshot build/opengl33
```

This checks shader compilation/linking, SDF coverage, texture UV sampling,
nested clipping, state recovery after a custom callback, injected button and
slider input, and changing shader output at wide and compact window sizes.
It exits nonzero on failure and optionally writes `*-desktop.ppm` and
`*-compact.ppm` screenshots. These GPU checks are separate from CTest so the
core tests remain runnable without a display. GL resources are released
before the GLFW context is destroyed.

## Lua bindings

`lua/` builds `lambui_lua`, a small sol2-based binding layer exposing a
WoW-like scripting surface:

```lua
local playerFrame = UI.CreateFrame("Frame", "PlayerUnitFrame")
playerFrame:SetSize(200, 60)
playerFrame:SetPoint("TOPLEFT", UI.Root, "TOPLEFT", 20, -20)

local healthBar = playerFrame:CreateStatusBar("PlayerHealthBar")
healthBar:SetPoint("TOPLEFT", playerFrame, "TOPLEFT", 10, -10)
healthBar:SetMinMaxValues(0, 100)

playerFrame:RegisterEvent("PLAYER_HEALTH_CHANGED", function(newHealth)
    healthBar:SetValue(newHealth)
end)
```

Enable it with `-DLAMBUI_BUILD_LUA_BINDINGS=ON`. It is off by default so that
consumers who only want the core C++ library don't pay for fetching Lua/sol2.
