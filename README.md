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

### Images

`UIImageWidget` displays an image source through a host-provided
`UIImageLoader`, without graphics-API or file-decoder dependencies. The loader
receives the source string and returns `UIImage{textureHandle, width, height}`;
the host owns and caches the texture and must keep it valid while in use.
Return an empty image on load failure. `SetSource` returns whether loading
succeeded; an empty source, missing loader, null handle, or nonpositive image
dimensions clears the image. Loader exceptions propagate to C++ callers and
become Lua errors through the bindings.

```cpp
auto* image = manager.GetRoot().CreateChild<LambUI::UIImageWidget>("Artwork");
image->SetImageLoader(loadImage);
image->SetAllPoints(&manager.GetRoot());
image->SetSource("assets/artwork.png");
```

`SetFit(ImageFit::Contain)` (the default) centers the entire image inside its
layout box with preserved aspect ratio. `Cover` fills the box and crops UVs
symmetrically; `Stretch` fills it without preserving aspect ratio. Fitting
follows layout changes without reloading. `SetTint` applies packed RGBA color.
The widget's layout and hit-test bounds remain the full box.

For Lua, call `LuaUIBindings::SetImageLoader(loadImage)` before running scripts.
New image widgets copy that loader; changing it does not replace loaders on
existing widgets. In C++, `UIImageWidget::SetImageLoader` reloads its current
source. Captured host services must outlive the widgets using them.

```lua
local image = UI.Root:CreateImage("Artwork")
image:SetAllPoints(UI.Root)
image:SetFit("CONTAIN")
local loaded = image:SetSource("assets/artwork.png")
```

`UI.CreateFrame("Image", name, parent)` is also supported. Lua exposes
`GetSource`, `IsLoaded`, `GetFit`, and `SetTint`; fit strings are `CONTAIN`,
`COVER`, and `STRETCH`. Use `UITextureWidget` when the host already provides a
raw texture handle, or `UICanvasWidget` for custom rendering callbacks.

Preload button artwork once with `UI.LoadImage(path)`, then swap the returned
opaque image handles directly on a single button:

```lua
local normalImage = assert(UI.LoadImage("assets/button.png"))
local hoverImage = assert(UI.LoadImage("assets/button_hover.png"))
local button = UI.CreateFrame("Button", "Action")
button:SetSize(260, 110)
button:SetPoint("CENTER", UI.Root, "CENTER", 0, 0)
button:SetButtonColors(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF)
assert(button:SetImage(normalImage))
button:SetScript("OnEnter", function()
  assert(button:SetImage(hoverImage))
end)
button:SetScript("OnLeave", function()
  assert(button:SetImage(normalImage))
end)
```

`UI.LoadImage` uses the current binding's host loader and returns `nil` for
an empty source, absent loader, or invalid image. Loader exceptions become Lua
errors. `button:SetImage` returns `true` and changes only the texture handle;
it does not load or upload a texture, resize the button, or change its colors.
Artwork stretches to the button bounds and is multiplied by its state color;
use white state colors to preserve the original artwork. Handles can be shared
between buttons within the same binding, but cannot be used after that binding
expires. Invalid handle arguments raise Lua errors. The host retains ownership
of the textures and must keep them valid while assigned to widgets, even if
the Lua image handle has been garbage-collected.

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

### Hover callbacks and native pointers

Applications can already register enter/exit handlers on any widget. C++ names
these `OnMouseEnter` and `OnMouseLeave`; Lua uses `OnEnter` and `OnLeave`:

```cpp
button->RegisterCallback(LambUI::UIEventType::OnMouseEnter,
  [](const LambUI::UIEventData& event) {
    OnButtonEntered(event.mouseX, event.mouseY);
  });
button->RegisterCallback(LambUI::UIEventType::OnMouseLeave,
  [](const LambUI::UIEventData&) { OnButtonExited(); });
```

```lua
button:SetScript("OnEnter", function() OnButtonEntered(button) end)
button:SetScript("OnLeave", function() OnButtonExited(button) end)
```

