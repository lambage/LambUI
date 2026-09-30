#pragma once

#include "LambUI/IRenderer.h"

// Minimal fixed-function (OpenGL 1.1) renderer: draws colored quads via
// immediate mode so this example needs no GL loader (glad/glew) dependency.
// Text rendering (MSDF, per goals.txt item 5) is left as a TODO placeholder.
class GLExampleRenderer : public LambUI::IRenderer {
public:
    void SetViewportSize(int width, int height);

    void SubmitRenderCommands(const std::vector<LambUI::UIRenderCommand>& commands) override;

private:
    int m_viewportWidth = 0;
    int m_viewportHeight = 0;
};
