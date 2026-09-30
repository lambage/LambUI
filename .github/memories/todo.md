# LambUI TODO (as of 2026-09-30)

Baseline: core architecture (dual-tree, anchor layout, IRenderer HAL, input
injection, event routing, Lua/sol2 bindings) is fully implemented and builds
clean. Tests pass (8 GoogleTest cases). ~70% feature-complete vs goals.txt.
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
    touched here. True shader-based smoothstep sampling for GL/SDL3 would
    need a bigger pipeline upgrade (GL 3+ core context+GLSL, or SDL3's GPU
    API) - left as a lower-priority polish item if crisper edges are needed.
- [ ] Scrollable containers
  - No `UIScrollContainer`/`UIListBox` widget class.
  - `UIManager::InjectMouseWheel` is a no-op stub (comment: "Reserved for
    scrollable containers (not yet implemented)").
  - No widget currently emits PushScissor/PopScissor even though the
    command types + GL/SDL3 backend handling for them already work.
- [ ] Event bubbling
  - Events only fire on the leaf widget that was hit; unhandled events
    never propagate up to parent widgets (no bubbling phase at all).
- [ ] Vulkan renderer backend (examples/vulkan)
  - Swapchain + frame loop wired up, but `VulkanExampleRenderer::
    SubmitRenderCommands` only logs commands — no render pass/pipeline,
    no quad or text drawing implemented yet.

## Medium priority (common game UI patterns)
- [ ] Expand Lua bindings: UIInputBox, UIDropDownBox, UICanvasWidget,
      UIControl are not exposed to Lua yet (lua/src/LuaBindings.cpp only
      covers Widget/Button/StatusBar/FontString/Texture).
  - `UI.CreateFrame` factory only supports "Frame", "Button", "StatusBar",
    "EditBox" — missing "DropDown", "Canvas" etc.
- [x] Implement `ITextMeasurer` concretely in at least one example backend
      (needed before text auto-sizing / wrapping can be tested end-to-end).
  - Done as part of the SDF font work above: `FontAtlasTextMeasurer` is wired
    into both the GL and SDL3 examples via `UIManager`'s textMeasurer ctor
    arg, and `UITextWidget::SetText` auto-sizes against it.
- [ ] More compound widgets: ProgressBar, MenuBar, TabControl, TreeView,
      context menu, tooltip system.
- [ ] Text wrapping + multiline text input (UIInputBox is single-line only).


## Lower priority (polish)
- [ ] Animation/tween framework.
- [ ] Keyboard navigation (Tab/Shift-Tab focus cycling, arrow keys).
- [ ] Margin/padding system (currently only anchor point + offset).
- [ ] Min/max size constraints, aspect-ratio preservation, relative
      ("% of parent") sizing.
- [ ] Focus ring visualization; default-button Enter-key highlight.
- [ ] Text selection / copy-paste.
- [ ] Rendering perf: batching/instancing, texture atlas management,
      command buffer dirty-tracking (currently one draw call per command).

## Notes
- Full status assessment generated via Explore subagent on 2026-09-30;
  re-run a similar sweep periodically to keep this list current since it's
  based on a point-in-time code read, not continuously verified.
