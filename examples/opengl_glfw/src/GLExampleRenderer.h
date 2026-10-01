#pragma once

#include "LambUI/IRenderer.h"
#include "LambUI/UIFontAtlas.h"
#include <unordered_map>
#include <utility>

// Minimal fixed-function (OpenGL 1.1) renderer: draws colored quads via
// immediate mode so this example needs no GL loader (glad/glew) dependency.
// Text is rendered from a baked FontAtlas SDF texture using GL_ALPHA_TEST
// (the classic pre-shader distance-field technique) since this example
// intentionally stays on the legacy fixed-function pipeline.
class GLExampleRenderer : public LambUI::IRenderer {
public:
    GLExampleRenderer();
    ~GLExampleRenderer() override;
    GLExampleRenderer(const GLExampleRenderer&) = delete;
    GLExampleRenderer& operator=(const GLExampleRenderer&) = delete;

    void SetViewportSize(int width, int height);

    // Uploads the atlas's SDF bitmap as a GL texture; call once at startup.
    bool LoadFont(const LambUI::FontAtlas& atlas, void* fontHandle = nullptr);

    void SubmitRenderCommands(const std::vector<LambUI::UIRenderCommand>& commands) override;

private:
    void DrawString(const LambUI::UIRenderCommand& cmd);

    int m_viewportWidth = 0;
    int m_viewportHeight = 0;

    std::unordered_map<void*, std::pair<const LambUI::FontAtlas*, unsigned int>> m_fonts;
};
