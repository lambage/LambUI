# LambUI TODO (as of 2026-10-01)

Baseline: core architecture (dual-tree, anchor layout, IRenderer HAL, input
injection, event routing, manual Lua C API bindings) is fully implemented and
builds as C++14. Tests pass (120 GoogleTest cases; optional Lua enabled).
See goals.txt for full architecture vision.

## High priority (blocks core use cases)
- [x] C++14 compatibility and remove sol2 (user-requested).
  - Boost.Optional preserves optional style/layout values; Boost.Algorithm
    replaces clamps. Removed C++17 syntax and example filesystem usage.
  - Manual Lua C API bindings retain the existing scripting surface, with
    checked userdata, protected callbacks, registry cleanup and detachment.
    Host API takes lua_State*; binding must die before Lua and UIManager.
  - Full MSVC/Ninja C++14 build and all 101 tests pass (8 new Lua tests).
    GL33/SDL3 desktop/compact native smoke checks pass. Clean core/Lua build,
    fetched Boost installation and exact-C++14 installed core consumer pass.
  - README covers Boost dependencies and Lua API/lifetime migration. Newer
    widget bindings remain below; the pinned Lua wrapper emits a CMake
    deprecation warning but configures/builds successfully on CMake 4.
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
  - Nested submenus, multiline tooltips, and keyboard navigation are completed
    below; Lua exposure remains a separate backlog item.
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
- [x] Input text boxes should show a blinking cursor.
  - Focused inputs emit a clipped caret at the insertion point; injected
    Update(deltaTime) seconds toggle visibility every 0.5s. Focus, editing,
    and cursor navigation restart the visible phase.
  - Uses the manager's text measurer and selected font, with a codepoint-based
    fallback; long text shifts to keep the caret inside the input bounds.
  - Four new regressions cover timing, editing, font metrics, UTF-8 fallback,
    clipping, tiny fields, and hidden focus. Full MSVC/Ninja build and all
    67 CTest cases pass. Native visual testing not run; Lua remains disabled.
- [x] Margin/padding system.
  - Nonnegative per-edge UIInsets; parent anchors use padded content bounds,
    margins offset widget anchors. Padding-only edits refresh child layout.
  - Scroll ranges, scrollbar geometry, render clips, and injected child hits
    use the padded viewport; default unpadded behavior remains unchanged.
- [x] Min/max size constraints, aspect-ratio preservation, relative
      ("% of parent") sizing.
  - Base-widget min/max constrain explicit, relative, and stretched sizes;
    minimum wins conflicting limits. Aspect ratio fits inside the solved box;
    incompatible limits win over ratio, preserving the first anchor.
  - Relative fractions use available parent content minus margins; SetSize
    restores pixel sizing. README documents precedence and window limits.
- [x] Styling, rounded corners, shadows, fill colors/patterns.
  - Opt-in UIStyle/ClearStyle; optional background color preserves control
    state colors when unset. Solid/checker/stripe fills retain texture UVs.
  - Rounded pixel strips and layered shadow falloff emit existing HAL quads;
    bounded tessellation, no renderer changes or core platform dependencies.
    Background-only rounding; child clipping and hit tests stay rectangular.
  - Shared GL33/SDL3 Build tab demonstrates padding, responsive constrained
    sizing, patterned fills, rounded buttons, and shadows. Eight regressions;
    full MSVC/Ninja build and all 75 CTest cases pass. Native smoke and capture
    inspection pass at desktop/compact sizes in both backends; Lua disabled.

- [x] Make dropdowns and tab headers more identifiable (user-requested).
  - Dropdowns have a separated chevron area, up/down open-state indicator,
    hover/press fills, focus/open outline, and clipped selected-label space.
  - Tabs have a distinct header strip, separated/inset inactive headers,
    selected top accent, inactive bottom edges, and keyboard-focus accent.
    Existing page geometry and mouse/keyboard behavior are unchanged.
  - Three new regressions cover state cues, selection, clipping, and tiny
    bounds. Full MSVC/Ninja build and all 78 CTest cases pass; GL33/SDL3
    desktop/compact native smoke and screenshot inspection pass. Lua disabled.

## Medium priority (common game UI patterns)
- [x] Implement `ITextMeasurer` concretely in at least one example backend
      (needed before text auto-sizing / wrapping can be tested end-to-end).
  - Done as part of the SDF font work above: `FontAtlasTextMeasurer` is wired
    into both the GL and SDL3 examples via `UIManager`'s textMeasurer ctor
    arg, and `UITextWidget::SetText` auto-sizes against it.
- [x] Text wrapping + multiline text input.
  - Shared UTF-8 line layout handles word wrapping, long words, explicit
    CR/LF/CRLF breaks, and blank lines using selected-font metrics.
  - UITextWidget SetWordWrap uses fixed content bounds with clipping;
    unwrapped labels retain intrinsic multiline sizing.
  - UIInputBox SetMultiline enables Enter/newline editing, visual-line
    Home/End and Up/Down navigation, wrapping, and clipped caret scrolling.
    Single-line submission remains the default; SetWordWrap can disable wrap.
  - Seven regressions added. Full MSVC/Ninja build and all 85 CTest cases
    pass; GL33/SDL3 desktop/compact native smoke and multiline screenshot
    inspection pass. Controls tab demonstrates both widgets. Lua disabled.