The `OnButtonEntered`/`OnButtonExited` functions above are application hooks,
not LambUI functions: animation, audio, or other application behavior stays in
the host. Handlers run synchronously on the injecting/updating UI thread,
after the control's own state handling, once per hover transition rather than
on every move. Keep captured application objects alive, or detach handlers
with `RegisterCallback(type, {})` / `SetScript(name, nil)`; registration replaces
the previous handler for that event type.

Hover targets the topmost mouse-enabled widget and does not bubble to parents.
Use `SetMouseEnabled(false)` on decorative children (such as button labels) so
the containing control receives hover. `Update` refreshes hover after visibility
or layout changes. During mouse capture, hover stays on the pressed widget
until release; no enter/leave notifications fire for widgets underneath a drag.

Hosts should forward native window-leave notifications to
`UIManager::InjectMouseLeave()` and forward the current position through
`InjectMouseMove(x, y)` on re-entry. Leaving clears hover, or defers the leave
until release if a widget has capture.

After input and `Update`, query `UIManager::GetPointerShape()` and map it to a
cached native cursor. `PointerShape` contains `Arrow`, `ResizeEW`, `ResizeNS`,
`ResizeNWSE`, and `ResizeNESW`. Normal resizable windows use their actual edge
and corner hit regions; title buttons, contents, locked/minimized/maximized
windows use the arrow. The direction is retained throughout captured resizing.
Custom widgets may override `UIWidget::GetPointerShape(x, y, captured)`.

All four examples map these to GLFW/SDL system cursors, applying only changes
and releasing resources before host shutdown. Unsupported native shapes fall
back to the default arrow. Cursor handling stays entirely outside `IRenderer`
and the command bucket; the core never polls input or calls platform APIs.

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
Games can call `manager.SetPointerFocusHighlightsEnabled(false)` to hide focus
outlines and focus-only colors during mouse interaction while preserving logical
focus, hover/press feedback, and keyboard activation. Keyboard input restores the
highlights; actual pointer movement or a click hides them again. The default is
`true`, preserving focus highlights for both input methods.
Custom render overrides continue using `GenerateChildRenderCommands` or
`AppendChildRenderCommands` so children retain focus decoration.
Buttons show their pressed color while Enter/Space is held; releasing the matching
key activates once, and focus loss cancels. No default button is chosen implicitly.
Clipboard support remains backlog work.

Register a dialog's explicit action with
`manager.SetDefaultButton(dialog, button)`; `GetDefaultButton(dialog)` returns
the registered `UIButton*`. A dialog can be any widget in the manager's normal
tree, including a `UIWindow`. Registration returns false without changing the
existing choice for foreign widgets, buttons outside the dialog, or buttons
inside a nested window. The button must be a strict descendant of the dialog.
Passing `nullptr` clears the choice but retains a scope boundary, blocking outer
defaults. The nearest registered ancestor of the focused widget wins; an
unregistered window also blocks defaults outside that window.

Unmodified Enter in a single-line input uses an eligible default instead of
`OnEnterPressed`, including in read-only inputs. It shows the button's pressed
color without moving focus, then fires one bubbling `OnClick` on matching release.
Repeats are ignored. Focus changes, changed registrations, popup opening, or lost
eligibility cancel the pending action until release. Eligibility is checked on
input, update, and render; hidden ancestors and mouse/keyboard-disabled buttons
are excluded. With no eligible default, normal input submission is unchanged.

Multiline inputs, focused buttons, and other controls keep their own Enter
behavior. Space, Shift/Control+Enter, popups, and events without keyboard focus
never invoke a dialog default. Custom `IFocusable` widgets can opt into this
routing by overriding `CanUseDialogDefault()` to return true; its default is false.

```cpp
auto* dialog = manager.GetRoot().CreateChild<UIWidget>("SaveDialog");
auto* nameField = dialog->CreateChild<UIInputBox>("Name");
auto* save = dialog->CreateChild<UIButton>("Save");
manager.SetDefaultButton(*dialog, save);
```

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
Backspace, Delete, cut, paste, and multiline Enter without clearing the current selection.
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
behind the text, including explicit blank lines. Unicode grapheme/shaping
support remains separate; selection boundaries are codepoints, not graphemes.

