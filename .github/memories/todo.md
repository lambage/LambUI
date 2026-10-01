# LambUI TODO (as of 2026-10-01)

Baseline: core architecture (dual-tree, anchor layout, IRenderer HAL, input
injection, event routing, Lua/sol2 bindings) is fully implemented and builds
clean. Tests pass (34 GoogleTest cases).
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
    Native visual testing was not performed; Lua remains disabled.
  - Follow-ups: nested submenus and multiline tooltips; keyboard navigation
    and Lua exposure remain separate backlog items.
- [ ] Multiple fonts
  - allow for multiple fonts to be registered and text objects will contain
    a font property
- [ ] Keyboard navigation (Tab/Shift-Tab focus cycling, arrow keys).
- [ ] Margin/padding system (currently only anchor point + offset).
- [ ] Min/max size constraints, aspect-ratio preservation, relative
      ("% of parent") sizing.
- [ ] styling, rounded corners, shadows, fill colors/patterns    

## Medium priority (common game UI patterns)
- [ ] Expand Lua bindings: UIInputBox, UIDropDownBox, UICanvasWidget,
      UIControl are not exposed to Lua yet (lua/src/LuaBindings.cpp only
      covers Widget/Button/StatusBar/FontString/Texture).
  - Also expose the new compound widgets and tooltip properties.
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
- [ ] Animation/tween framework.
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
