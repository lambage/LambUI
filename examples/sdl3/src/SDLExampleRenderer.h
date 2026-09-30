#pragma once

#include "LambUI/IRenderer.h"
#include "LambUI/UIFontAtlas.h"

struct SDL_Renderer;
struct SDL_Texture;

// Minimal SDL3 2D renderer: draws colored quads via SDL_RenderFillRect.
// Text is drawn from a baked FontAtlas SDF bitmap uploaded as an SDL_Texture;
// since SDL_Renderer has no programmable shader stage, the raw distance
// values are sharpened into a crisp alpha mask once at upload time instead
// of being smoothstepped per-pixel like a true MSDF shader would.
class SDLExampleRenderer : public LambUI::IRenderer {
public:
    explicit SDLExampleRenderer(SDL_Renderer* renderer) : m_renderer(renderer) {}
    ~SDLExampleRenderer() override;

    bool LoadFont(const LambUI::FontAtlas& atlas);

    void SubmitRenderCommands(const std::vector<LambUI::UIRenderCommand>& commands) override;

private:
    void DrawString(const LambUI::UIRenderCommand& cmd);

    SDL_Renderer* m_renderer;
    const LambUI::FontAtlas* m_fontAtlas = nullptr;
    SDL_Texture* m_fontTexture = nullptr;
};