`UITextWidget` offers the same selection property and range methods, but
selection defaults to **off**. Enable it with `SetSelectionEnabled(true)`;
labels configured as decorative with `SetMouseEnabled(false)` must also have
mouse input enabled to receive focus. Selectable labels support captured drag,
Shift-click, Shift+arrows/Home/End, Ctrl+A, and Ctrl+Home/End. They are copy-only:
typing, cut, paste, and Enter never change their text or activate a dialog default.
Selection persists on focus loss, while `SetText` resets it. Disabling selection
clears the range and removes the label from keyboard focus navigation.

Label highlights use the existing font, padding, and wrapped-line geometry,
with content and ancestor scissor clips. Labels keep their normal auto-sizing
or fixed wrapped bounds and do not gain an input border, caret, or internal
scrolling. Set the label's text measurer as usual for font-aware hit testing.
The shared Controls gallery's material description demonstrates selectable text.

### Clipboard

Clipboard access is supplied by the host, never polled from the core library:

```cpp
manager.SetClipboardCallbacks(
  [](std::string& utf8) { return HostReadClipboard(utf8); },
  [](const std::string& utf8) { return HostWriteClipboard(utf8); });
```

`HostReadClipboard` and `HostWriteClipboard` stand for your engine's synchronous
clipboard adapters. Pass valid UTF-8, return true on success, and keep captured
host resources alive while registered. Callbacks run on the injecting UI thread;
they should not mutate the UI. `SetClipboardCallbacks({}, {})` detaches them.
There is no internal clipboard fallback. GLFW and SDL3 examples install native
adapters in their platform source files; smoke tests use an in-memory clipboard
and never overwrite the OS clipboard.

Forward `ScanCode::C`, `X`, and `V` plus both Control keys through
`InjectKeyEvent` for Ctrl+C/X/V. Hosts can instead call `InjectCopy()`,
`InjectCut()`, and `InjectPaste()` directly, including for platform-specific
shortcuts. These operate only on eligible focus within the active popup scope
and return whether the operation succeeded. Key releases do not repeat edits.

Copy requires a nonempty selection and also works in read-only inputs. Cut
deletes only after a successful clipboard write; cut and paste require editing
enabled. Missing/failed callbacks and empty pastes preserve text and selection.
Paste replaces the selection or inserts at the cursor, including when selection
is disabled. CRLF/CR normalize to LF for multiline inputs and spaces for
single-line inputs; tabs become spaces and other ASCII control bytes are dropped.
Each successful paste or cut emits one `OnTextChanged`, never `OnEnterPressed`.
`UIInputBox::PasteText(utf8)` also accepts host-provided text directly, subject to
focus/editing checks and the same normalization, without accessing a clipboard.

### Known limitations (by design, for now)

- Anchors must reference a widget that has *already* been positioned this
  pass (typically its parent or an earlier sibling) — there's no
  general dependency-graph solver for arbitrary forward references.
- Text rendering uses a single-channel SDF atlas for printable ASCII, not
  full MSDF or Unicode shaping. The OpenGL 3.3 example samples it with
  derivative-based `smoothstep`; legacy OpenGL and SDL3 use approximations.
- The Vulkan example uses one frame in flight and a combined graphics/present
  queue. It favors readable synchronization over maximum throughput; textures
  are immutable uploads and remain renderer-owned until teardown.

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
| `LAMBUI_EXAMPLE_OPENGL` / `_OPENGL33` / `_VULKAN` / `_SDL3` | `ON` | Toggle individual examples (Vulkan skips without the SDK or enabled Lua bindings; requires `glslc`) |

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

Installed-package consumers also need fmt's CMake package; the fetched
dependency installs alongside the core. Boost is a soft, bring-your-own
dependency: by default `UIStyle::fillColor` and `UIWidget::GetStyle()` use
`std::optional` (requires C++17). Set `LAMBUI_USE_BOOST=ON` to use
`boost::optional`/`boost::none` instead (e.g. to stay on C++14, or because you
already depend on Boost elsewhere) — assignment, boolean checks,
dereferencing, `reset()` and `value_or()` behave the same either way.

## Examples

Each example under `examples/` builds a `LambUI::UIManager` demo widget tree
with its own `IRenderer` implementation and windowing/input glue:

