# LambUI

An engine-agnostic, retained-mode UI library for games, in the style of
World of Warcraft's Point-and-Anchor frame system. LambUI is C++14 with
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
├── lua/                    # optional Lua C API scripting companion library
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

### Layout and styling

All widgets support `SetMargin(UIInsets)` and `SetPadding(UIInsets)`, in
left/top/right/bottom order. Insets are nonnegative pixels. Parent anchors
target the parent's padded `GetContentRect()`; sibling anchors target the
sibling's outer rectangle. Margins move each widget anchor inward (center
anchors use half the opposing-margin difference). `GetComputedRect()` and
hit testing describe the outer box, excluding margins. Padding does not
automatically move a widget's own text or decorations; it positions children.
For compound controls, put application padding on their content/page widget.
Scroll containers use their padded content box for scrolling and child clipping.

`SetRelativeSize(widthFraction, heightFraction)` uses the parent's content
size minus this widget's margins: `0.5f` means 50%; a negative axis keeps its
explicit pixel size. `SetSize` returns both axes to pixel sizing. Opposing
anchors still stretch and take precedence over either requested size.
`SetMinSize`/`SetMaxSize` constrain the resulting outer box, including stretched
boxes; minimum wins if limits conflict. Maximum defaults to infinity.
`SetAspectRatio(width / height)` fits inside that box while respecting limits;
zero disables it. If the ratio and limits are incompatible, limits win.
When constraints change the solved size, the first registered anchor stays
fixed. Relative sizing requires a parent; roots retain manager display sizes.
The window-specific `SetSizeLimits` still governs normal-window operations;
inherited min/max constraints also apply to minimized/maximized layout.

```cpp
auto* panel = manager.GetRoot().CreateChild<LambUI::UIWidget>("Panel");
panel->SetPoint(LambUI::AnchorPoint::TopLeft, &manager.GetRoot(),
                LambUI::AnchorPoint::TopLeft);
panel->SetMargin({12, 12, 12, 12});
panel->SetPadding({16, 12, 16, 12});
panel->SetRelativeSize(0.5f, 0.5f);
panel->SetMinSize(120, 80);
panel->SetMaxSize(480, 320);
panel->SetAspectRatio(1.5f);
LambUI::UIStyle style;
style.fillColor = 0x344A49FFu;
style.cornerRadius = 8;
style.pattern = LambUI::UIFillPattern::Checkerboard;
style.patternColor = 0x3C5553FFu;
style.patternSize = 12;
style.shadowColor = 0x00000080u;
style.shadowOffsetY = 4;
style.shadowBlur = 6;
panel->SetStyle(style);
```

Styles are opt-in values, not cascading rules. `ClearStyle()` restores the
original rendering. A widget's first full-bounds quad is its background;
styling replaces that quad, retaining its texture and UVs. Without one, a
background is inserted. Leaving `fillColor` unset preserves state-dependent
colors (such as button hover/press); an explicit color overrides them.
Colors are packed RGBA. Patterns include solid, checkerboard, and horizontal
stripes. Shadows support color, signed offsets, nonnegative spread and blur.
Backgrounds/shadows use existing quad commands on all drawing backends:
rounded edges are pixel-strip approximations, blur is an eight-layer falloff,
not a Gaussian shader. Curves use at most 32 strips per corner half; pattern
cells grow as needed to bound each axis to 64 cells. This trades extra commands
for portability. Rounded backgrounds do not round child clipping or hit tests,
and internal decorations remain unchanged. Shadows obey ancestor clips but
sit outside their own container's clip. No graphics headers or HAL changes.
The GL33/SDL3 Build tab demonstrates these APIs; Lua exposure remains pending.

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

### Scroll containers

Create scrolling children under `UIScrollContainer::GetContent()` and declare
their total extent with `SetContentSize(width, height)`. Horizontal and vertical
scrollbars appear automatically when content overflows. Drag a thumb or click
the track to page by one viewport; wheel input continues to work over either.
Thumbs are proportional, with a 20-pixel minimum capped to the available track.

