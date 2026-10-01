# LambUI TODO (as of 2026-10-01)

Baseline: core architecture (dual-tree, anchor layout, IRenderer HAL, input
injection, event routing, Lua/sol2 bindings) is fully implemented and builds
clean. Tests pass (63 GoogleTest cases; optional Lua disabled in this build).
See goals.txt for full architecture vision.

## High priority (blocks core use cases)
- [x] SDF font rendering pipeline (goals.txt item 5)
  - Added `LambUI::FontAtlas` (include/LambUI/UIFontAtlas.h, src/UIFontAtlas.cpp):
    loads a TTF/OTF file via stb_truetype (FetchContent'd, private to the core
    lib - never appears in a public header), bakes a single-channel SDF atlas
    for printable ASCII via `stbtt_GetCodepointSDF`, and exposes glyph
    metrics/UVs as plain data (no graphics headers cross the HAL boundary).
  - Added `FontAtlasTextMeasurer : ITextMeasurer` (same header) - a concrete,
    working text measurer backed by the atlas.
  - GL example: uploads the atlas as a `GL_INTENSITY` texture and renders
    glyphs with `GL_ALPHA_TEST` (the classic pre-shader SDF technique - GL 1.1
    fixed-function has no programmable stage to do a real smoothstep).
  - SDL3 example: uploads the atlas as an `SDL_Texture`, with distance values
    sharpened into a crisp alpha mask at upload time (SDL_Renderer has no
    shader stage either, so this is a static approximation, not a true
    per-pixel smoothstep).
  - Both examples now render an actual "Hello, LambUI!" `UITextWidget` label
    instead of leaving text widgets unexercised; gray placeholder boxes are
    gone.
  - Note/follow-up: this is single-channel SDF (sharp corners can round
    slightly at large scale), not full multi-channel MSDF, and the Vulkan
    backend still has no quad/text rendering at all (see below) so it wasn't
    touched here. True shader-based smoothstep sampling is now demonstrated
    in the separate OpenGL 3.3 example below; the legacy GL and SDL3
    backends retain their original approximations.
- [x] Scrollable containers
  - `UIScrollContainer` pans explicit-size content, reclamps on viewport
    layout changes, and emits balanced PushScissor/PopScissor commands.
  - Hit testing excludes hidden/clipped descendants; wheel injection routes
    to the nearest scrollable ancestor (positive pixel deltas move left/up).
  - GL/SDL3 examples forward wheel input and intersect/restore nested clips.
  - Full MSVC/Ninja build and all 15 tests pass (7 scroll-container tests).
  - SDL3 clipping fix (2026-10-01): failed/disjoint intersections now become
    zero-area clips instead of negative extents that disable SDL clipping.
    Native pixel regressions cover all four offscreen directions, zero area,
    text/canvas drawing, deeper nesting, and parent-clip restoration. Desktop
    and compact smoke checks pass; full build and all 53 CTest cases pass.
- [x] Add reusable visual scrollbars to `UIScrollContainer`.
  - Default-enabled horizontal/vertical overlay tracks, proportional thumbs,
    captured dragging, track-click paging, and automatic hiding when content fits.
  - `SetScrollbarsEnabled` hides bars without disabling wheel/programmatic
    scrolling. Twelve-pixel overlays preserve existing viewport sizes/ranges;
    twenty-pixel minimum thumbs are capped to the available track.
  - Internal child widgets reuse injected input, event consumption, and clipped
    HAL quad commands across Window settings, control galleries, and tree views.
  - Full MSVC/Ninja build and all 49 CTest cases pass (12 scroll-container tests).
    GL33 1100x720/420x720 and SDL3 920x680/420x680 native smoke checks pass;
    desktop/compact captures inspected. Optional Lua remains disabled.
- [x] Event bubbling
  - `UIWidget::FireEvent` walks logical parents until `UIEventData::handled`
    is set; existing const-reference callback signatures remain compatible.
  - Mouse down/up/click and value/text/enter-pressed events bubble; mouse
    enter/leave stay target-local. Capture/focus and interface-routed wheel,
    keyboard, and character input are unchanged.
  - Controls consume press/release; dropdowns consume toggle/selection clicks
    so selecting an option cannot reopen the menu. Same-widget callbacks run.
  - Added 8 regression tests. Full MSVC/Ninja build and all 23 CTest cases
    pass; optional Lua bindings remain disabled in this build.