- `lambui_example_opengl` — GLFW + legacy OpenGL 1.1 immediate mode (no
  loader dependency needed).
- `lambui_example_opengl33` — GLFW + OpenGL 3.3 core, GLAD, VAO/VBO triangles,
  and GLSL 330 shaders. An animated contour canvas has a distortion slider
  and pause/resume button; labels use antialiased SDF text.
- `lambui_example_sdl3` — SDL3 + its built-in 2D `SDL_Renderer` API.
- `lambui_example_vulkan` — GLFW + Vulkan quad/SDF renderer with a complete
  Lua-authored expedition planner, separate from the C++ widget gallery.

### Vulkan Lua application

[planner.lua](examples/vulkan/src/planner.lua) owns the Fieldwork application:
route selection, terrain waypoints, multiline briefings, readiness checks,
progress, pause/resume/reset, responsive layout, and status messages. Briefings
are saved in memory for this run; changing routes discards unsaved edits.
The 30-second survey is simulated, and the generated terrain is fictional.
No C++ demo widgets or shared WidgetShowcase are used.

The header's **Canvas** and **Notes** buttons open independent floating windows.
Canvas contains a Vulkan-rendered animated distortion field, with pause/resume,
phase reset, and a strength slider. Notes opens an editable scratchpad. **Arrange**
opens both in an overlapping layout; click either window to raise it, drag its
title, resize its edges/corners, or use its minimize/maximize/close buttons.
Closed windows reopen without losing scratchpad text. The header stays reachable
when a UI window is maximized. These are nonmodal LambUI windows, not OS windows;
the planner map remains static. Actual host-window resizing rearranges the popups
to fit, while selecting a route preserves their current bounds and stacking.

```powershell
cmake -S . -B build -DLAMBUI_BUILD_LUA_BINDINGS=ON -DLAMBUI_EXAMPLE_VULKAN=ON
cmake --build build --target lambui_example_vulkan
.\build\examples\vulkan\lambui_example_vulkan.exe
.\build\examples\vulkan\lambui_example_vulkan.exe --validation --smoke-test --screenshot build/vulkan-planner
```

Requires the Vulkan SDK (`glslc` compiles GLSL to SPIR-V), a Vulkan-capable driver,
and a desktop display. `--validation` additionally requires the SDK's Khronos
validation layer and fails clearly when unavailable. The example is skipped
when Lua bindings are disabled, leaving the core's optional dependency unchanged.
Use `--font path.ttf`, `--heading-font path.ttf`, `--width 420 --height 780`, or
`--script path.lua` to override defaults. Minimum window size is 360x480.
Without smoke mode, `--screenshot prefix` captures one frame to `prefix.ppm`
and exits. Default shaders/script are loaded from the CMake build directory;
this executable is a build-tree example, not a relocatable installed bundle.

The host exposes opaque `Host.map` and `Host.headingFont` handles, plus
`Host.DrawField(time, strength)`, which is valid only inside a canvas render callback.
The field uses a dedicated Vulkan pipeline; bounds and inherited clipping come
from the active canvas, not from Lua-supplied coordinates. A replacement
script creates global `App` with `Resize(width, height)` and `Update(seconds)`
functions; calls are protected and errors exit the host. Scripts are trusted local
code with standard Lua libraries enabled, not sandboxed downloads.
The C++ host owns Vulkan, fonts, the generated terrain bitmap, frame timing, GLFW
input translation, and clipboard callbacks. It destroys the binding before Lua
and UIManager, and destroys Vulkan resources before the GLFW window.

The renderer consumes one command bucket between `BeginFrame` and `EndFrame`:
RGBA/UV quads, alpha blending, font-handle selection with default fallback,
four-sample derivative-smoothed SDF coverage, nested/empty framebuffer-scaled
scissors, and ordered custom callbacks. Native callbacks execute inside the render
pass; `GetCommandBuffer()` is valid only during an active frame. Callbacks must
respect the active clip and may not end the pass or recursively submit UI; the
renderer restores its pipeline, descriptors, viewport, scissor and vertex binding
before subsequent UI draws. Borrowed font atlases must outlive the renderer.
`UploadTexture` takes RGBA8 pixels before `BeginFrame`; returned handles are local
to that renderer. Null means solid white; at most 128 textures (including white
and font atlases) are retained. No texture release/update or batching API yet.