Bars overlay the inside right/bottom edges (12 pixels), preserving the existing
viewport size and scroll ranges. Leave that space clear when positioning content
that must remain unobscured. `SetScrollbarsEnabled(false)` hides both bars without
disabling wheel or programmatic scrolling; `AreScrollbarsEnabled()` reports the
setting, which defaults to true. Tree views and the example galleries reuse this
behavior. Rendering uses ordinary clipped quad commands and injected input only.

### Compound widgets

Include `<LambUI/LambUI.h>` or the individual widget headers. All widgets
emit ordinary HAL commands and use injected input; no backend changes are
needed.

| Widget | C++ API |
|---|---|
| `UIProgressBar` | `SetMinMaxValues`, `SetValue`, `SetOrientation`, `SetColors`; non-interactive, horizontal or bottom-up vertical fill |
| `UITabControl` | `AddTab(label)` returns a retained page; `SetSelectedIndex` switches visible pages without losing their state |
| `UITreeView` | `AddNode(parentId, label)`, `SetExpanded`, `SetSelectedNode`, `SetNodeText`, `ClearNodes`; clipped, wheel-scrollable rows |
| `UIContextMenu` | `SetItems`, `Open`, `Close`; actions, nested `UIMenuItem::children`, disabled rows, separators, and scrolling for long menus |
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
tree->SetTooltip("Project assets\nLocal workspace");

auto* progress = root.CreateChild<UIProgressBar>("Loading");
progress->SetPoint(AnchorPoint::TopLeft, &root, AnchorPoint::TopLeft, 12, 292);
progress->SetSize(320, 16);
progress->SetMinMaxValues(0, 100);
progress->SetValue(35);

