#pragma once

#include "LambUI/IRenderer.h"

struct SDL_Renderer;

// Minimal SDL3 2D renderer: draws colored quads via SDL_RenderFillRect.
// Text rendering (MSDF, per goals.txt item 5) is left as a TODO placeholder.
class SDLExampleRenderer : public LambUI::IRenderer {
public:
    explicit SDLExampleRenderer(SDL_Renderer* renderer) : m_renderer(renderer) {}

    void SubmitRenderCommands(const std::vector<LambUI::UIRenderCommand>& commands) override;

private:
    SDL_Renderer* m_renderer;
};
