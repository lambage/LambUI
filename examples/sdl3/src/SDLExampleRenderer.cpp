#include "SDLExampleRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <vector>

using namespace LambUI;

namespace {
void ApplyColor(SDL_Renderer* renderer, uint32_t packedRGBA) {
    const Uint8 r = static_cast<Uint8>((packedRGBA >> 24) & 0xFF);
    const Uint8 g = static_cast<Uint8>((packedRGBA >> 16) & 0xFF);
    const Uint8 b = static_cast<Uint8>((packedRGBA >> 8) & 0xFF);
    const Uint8 a = static_cast<Uint8>(packedRGBA & 0xFF);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}
} // namespace

SDLExampleRenderer::~SDLExampleRenderer() {
    if (m_fontTexture) SDL_DestroyTexture(m_fontTexture);
}

bool SDLExampleRenderer::LoadFont(const FontAtlas& atlas) {
    m_fontAtlas = &atlas;

    const int width = atlas.GetAtlasWidth();
    const int height = atlas.GetAtlasHeight();
    const auto& sdfPixels = atlas.GetAtlasPixels();

    // SDL_Renderer has no per-pixel alpha test/shader, so bake a sharpened
    // coverage curve into the RGBA upload to approximate a crisp SDF edge.
    const float onEdge = static_cast<float>(atlas.GetOnEdgeValue());
    constexpr float kSharpness = 4.0f;
    std::vector<uint32_t> rgba(static_cast<size_t>(width) * static_cast<size_t>(height));
    for (size_t i = 0; i < rgba.size(); ++i) {
        const float d = static_cast<float>(sdfPixels[i]);
        const float sharpened = std::clamp((d - onEdge) * kSharpness + 128.0f, 0.0f, 255.0f);
        const uint8_t a = static_cast<uint8_t>(sharpened);
        rgba[i] = 0x00FFFFFFu | (static_cast<uint32_t>(a) << 24); // white RGB, sharpened alpha
    }

    if (m_fontTexture) SDL_DestroyTexture(m_fontTexture);
    m_fontTexture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STATIC, width, height);
    if (!m_fontTexture) return false;

    SDL_UpdateTexture(m_fontTexture, nullptr, rgba.data(), width * static_cast<int>(sizeof(uint32_t)));
    SDL_SetTextureBlendMode(m_fontTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(m_fontTexture, SDL_SCALEMODE_LINEAR);
    return true;
}

void SDLExampleRenderer::DrawString(const UIRenderCommand& cmd) {
    if (!m_fontAtlas || !m_fontTexture) return;

    const Uint8 r = static_cast<Uint8>((cmd.color >> 24) & 0xFF);
    const Uint8 g = static_cast<Uint8>((cmd.color >> 16) & 0xFF);
    const Uint8 b = static_cast<Uint8>((cmd.color >> 8) & 0xFF);
    const Uint8 a = static_cast<Uint8>(cmd.color & 0xFF);
    SDL_SetTextureColorMod(m_fontTexture, r, g, b);
    SDL_SetTextureAlphaMod(m_fontTexture, a);

    const float atlasW = static_cast<float>(m_fontAtlas->GetAtlasWidth());
    const float atlasH = static_cast<float>(m_fontAtlas->GetAtlasHeight());
    float penX = cmd.x;
    const float penY = cmd.y + m_fontAtlas->GetAscent();

    for (unsigned char c : cmd.text) {
        const GlyphInfo* glyph = m_fontAtlas->FindGlyph(static_cast<char32_t>(c));
        if (!glyph) continue;

        if (glyph->width > 0.0f && glyph->height > 0.0f) {
            SDL_FRect src{glyph->u0 * atlasW, glyph->v0 * atlasH,
                          (glyph->u1 - glyph->u0) * atlasW, (glyph->v1 - glyph->v0) * atlasH};
            SDL_FRect dst{penX + glyph->bearingX, penY + glyph->bearingY, glyph->width, glyph->height};
            SDL_RenderTexture(m_renderer, m_fontTexture, &src, &dst);
        }
        penX += glyph->advance;
    }
}

void SDLExampleRenderer::SubmitRenderCommands(const std::vector<UIRenderCommand>& commands) {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    for (const auto& cmd : commands) {
        switch (cmd.type) {
            case RenderCommandType::DrawQuad: {
                ApplyColor(m_renderer, cmd.color);
                SDL_FRect rect{cmd.x, cmd.y, cmd.width, cmd.height};
                SDL_RenderFillRect(m_renderer, &rect);
                break;
            }
            case RenderCommandType::DrawString:
                DrawString(cmd);
                break;
            case RenderCommandType::PushScissor: {
                SDL_Rect clip{static_cast<int>(cmd.x), static_cast<int>(cmd.y),
                              static_cast<int>(cmd.width), static_cast<int>(cmd.height)};
                SDL_SetRenderClipRect(m_renderer, &clip);
                break;
            }
            case RenderCommandType::PopScissor:
                SDL_SetRenderClipRect(m_renderer, nullptr);
                break;
            case RenderCommandType::CustomCallback:
                if (cmd.customRenderFunc) {
                    UICustomRenderArgs args;
                    args.viewportX = cmd.x;
                    args.viewportY = cmd.y;
                    args.viewportWidth = cmd.width;
                    args.viewportHeight = cmd.height;
                    args.userData = cmd.customRenderUserData;
                    cmd.customRenderFunc(args);
                }
                break;
        }
    }
}