Swapchain recreation waits for idle, handles out-of-date/suboptimal results,
and defers minimized/zero-sized surfaces. One fence protects the reusable upload
buffer; presentation semaphores belong to individual swapchain images. GPU smoke
checks cover alpha/UV pixels, nested/disjoint/empty scissors, SDF coverage/font
fallback, callback state restoration, Lua input workflows and 1100x780, 420x780,
360x480 resizing with captures. Readback requires surface transfer-source support
and an RGBA/BGRA8 format; otherwise capture fails explicitly. Native tests remain
separate from headless CTest. OS clipboard round-trip, non-1x DPI, and native
host-window minimize/restore interaction are not automated by this smoke sequence.
The floating UI windows are tested: canvas motion/pause/strength pixels, exact
overlap-region equality when raising each window, drag/resize, title-button
minimize/maximize/restore/close and reopening. Additional captures use
`-windows-`, `-canvas-front-`, `-canvas-resized-`, `-canvas-minimized-`, and
`-canvas-maximized-` with desktop/compact/minimum size suffixes.

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

#### Rendering performance

GL33 coalesces consecutive quads and font glyphs with matching texture and
shader state into instanced draws, capped at 4,096 quads per batch. Tint is
per instance, so differently colored shapes can share a draw. Painter order
is unchanged: texture/shader changes, scissors, and custom callbacks split
batches. Callbacks still execute every frame, including animated canvases.

Each quad uses a 48-byte bounds/UV/color instance record. The renderer retains
batch buffers and uploads only batches whose instance data changed; unchanged
frames have zero geometry uploads but still draw into the host's framebuffer.
Uniform and texture-content changes do not require geometry uploads. Unused
batch buffers are released when the command list shrinks. Per-submission
`GetDrawCallCount()`, `GetQuadCount()`, and `GetUploadCount()` expose the work.
This is backend command-data dirty tracking, not logical-tree caching:
`UIManager` continues generating commands each frame so existing custom widgets
and dynamic visual state need no new invalidation contract. `IRenderer` is
unchanged; legacy GL, SDL3, and Vulkan retain their existing submission paths.

The platform-independent `TextureAtlas` packs tightly packed, top-down RGBA8
images into fixed-size pages. Every image has a one-pixel extruded gutter for
linear filtering without neighboring-image bleed. Regions remain stable for
the page lifetime; there is no repacking, eviction, resizing, or mipmapping.
The shelf packer can leave unused space. A full page returns `boost::none`;
allocate another page when needed. Invalid page dimensions throw
`std::invalid_argument`; image dimensions/data are validated before insertion.

```cpp
TextureAtlas icons(256, 256);
auto region = icons.AddImage(imageWidth, imageHeight, rgbaBytes);
if (!region) throw std::runtime_error("Atlas page full or invalid image");
void* texture = renderer->UploadTextureAtlas(icons); // GL33 example adapter
if (!texture) throw std::runtime_error("Atlas upload failed");
imageWidget->SetTexture(texture);
imageWidget->SetUVRect(region->u0, region->v0, region->u1, region->v1);
```

`UpdateImage(region.id, rgbaBytes)` replaces same-size content without moving
UVs. `GetRevision()` advances only on successful additions or changed pixels;
failed operations and identical updates leave it unchanged. Other hosts upload
`GetPixels()` as RGBA8 using `GetWidth()`/`GetHeight()` and their own opaque
texture handle. No GPU allocation happens in the core atlas.

