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

### Event bubbling

`UIWidget::FireEvent` dispatches to the target, then its logical parents up to
the root until `UIEventData::handled` is true. Each widget runs `OnEvent`
followed by its registered callback; consuming an event stops ancestors, not
the callback on that same widget. Existing `void(const UIEventData&)`
callbacks remain valid. Callbacks leave events unhandled unless they set the
mutable flag:

```cpp
button->RegisterCallback(LambUI::UIEventType::OnClick,
  [](const LambUI::UIEventData& event) {
    event.handled = true;
  });
```

Mouse down/up/click and value/text/enter-pressed notifications bubble.
Mouse enter/leave stay target-local. Controls consume press/release events;
dropdowns also consume toggle and option-selection clicks. Mouse capture and
focus remain on the original hit widget, and bubbling preserves the original
payload. Use a fresh event payload for each dispatch (or reset `handled`).
Wheel input still uses the nearest `IScrollable` ancestor; keyboard/character
input still uses the focused `IFocusable`. Game-event subscriptions are a
separate broadcast mechanism and do not bubble.

### Compound widgets

Include `<LambUI/LambUI.h>` or the individual widget headers. All widgets
emit ordinary HAL commands and use injected input; no backend changes are
needed.

| Widget | C++ API |
|---|---|
| `UIProgressBar` | `SetMinMaxValues`, `SetValue`, `SetOrientation`, `SetColors`; non-interactive, horizontal or bottom-up vertical fill |
| `UITabControl` | `AddTab(label)` returns a retained page; `SetSelectedIndex` switches visible pages without losing their state |
| `UITreeView` | `AddNode(parentId, label)`, `SetExpanded`, `SetSelectedNode`, `SetNodeText`, `ClearNodes`; clipped, wheel-scrollable rows |
| `UIContextMenu` | `SetItems`, `Open`, `Close`; actions, disabled rows, separators, and scrolling for long menus |
| `UIMenuBar` | `AddMenu(label, items)` returns a context menu; click to toggle, hover to switch while a menu is open |
| `UITooltip` | Normally managed automatically through `UIWidget::SetTooltip(text)` and `UIManager::SetTooltipDelay(seconds)` |

Progress, tab selection, and tree selection emit `OnValueChanged` only when
the value changes; callbacks query the widget's current value. Tree node IDs
are local to a tree and remain valid until `ClearNodes`; `RootNode` (zero)
means the invisible root or no selection. Collapsing a branch preserves its
selection and descendant expansion state. Click the disclosure mark to
expand/collapse, or the label to select. Create page content under the widget
returned by `AddTab`, not directly under the tab control.

```cpp
using namespace LambUI;
auto& root = manager.GetRoot();
auto* tabs = root.CreateChild<UITabControl>("Inspector");
tabs->SetPoint(AnchorPoint::TopLeft, &root, AnchorPoint::TopLeft, 12, 40);
tabs->SetSize(320, 240);
auto* page = tabs->AddTab("Assets");
tabs->AddTab("Settings");
auto* tree = page->CreateChild<UITreeView>("Assets");
tree->SetAllPoints(page);
const auto folder = tree->AddNode(UITreeView::RootNode, "Textures");
tree->AddNode(folder, "Portrait");
tree->SetTooltip("Project assets");

auto* progress = root.CreateChild<UIProgressBar>("Loading");
progress->SetPoint(AnchorPoint::TopLeft, &root, AnchorPoint::TopLeft, 12, 292);
progress->SetSize(320, 16);
progress->SetMinMaxValues(0, 100);
progress->SetValue(35);

auto* menu = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager, "AssetMenu");
menu->SetItems({{"Collapse", [tree, folder] { tree->SetExpanded(folder, false); }},
        {"", {}, true, true}, {"Unavailable", {}, false}});
tree->RegisterCallback(UIEventType::OnClick, [menu, tree](const UIEventData& event) {
  if (event.button == MouseButton::Right) {
    event.handled = true;
    menu->Open(event.mouseX, event.mouseY, tree);
  }
});

auto* bar = root.CreateChild<UIMenuBar>(manager, "MainMenu");
bar->SetPoint(AnchorPoint::TopLeft, &root, AnchorPoint::TopLeft, 12, 8);
bar->SetSize(320, 28);
bar->AddMenu("View", {{"Assets", [tabs] { tabs->SetSelectedIndex(0); }},
             {"Settings", [tabs] { tabs->SetSelectedIndex(1); }}});
```

The manager owns a separate overlay layer whose logical parent is `Root`.
Create context menus directly under `GetOverlayRoot()` with the same manager;
`UIMenuBar` does this automatically. The active popup is laid out after the
normal tree and rendered after its scissor stack has closed. `ShowPopup`
also accepts a custom overlay widget with an explicit `SetSize`; pass an
owner to close it when that owner's ancestry becomes hidden. Only one popup
is active at a time. Outside clicks dismiss it without clicking through;
Escape closes it. Menu actions close the popup before invoking the callback,
so the callback can safely replace items or open another menu. Callbacks must
not outlive objects they capture, as with other LambUI callbacks.

Tooltips inherit from the nearest hovered ancestor with nonempty tooltip
text. `Update(deltaTime)` takes elapsed seconds and drives the default
0.5-second delay. Clicks, wheel input, keyboard input, and popup opening hide
tooltips; they never capture mouse input. Text is measured through
`ITextMeasurer`, with an approximate monospace fallback when none is supplied.
Popup and tooltip bounds are clamped to the display. Labels are single-line;
tabs divide the available width evenly. Nested submenus, keyboard navigation,
multiline tooltips, and Lua exposure of the new widgets are not implemented.

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