- [x] Text selection (enable/disable property on UIInputBox).
  - Default-enabled click/drag and Shift selection, Ctrl+A, UTF-8 range APIs,
    replacement editing, clipped highlights, and injected-time autoscrolling.
  - Six regressions added. Full MSVC/Ninja build and all 91 CTest cases pass;
    GL33/SDL3 desktop/compact smoke and selection captures pass. Lua disabled.
- [x] Text input editing enable/disable property (user-requested).
  - UIInputBox SetEditingEnabled/IsEditingEnabled defaults to enabled.
    Read-only blocks user edits but retains selection, navigation, application
    SetText updates, and single-line Enter submission. Selection is independent.
  - Two regressions added. Full MSVC/Ninja build and all 93 CTest cases pass;
    GL33/SDL3 desktop/compact read-only smoke checks pass. Lua disabled.
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
- [x] Add the shared widget showcase to legacy OpenGL 1.1 (user-requested).
  - Reuses WidgetShowcase with 20px body/26px heading fonts, all controls,
    menus, scrollbars, and movable/resizable preview windows. Fixed-function
    material canvas; real frame timing; font overrides and screenshot CLI.
  - Corrected window/framebuffer scissor scaling. Native checks cover nested,
    disjoint/empty clips, font selection/fallback, and captured view changes.
  - Full MSVC/Ninja build and all 101 CTest cases pass. Native GPU smoke and
    capture inspection pass at 920x680 and 420x680. Core/HAL unchanged;
    legacy alpha-tested text remains available for renderer comparisons.
- [x] Reduce font aliasing in the OpenGL 3.3 example (user-reported,
      2026-10-01).
  - Follow-up: thin strokes changed brightness after one-pixel window moves,
    reproduced at native 1:1 framebuffer/content scale (not a DPI mismatch).
    Replaced sampled-distance derivatives with a stable UV pixel footprint
    and four half-pixel coverage samples; linear texture filtering retained.
  - FontAtlas exposes its baked distance scale as plain metadata. Text size,
    atlas pixels/metrics, other backends, and HAL/platform boundaries unchanged.
  - Native regression checks isolated "i" and "Material / Contour" pixel
    stability under X/Y/diagonal moves at 1x/1.25x/1.5x/2x, plus fractional
    placements and coverage against a 4x reference. Old shader fails movement.
  - Full MSVC/Ninja build and all 101 CTest cases pass (Lua enabled).
    Desktop/compact GL33 GPU smoke and screenshot inspection pass.
- [x] Nested submenus and multiline tooltips.
  - Recursive UIMenuItem children; hover/click/keyboard opening, Left return,
    scoped navigation, whole-chain dismissal and focus restoration. Separate
    overlay clips, edge flipping, retained cascade direction, and row-scroll
    dismissal preserve the HAL/injected-input boundaries.
  - Shared text layout preserves explicit/blank lines and wraps UTF-8 text;
    tooltips reflow to a 320px/display-width cap and clip excess height.
  - Seven regressions added. Full MSVC/Ninja build and all 108 CTest cases
    pass (Lua enabled). GL33/SDL3/legacy GL desktop/compact native smoke
    passes; submenu and tooltip captures inspected. Shared showcase updated.
- [x] Focus ring visualization; focused-button Enter/Space feedback.
  - Two-tone inset outlines follow manager focus, after widget content and
    within ancestor clips. Per-widget color and opt-out; no layout/HAL changes.
  - Buttons use their pressed color while an activation key is held; matching
    release activates once, and focus loss cancels without changing mouse state.
  - Four regressions; full MSVC/Ninja build and all 112 CTest cases pass (Lua
    enabled). GL33/SDL3/legacy GL desktop/compact native smoke passes; SDL3
    focus/pressed captures inspected. README and shared showcase updated.
- [x] Explicit dialog-default button selection and Enter routing.
  - UIManager SetDefaultButton/GetDefaultButton register a descendant button
    per dialog; nearest scope wins, cleared scopes and windows block outer defaults.
  - Single-line inputs route unmodified Enter to the eligible default, retaining
    focus and showing pressed feedback until one release-time OnClick. Focus,
    registration, popup, and eligibility changes cancel pending activation.
  - Multiline inputs and other controls retain Enter; custom IFocusable widgets
    can opt in. Hidden/disabled defaults preserve normal input submission.
  - Eight regressions; full MSVC/Ninja build and all 120 CTest cases pass (Lua
    enabled). README updated; native visual smoke not run. Core HAL unchanged.
- [ ] Copy/paste and opt-in selection for static text labels.
- [ ] Rendering perf: batching/instancing, texture atlas management,
      command buffer dirty-tracking (currently one draw call per command).
- [ ] Expand Lua bindings: UIInputBox, UIDropDownBox, UICanvasWidget,
      UIControl are not exposed to Lua yet (lua/src/LuaBindings.cpp only
      covers Widget/Button/StatusBar/FontString/Texture).
  - Also expose the new compound widgets, checkbox/radio/window APIs and
    window events, and tooltip properties.
  - Expose the base-widget margin/padding, size constraints, relative sizing,
    aspect ratio, and UIStyle APIs.
  - `UI.CreateFrame` factory only supports "Frame", "Button", "StatusBar",
    "EditBox" — missing "DropDown", "Canvas" etc.      
- [ ] Software renderer backend
  - user provides a custom framebuffer
- [ ] Animation/tween framework.

## Notes
- Full status assessment generated via Explore subagent on 2026-09-30;
  re-run a similar sweep periodically to keep this list current since it's
  based on a point-in-time code read, not continuously verified.