- [x] More compound widgets: ProgressBar, MenuBar, TabControl, TreeView,
      context menu, tooltip system.
  - Added UIProgressBar, UIMenuBar, UITabControl, UITreeView, UIContextMenu,
    and UITooltip; public headers exported through LambUI.h.
  - Retained tab pages; scrollable tree selection/expansion; menu actions,
    disabled items, separators, reusable rows, and scrolling.
  - Manager-owned overlays render above normal content/clips, clamp to the
    viewport, and dismiss on outside click/Escape without click-through.
    Tooltips use injected frame time and never capture mouse input.
  - Full MSVC/Ninja build and all 34 CTest cases pass (11 new regressions).
    Native visual testing subsequently passed in the GL33/SDL3 showcases
    below; Lua remains disabled.
  - Follow-ups: nested submenus and multiline tooltips; keyboard navigation
    and Lua exposure remain separate backlog items.
- [x] Checkboxes, radio buttons, and configurable windows (user-requested).
  - UICheckBox: checked/enabled state, labels, injected click and Space/Enter.
    UIRadioButton: exclusive groups scoped to siblings; settled-state callbacks.
  - UIWindow: clipped client content, click-to-front, title dragging and
    edge/corner resizing with independent locks and size limits.
  - Minimize/maximize/restore and close-as-hide; each title button independently
    Enabled/Disabled/Hidden. Dedicated OnWindowStateChanged and OnClose events.
  - Mouse capture pairs buttons and ignores hidden targets. Pending geometry
    survives immediate state transitions; dragging back to origin is exact.
  - Dropdown options now show labels and safely reuse rows after replacement.
  - Full MSVC/Ninja build and all 44 CTest cases pass (10 new regressions).
- [x] Multiple fonts
  - `FontAtlasTextMeasurer::RegisterFont` maps opaque handles to borrowed
    atlases; null/unknown handles use the default, duplicates are rejected.
  - `UITextWidget` and `UIInputBox` expose SetFont/GetFont. Text widgets
    remeasure existing text on font/measurer changes; input sizes stay fixed.
  - GL, GL33, and SDL3 select per-handle textures. GL33/SDL3 show a separate
    heading face with --heading-font; GPU resources are released on teardown.
  - Four regressions added. Full MSVC/Ninja build and all 53 CTest cases pass.
    GL33/SDL3 desktop/compact native metrics/pixel/fallback checks pass;
    screenshots inspected. Lua remains disabled; compound labels use default.
- [x] Keyboard navigation (Tab/Shift-Tab focus cycling, arrow keys).
  - Injected tree-order focus cycling skips hidden/disabled controls, reveals
    scrolled targets, scopes popups, and restores focus on dismissal.
  - Button activation; arrows for sliders, dropdowns, tabs, trees, menus,
    and radio groups; UTF-8 input cursor movement/insertion/deletion.
  - GL/GL33/SDL3 key mappings; ten new routing regressions. Full MSVC/Ninja
    build and all 63 CTest cases pass; desktop/compact GL33/SDL3 native smoke
    checks pass. Lua remains disabled; focus rings/blinking cursors stay separate.
- [ ] Input text boxes should show a blinking cursor 
- [ ] Margin/padding system (currently only anchor point + offset).
- [ ] Min/max size constraints, aspect-ratio preservation, relative
      ("% of parent") sizing.
- [ ] styling, rounded corners, shadows, fill colors/patterns    

## Medium priority (common game UI patterns)
- [ ] Expand Lua bindings: UIInputBox, UIDropDownBox, UICanvasWidget,
      UIControl are not exposed to Lua yet (lua/src/LuaBindings.cpp only
      covers Widget/Button/StatusBar/FontString/Texture).
  - Also expose the new compound widgets, checkbox/radio/window APIs and
    window events, and tooltip properties.
  - `UI.CreateFrame` factory only supports "Frame", "Button", "StatusBar",
    "EditBox" — missing "DropDown", "Canvas" etc.