auto* menu = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager, "AssetMenu");
menu->SetItems({{"Folders", {}, true, false, {
          {"Collapse", [tree, folder] { tree->SetExpanded(folder, false); }},
          {"Expand", [tree, folder] { tree->SetExpanded(folder, true); }}}},
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
chain is active at a time; `GetActivePopup()` returns its root. Menu items with
nonempty `children` open submenus on hover, click, Enter/Space, or Right;
their own action is ignored. Left returns keyboard focus to the parent row;
Up/Down, Home/End, and Tab navigate the focused menu, skipping unavailable rows.
Submenus render outside parent clips, flip left at the right display edge,
and close when their owning row scrolls out of view. Outside clicks dismiss
the chain without clicking through; Escape closes the whole chain.
Menu actions close the chain before invoking the callback,
so the callback can safely replace items or open another menu. Callbacks must
not outlive objects they capture, as with other LambUI callbacks.

Tooltips inherit from the nearest hovered ancestor with nonempty tooltip
text. `Update(deltaTime)` takes elapsed seconds and drives the default
0.5-second delay. Clicks, wheel input, keyboard input, and popup opening hide
tooltips; they never capture mouse input. Text is measured through
`ITextMeasurer`, with an approximate monospace fallback when none is supplied.
Tooltips preserve CR/LF/CRLF and blank lines, wrap words and long UTF-8 text,
and reflow after display-size changes. Their width is capped at 320 pixels
or the display width, including 12 pixels of horizontal padding; excess height
is clipped to the display. Direct `UITooltip::SetText` calls can supply a third
`maximumWidth` argument. Popup and tooltip bounds are clamped to the display.
Other compound labels remain single-line; tabs divide the available width evenly.
Lua exposure of the new widgets remains pending.

### Checkboxes, radio buttons, and windows

`UICheckBox` and `UIRadioButton` expose `SetText`, `SetChecked`, `IsChecked`,
and `SetEnabled`. Left click or Space/Enter on a focused control activates it;
key repeats do not retrigger selection. Disabled controls remain visible and
block click-through, but programmatic changes still work. Changed selections
emit `OnValueChanged`.

Radio buttons with the same `SetGroup(string)` and immediate parent are
exclusive; the empty group name is also a group. Clicking an already selected
radio keeps it selected. Programmatic `SetChecked(false)` can clear a group.
All peer states are updated before selection callbacks run. Arrow keys move
focus and selection through visible, enabled members of the same group.

### Keyboard navigation

Translate platform keys to `LambUI::ScanCode` before calling
`UIManager::InjectKeyEvent(code, isDown)`. Forward both press and release,
including `LeftShift` and `RightShift`; text still arrives separately through
`InjectCharacter`. GL, GL33, and SDL3 examples provide these mappings.

- Tab/Shift-Tab wrap forward/backward in logical tree order. Hidden subtrees,
  disabled checkboxes/radios, and nonfocusable widgets are skipped. Focused
  descendants of scroll containers are scrolled into view.
- `GetFocusedWidget()` exposes current focus. Custom widgets implement
  `IFocusable`; `CanFocus()` controls eligibility and `GetFocusNeighbor()`
  optionally supplies directional targets. `SetKeyboardEnabled(false)` opts
  a widget out without disabling its mouse input or its children's keyboard input.
- Space/Enter activate buttons and checkboxes on matching key release.
  Focus loss cancels pending activation. Slider arrows adjust by 1% of its
  range; Home/End select its endpoints.
- Dropdown Up/Down and Home/End select options. Space/Enter toggle the list,
  Escape closes it, and leaving focus closes it. Tabs use arrows to switch
  pages, with Home/End selecting the first/last page.
- Tree Up/Down select visible rows; Right expands or enters a branch; Left
  collapses or selects its parent. Home/End select endpoints; Space/Enter
  toggle expansion. Selected rows scroll into view.
- Menu bars use Left/Right to choose headers and Down/Enter/Space to open.
  Popup Up/Down and Tab cycle enabled items, Home/End choose endpoints, and
  Escape dismisses. Left/Right switches open menu-bar menus. Popups contain
  keyboard focus and restore the previous eligible widget on dismissal.
- Input Left/Right move by UTF-8 code point; Home/End move to the beginning/end.
  Insertion, Backspace, and Delete operate at that position. Cursor positions
  exposed by `GetCursorPosition()` are UTF-8 byte offsets.

Menu rows show keyboard selection using `UIButton::SetKeyboardFocusColor`.
Focused widgets also receive a two-tone inset outline, drawn after their own
subtree and within ancestor clips. It follows mouse or keyboard focus without
changing layout; hidden/disabled targets lose it. Use `SetFocusRingEnabled(false)`
for custom focus rendering, or `SetFocusRingColor(rgba)` to change its inner color.
Custom render overrides continue using `GenerateChildRenderCommands` or
`AppendChildRenderCommands` so children retain focus decoration.
Buttons show their pressed color while Enter/Space is held; releasing the matching
key activates once, and focus loss cancels. Enter targets the focused control,
not an implicit dialog-default button. Clipboard support remains backlog work.

`UIWindow` is a retained widget, not a native OS window. Add application
widgets beneath `GetContent()` to keep them clipped inside its client area.
Left-drag the title bar to move, or any edge/corner to resize. Clicking a
window or its contents raises it above its siblings. `SetMovable(false)` and
`SetResizable(false)` independently lock these interactions.

```cpp
auto* window = manager.GetRoot().CreateChild<UIWindow>("Inspector");
window->SetTitle("Material settings");
window->SetSizeLimits(260, 180, 900, 700);
window->SetBounds(40, 60, 360, 280);
window->SetMovable(true);
window->SetResizable(false);
window->SetButtonMode(WindowButton::Minimize, WindowButtonMode::Enabled);
window->SetButtonMode(WindowButton::Maximize, WindowButtonMode::Disabled);
window->SetButtonMode(WindowButton::Close, WindowButtonMode::Hidden);

auto* enabled = window->GetContent()->CreateChild<UICheckBox>("Preview");
enabled->SetText("Live preview");
enabled->SetPoint(AnchorPoint::TopLeft, window->GetContent(), AnchorPoint::TopLeft, 12, 12);
enabled->SetChecked(true);
```

Each title-bar button independently supports `Enabled`, `Disabled` (visible
but inert), or `Hidden` (takes no space). These policies only govern input;
the host can still call `Minimize`, `Maximize`, `Restore`, and `Close`.
Minimize collapses to the title bar and hides client input; maximize fills
the parent and follows its size. Restore returns to saved normal bounds,
or to maximized state when minimized from maximized. Close hides without
destroying content; reopen with `SetVisible(true)`. State transitions emit
`OnWindowStateChanged`; closing emits `OnClose` once while visible.

`SetBounds` uses parent-relative coordinates, selects normal state, and
applies size limits. Limits also apply to interactive resize and restore,
not maximized/minimized sizes or inherited raw `SetSize`/anchor setters.
Moving converts anchors to a parent-relative top-left position and keeps
the title bar reachable within the parent. Avoid cross-window sibling
anchors because activation changes sibling ordering. Mouse capture pairs
press/release buttons and ignores hidden captured widgets.

### Multiple fonts

`UITextWidget` and `UIInputBox` expose `SetFont(void*)` and `GetFont()`.
The handle is an opaque key, passed unchanged in `UIRenderCommand::fontHandle`;
the core never dereferences it or creates GPU resources. A null handle selects
the default font. The atlas measurer and example renderers also fall back to
the default for unknown handles.

Register each additional atlas with both the measurer and your renderer using
the same unique, non-null handle. In the GL/GL33/SDL3 examples:

```cpp
FontAtlas bodyFont;
FontAtlas headingFont;
if (!bodyFont.LoadFromFile("assets/body.ttf", 20) ||
  !headingFont.LoadFromFile("assets/heading.ttf", 26)) return;

auto measurer = std::make_shared<FontAtlasTextMeasurer>(bodyFont);
void* headingHandle = &headingFont;
if (!renderer->LoadFont(bodyFont) ||
  !renderer->LoadFont(headingFont, headingHandle) ||
  !measurer->RegisterFont(headingHandle, headingFont)) return;

UIManager manager(renderer, measurer);
auto* title = manager.GetRoot().CreateChild<UITextWidget>("Title");
title->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
title->SetTextMeasurer(manager.GetTextMeasurer());
title->SetText("Inventory");
title->SetFont(headingHandle);
```

`FontAtlasTextMeasurer::RegisterFont` returns false for null or duplicate keys
without changing the existing registration. The example renderers reserve
`LoadFont(atlas)` for the default and reject duplicate keys or failed uploads.
`GetFont(handle)` on the atlas measurer resolves a key to its atlas, including
default fallback. Registration is additive; replacement/unregistration is not
supported. Register fonts before assigning them to text widgets.

Atlases are borrowed: keep them alive and unchanged while registered, and keep
handle identities stable. Renderer instances own their uploaded textures and
must be destroyed before their graphics context. Widgets borrow their measurer.
Changing an unwrapped text widget's font, text, or measurer recalculates its
intrinsic size; explicit anchors still control final layout. Input fields retain their
explicit size. Existing compound-control labels continue using the default
font. Custom `IRenderer`/`ITextMeasurer` implementations must agree on handle
mapping and fallback; the HAL has no new virtual methods.

### Text wrapping and multiline input

`UITextWidget` recognizes LF, CRLF, and CR line breaks, including empty and
trailing lines. With a text measurer, unwrapped labels auto-size to the widest
line and total line height. `SetWordWrap(true)` disables intrinsic resizing:
assign a size or anchors, and text wraps within the resolved content width and
clips to the content bounds. Resizing, padding, text, and font changes reflow
the lines. Both text widgets and input fields honor their own padding.

```cpp
auto* description = manager.GetRoot().CreateChild<UITextWidget>("Description");
description->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 20, 20);
description->SetTextMeasurer(manager.GetTextMeasurer());
description->SetWordWrap(true);
description->SetSize(280, 80);
description->SetText("Material details\nA longer description wraps within the assigned width.");

auto* notes = manager.GetRoot().CreateChild<UIInputBox>("Notes");
notes->SetPoint(AnchorPoint::TopLeft, description, AnchorPoint::BottomLeft, 0, 12);
notes->SetSize(280, 120);
notes->SetMultiline(true);
notes->SetText("First line\nSecond line");
```

Inputs remain single-line by default. In multiline mode, Enter inserts LF and
fires `OnTextChanged`, not `OnEnterPressed`; single-line Enter still submits.
Forward Enter through `InjectKeyEvent`, not `InjectCharacter`. `SetText`
normalizes CRLF/CR to LF in multiline mode and replaces line breaks with spaces
in single-line mode; disabling multiline flattens existing text the same way.
Programmatic setters do not fire editing events.

Multiline inputs wrap by default; `SetWordWrap(false)` retains only explicit
line breaks and permits horizontal caret scrolling. Home/End target visual
line boundaries; Up/Down retain the preferred horizontal position across short
lines. Left/Right, Backspace, and Delete remain UTF-8-codepoint aware, including
joining lines by deleting a newline. The blinking caret scrolls into view both
vertically and horizontally; Tab continues normal focus navigation.

Wrapping prefers spaces/tabs and splits oversized words at UTF-8 codepoint
boundaries. Whitespace is preserved; spaces at a full line's end may hang outside
the clip without moving the wrapped caret out of bounds. Each visual line is a
separate existing `DrawString` command, so backends need no new wrapping API.
Measurement uses the selected font, or an 8px advance/16px line-height fallback.
This does not add Unicode shaping, clipboard support, or scrollbars
inside inputs. The GL33/SDL3 Controls tab includes wrapped material text and
editable multiline notes.

### Text selection

`UIInputBox::SetEditingEnabled(false)` makes the field read-only;
`IsEditingEnabled()` reports the flag, which defaults to true. It blocks typing,
Backspace, Delete, and multiline Enter without clearing the current selection.
Focus, pointer/keyboard navigation, selection, and application `SetText` updates
remain available. Single-line Enter still fires `OnEnterPressed`, since it does
not edit text. Re-enable editing with `SetEditingEnabled(true)`.

Editing and selection are independent properties:

```cpp
notes->SetEditingEnabled(false);
notes->SetSelectionEnabled(true);
```

`UIInputBox` enables selection by default in both single-line and multiline
mode. `SetSelectionEnabled(false)` clears the range and disables user and
programmatic selection without disabling editing or click-to-position.
`IsSelectionEnabled()` reports the property. The Controls showcase exposes it
through the notes field's **Select text** checkbox.

- Click positions the caret; left-button drag selects using captured pointer
  input. Shift-click extends from the existing anchor. Dragging outside the
  field scrolls toward the pointer using injected `Update(deltaTime)` time.
- Shift+Left/Right/Home/End/Up/Down extends the range using the same visual-line
  layout as the caret. Unmodified Left/Right collapses a range to its start/end.
- Ctrl+A selects all; Ctrl+Home/End goes to the document start/end, with Shift
  extending selection. Forward both Control keys and `ScanCode::A`, as well as
  both Shift keys, through `InjectKeyEvent`; all three native text examples do so.
- Typing, Backspace, Delete, and multiline Enter replace/delete the selected
  range and emit one `OnTextChanged` notification per edit.

`SetSelection(anchor, cursor)` sets a directional range in UTF-8 byte offsets,
clamped to the string and rounded down to codepoint boundaries. `SelectAll()`
and `ClearSelection()` are convenience methods; `GetSelectionStart()` and
`GetSelectionEnd()` return the ordered half-open range, and `GetSelectedText()`
returns its contents. These setters do not fire text-change events. Selection
persists across focus loss and is drawn with an inactive tint; focus loss or
disabling selection cancels an active drag. Highlights are clipped HAL quads
behind the text, including explicit blank lines. Static `UITextWidget` labels
are not selectable; clipboard and Unicode grapheme/shaping support remain separate.

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

Requires CMake 3.20+ and a C++14 compiler, including for the optional Lua bindings
and examples. A newer parent-project C++ standard is respected. Boost.Optional
and Boost.Algorithm supply optional values and clamping without compiled Boost
libraries. Boost 1.85+ is discovered through its CMake config; when unavailable,
Boost 1.86.0 is fetched from a checksum-pinned archive. The core also uses fmt
and private stb headers. These and the enabled example/test dependencies
(GoogleTest, GLFW, GLAD, SDL3) are fetched on demand via `FetchContent`.
Lua bindings reuse Lua 5.3+ when found, or fetch Lua 5.4.6. No sol2, vcpkg,
or Conan setup is required.
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
| `LAMBUI_BUILD_LUA_BINDINGS` | `OFF` | Build `lua/` (Lua C API; Lua fetched if not found) |
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

Installed-package consumers also need fmt and Boost's optional/algorithm CMake
packages; the fetched dependencies install alongside the core. `UIStyle::fillColor`
and `UIWidget::GetStyle()` now use `boost::optional` rather than `std::optional`.
Assignment, boolean checks, dereferencing, `reset()` and `value_or()` retain their
usual usage; use `boost::none` in place of `std::nullopt`.

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
Linux, or macOS; use `--font "path/to/font.ttf"` for body text and
`--heading-font "path/to/heading.ttf"` for the title font.
Escape dismisses an open menu; otherwise it closes the window. Shader sources are embedded in
[GL33ExampleRenderer.cpp](examples/opengl33_glfw/src/GL33ExampleRenderer.cpp).

All graphics resources and input callbacks belong to the example. The canvas
is drawn through `CustomCallback` in LambUI's command bucket; neither shader
logic nor GLFW polling enters the core library. Text uses `GL_R8` sampling
and `fwidth`/`smoothstep`. Nested scissors convert logical window coordinates
to framebuffer pixels for high-DPI displays. Texture handles in this backend
encode a `GLuint` through `uintptr_t`; font handles independently select
registered atlas/texture pairs.

Run the bounded native GPU checks (requires a working desktop GL context):

```powershell
.\build\examples\opengl33_glfw\lambui_example_opengl33.exe --smoke-test --screenshot build/opengl33
```

This checks shader compilation/linking, SDF coverage, texture UV sampling,
nested clipping, state recovery after a custom callback, injected button and
slider input, and changing shader output at wide and compact window sizes.
Normal-size text is also checked for soft edges and solid strokes at
1x/1.5x/2x display scales and integer/half-pixel positions. The font shader
uses a widened derivative-based SDF transition to reduce small-text aliasing.
The smoke checks also verify registered atlas metrics, distinct heading-font
pixels, and exact default-font fallback for an unknown handle.
It exits nonzero on failure and optionally writes `*-desktop.ppm` and
`*-compact.ppm` screenshots. These GPU checks are separate from CTest so the
core tests remain runnable without a display. GL resources are released
before the GLFW context is destroyed.

### Compound widget showcases

The legacy OpenGL, OpenGL 3.3, and SDL3 examples all use
[WidgetShowcase.h](examples/common/WidgetShowcase.h), an example-only,
backend-independent gallery of the available controls and containers. The GL33 demo keeps
its animated contour canvas and distortion controls, with an asset inspector
overlaid on the right. Legacy GL and SDL3 present a resizable asset browser.
Legacy GL draws its material preview with fixed-function quads and line strips;
it needs no shader loader and retains its alpha-tested SDF text approximation.
Vulkan remains a separate swapchain-only example.

```powershell
cmake --build build --target lambui_example_opengl lambui_example_opengl33 lambui_example_sdl3
.\build\examples\opengl_glfw\lambui_example_opengl.exe
.\build\examples\opengl33_glfw\lambui_example_opengl33.exe
.\build\examples\sdl3\lambui_example_sdl3.exe
```

- **Assets:** select tree rows, expand/collapse folders, and scroll the list.
- **Build job:** inspect the selected asset and pause/resume or restart a
  simulated build. Its progress bar advances with real frame time.
- **Controls:** checkbox, radio groups, a disabled checkbox, speed slider,
  editable asset name, labeled dropdown, and custom-rendered material canvas.
- **Window:** live move/resize toggles and independent On/Off/Hide settings
  for minimize, maximize, and close. Open preview creates no new objects:
  it reopens/repositions the retained window. Scroll for lower settings on
  compact displays; View > Open preview also remains available after closing.
- **View / Build menus:** switch tabs and control the build through menu actions.
- **Context menu:** right-click the tree for build and folder actions; the
  menu also shows a separator and a disabled action. Outside clicks or Escape
  dismiss it without clicking through to another control.
- **Tooltips:** hover over the tree or build pause button. GL's animation
  pause button and distortion slider also have tooltips.

All three demos use a 20px body font to fit fixed-height widget rows and a separate
26px heading font. They discover a system serif face for headings, falling
back to the body face at 26px when none is available. Legacy GL and SDL3 support the same
`--font path` and `--heading-font path` overrides and system-font discovery
as GL, with native font-selection pixel checks. Its resources are released before the SDL renderer and
window are destroyed. Platform input and frame timing stay in each backend;
the shared composition uses only LambUI APIs.

Run bounded native checks and capture all views:

```powershell
.\build\examples\opengl_glfw\lambui_example_opengl.exe --smoke-test --screenshot build/widgets-legacy
.\build\examples\opengl33_glfw\lambui_example_opengl33.exe --smoke-test --screenshot build/widgets-gl
.\build\examples\sdl3\lambui_example_sdl3.exe --smoke-test --screenshot build/widgets-sdl
```

The shared smoke sequence injects tab/button clicks, tree selection and
expansion, wheel scrolling, context-menu actions, and Escape dismissal. It also
checks checkbox keyboard activation, radio exclusivity, text entry, dropdown
selection, window policies, drag/resize, title buttons, and close/reopen.
GL33 additionally verifies its renderer and animated shader pixels. Legacy GL
checks scaled/nested/empty scissors, font selection/fallback, nonblank rendering,
and changed pixels across views. SDL also checks nonblank rendering and changed
pixels. Legacy GL keeps input/layout in window coordinates and converts scissors
to framebuffer pixels, including on high-DPI displays.
The screenshot prefix produces desktop/compact assets, menu, job, tooltip,
controls, inputs, canvas, window settings, normal/minimized/maximized window
captures (`.ppm` for GL, `.bmp` for SDL). GL33 runs at 1100x720 and 420x720;
legacy GL and SDL at 920x680 and 420x680. These tests require a native desktop renderer
and remain separate from headless CTest.

## Lua bindings

`lua/` builds `lambui_lua` (`LambUI::lua`), a manual Lua C API binding layer exposing a
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
consumers who only want the core C++ library don't pay for fetching Lua.

Link the companion target with `target_link_libraries(my_game PRIVATE LambUI::lua)`
when consuming the source project. The host-facing API now accepts `lua_State*`:

```cpp
#include <LambUILua/LuaBindings.h>
#include <memory>
#include <stdexcept>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

void RunUI(LambUI::UIManager& manager) {
  std::unique_ptr<lua_State, decltype(&lua_close)> lua(luaL_newstate(), lua_close);
  if (!lua) throw std::runtime_error("Could not create Lua state");
  luaL_openlibs(lua.get());
  LambUILua::LuaUIBindings bindings(lua.get(), manager);
  if (luaL_dostring(lua.get(), "UI.CreateFrame('Frame', 'Panel')") != LUA_OK)
    throw std::runtime_error(lua_tostring(lua.get(), -1));
  // Run the host's UI loop here while bindings, Lua and manager are alive.
}
```

The binding borrows the main Lua state and manager; **destroy the binding before
either one**, and use them on the same thread. Lua userdata do not own widgets.
Destroying the binding releases Lua registry references and disables its retained
callbacks; old userdata/factory calls then raise Lua errors rather than accessing
the manager. One binding per Lua state is supported. Coroutine scripts may call
the API, but event callbacks run as non-yielding protected calls on the main state.
Callback failures are reported through `UILog` and leave the Lua stack balanced.
`SetScript(name, nil)` clears a button script.

The existing Frame/Button/StatusBar/EditBox factory and Texture/FontString methods
remain available. Widget-specific methods reject incompatible widget types;
automatic sol2 type-registration globals are no longer provided. Full binding
coverage of newer widgets remains separate work. Gameplay event payloads retain
the previous pointer-as-integer convention; the binding does not dereference or
marshal arbitrary C++ payload objects. Texture handles likewise use integer IDs.