GL33's `UploadTextureAtlas` creates an owned texture or updates the existing
one only when its revision changes; call it after CPU edits and before render
submission. `GetTextureUploadCount()` is cumulative. Atlas objects must remain
alive at a stable address while registered. Stop using the texture handle and
call `ReleaseTextureAtlas` before destroying the atlas, or keep the atlas alive
until the renderer is destroyed. Font atlases retain their separate SDF format.

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
Performance checks compare batched translucent pixels against individual draws,
require 100 compatible quads and multi-glyph strings to use one draw, and verify
unchanged/partially changed uploads, callback barriers, batch overflow, and empty
submissions. RGBA atlas checks cover shared draws, edge sampling, stable handles,
and revision-based texture updates.
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
Vulkan uses the separate Lua-authored Fieldwork application described above.

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
`SetScript(name, nil)` clears a widget script. Scripts receive no arguments;
query the widget's current value/text from the callback. Supported names are
`OnClick`, `OnEnter`, `OnLeave`, `OnValueChanged`, `OnTextChanged`, and
`OnEnterPressed`, `OnClose`, and `OnWindowStateChanged`; callbacks use the core's
existing bubbling rules.

The Frame/Button/StatusBar/EditBox factory and Texture/FontString methods remain
available. `StatusBar` retains its historical slider behavior; the new
`ProgressBar` is noninteractive. The factory also supports `CheckBox` and
`ScrollContainer`, `Window`, `DropDownBox`, and `Canvas`. The Vulkan application uses these additional methods:

- Base widgets: `ClearPoints`, `GetRect` (resolved x/y/width/height after Update),
  `SetMouseEnabled`, `SetTooltip`, and `SetBackgroundColor` (a simple solid UIStyle).
- Buttons: `SetButtonColors(normal, hover, pressed)` with packed `0xRRGGBBAA` colors.
- Text/EditBox: `SetText`, `GetText`, `SetFont(integerHandle)`, `SetWordWrap`.
  New FontStrings automatically borrow the manager's text measurer.
  Hosts can also supply `LuaUIBindings::SetFontResolver` to enable
  `SetFont("Inter-Bold", 32)` or `SetFont("Inter-Bold")`. The resolver receives
  the requested name and pixel size (zero when omitted) and returns a registered
  font handle. Missing resolvers, unknown faces/sizes, and invalid sizes raise
  Lua errors; numeric handles remain supported. Register each resolved handle
  with both the renderer and text measurer so drawing and layout agree.
- EditBox: `SetMultiline`, `SetEditingEnabled`; existing injected selection,
  keyboard editing and host clipboard shortcuts work without Lua injection APIs.
- CheckBox: `SetText`, `GetText`, `SetChecked`, `IsChecked`.
- DropDownBox: `SetOptions({"First", "Second"})`, `SetSelectedIndex(index)`,
  `GetSelectedIndex()`. Lua indices are one-based; an empty list returns zero.
  `SetFont(integerHandle)` or `SetFont("Inter-Bold", 20)` applies to the selected
  value and all option labels, including options replaced later. Named fonts
  use the same host resolver and optional size as Text/EditBox.
  Selection changes fire `OnValueChanged`, including programmatic selections.
  Lists use a scrollable overlay outside parent clipping, opening above the
  control when necessary. C++ callers can opt into this behavior with the
  `UIDropDownBox(UIManager&, name)` constructor.
- ProgressBar: `SetMinMaxValues`, `SetValue`, `GetValue`, `SetProgressColors(background, fill)`.
- ScrollContainer: `GetContent`, `SetContentSize`, `SetScrollOffset`; create
  scrolling children under `GetContent()`, not directly under the viewport.
- Window: `SetTitle`, `SetBounds(x,y,width,height)`, `SetSizeLimits(minWidth,
  minHeight,maxWidth,maxHeight)`, `SetMovable`, `SetResizable`, `GetContent`,
  `Minimize`, `Maximize`, `Restore`, `Close`, and `GetWindowState` ("Normal",
  "Minimized", or "Maximized"). `BringToFront` is available on all widgets.
- Canvas: `SetRenderCallback(function(x,y,width,height) ... end)` receives its
  resolved logical rectangle during command submission. Call a host-provided
  native draw function there; do not mutate the UI tree or recursively render.
  Passing nil clears the callback. Registry ownership, protected errors, and
  binding detachment follow the same rules as event scripts.

Widget-specific methods reject incompatible widget types;
automatic sol2 type-registration globals are no longer provided. Full binding
coverage of newer widgets remains separate work. Gameplay event payloads retain
the previous pointer-as-integer convention; the binding does not dereference or
marshal arbitrary C++ payload objects. Texture handles likewise use integer IDs.