- [x] Implement `ITextMeasurer` concretely in at least one example backend
      (needed before text auto-sizing / wrapping can be tested end-to-end).
  - Done as part of the SDF font work above: `FontAtlasTextMeasurer` is wired
    into both the GL and SDL3 examples via `UIManager`'s textMeasurer ctor
    arg, and `UITextWidget::SetText` auto-sizes against it.
- [ ] Text wrapping + multiline text input (UIInputBox is single-line only).
- [ ] Vulkan renderer backend (examples/vulkan)
  - Swapchain + frame loop wired up, but `VulkanExampleRenderer::
    SubmitRenderCommands` only logs commands — no render pass/pipeline,
    no quad or text drawing implemented yet.

## Lower priority (polish)
- [x] Add a standalone OpenGL 3.3 shader example alongside existing examples
      (user-requested, 2026-10-01).
  - `examples/opengl33_glfw`: GLFW core context, pinned GLAD loader,
    GLSL 330 / VAO / VBO renderer, textured quads, derivative-smoothed SDF
    text, high-DPI nested scissors, and custom canvas commands.
  - Animated contour shader with injected pause/resume and distortion
    controls; existing examples and the core HAL remain unchanged.
  - Full MSVC/Ninja build and all 15 CTest cases pass. Native GPU smoke
    checks and screenshot inspection pass at 1100x720 and 420x720.
- [x] Showcase compound widgets in example applications (user-requested,
      2026-10-01).
  - OpenGL 3.3 retains its animated canvas and adds an asset inspector;
    SDL3 now has a resizable asset browser with real frame-time updates.
  - Shared example-only WidgetShowcase composes all six widgets: tree,
    tabs, progress, menu bar, context menus, and delayed tooltips.
  - Both demos support --font, --smoke-test, and --screenshot; bounded
    checks exercise input, scrolling, actions, overlays, and rendered pixels.
  - Full MSVC/Ninja build and all 34 CTest cases pass. Native smoke and
    assets/menu/job/tooltip screenshot checks pass at 1100x720 and 420x720
    (GL33), and 920x680 and 420x680 (SDL3).
  - Core library, legacy GL, and Vulkan examples remain unchanged.
- [x] Expand GL33/SDL3 demos to showcase all controls and window settings
      (user-requested).
  - Shared Controls and Window tabs add selection controls, disabled state,
    slider, input field, labeled dropdown, and backend-rendered canvases.
  - Preview window supports live move/resize locks and per-button On/Off/Hide
    settings; reopening remains available outside the window.
  - Native smoke passes GL 1100x720/420x720 and SDL 920x680/420x680, including
    policy switches, drag/resize, title buttons, text/selection input, and pixels.
    Captures cover controls/inputs/canvas/settings/normal/minimized/maximized;
    desktop and compact screenshots inspected. Full build and 44 tests pass.
- [ ] Animation/tween framework.
- [x] Reduce font aliasing in the OpenGL 3.3 example (user-reported,
      2026-10-01).
  - Widened derivative-based SDF edge smoothing without changing text size,
    atlas metrics, the core library, or other backends.
  - Native smoke checks now cover normal-size text at 1x/1.5x/2x display
    scales and integer/half-pixel positions, requiring soft edges and solid
    strokes. Desktop/compact screenshot comparisons show smoother text.
  - Full MSVC/Ninja build and all 34 CTest cases pass.
- [ ] Nested submenus and multiline tooltips.
- [ ] Focus ring visualization; default-button Enter-key highlight.
- [ ] Text selection / copy-paste.
- [ ] Rendering perf: batching/instancing, texture atlas management,
      command buffer dirty-tracking (currently one draw call per command).
- [ ] Software renderer backend
  - user provides a custom framebuffer

## Notes
- Full status assessment generated via Explore subagent on 2026-09-30;
  re-run a similar sweep periodically to keep this list current since it's
  based on a point-in-time code read, not continuously verified.
