#include "SDLExampleRenderer.h"
#include "LambUI/UILog.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <vector>

using namespace LambUI;

namespace {
constexpr const char* TAG = "SDLExampleRenderer";
void ApplyColor(SDL_Renderer* renderer, uint32_t packedRGBA) {
    const Uint8 r = static_cast<Uint8>((packedRGBA >> 24) & 0xFF);
    const Uint8 g = static_cast<Uint8>((packedRGBA >> 16) & 0xFF);
    const Uint8 b = static_cast<Uint8>((packedRGBA >> 8) & 0xFF);
    const Uint8 a = static_cast<Uint8>(packedRGBA & 0xFF);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}
} // namespace

SDLExampleRenderer::SDLExampleRenderer(SDL_Renderer* renderer) : m_renderer(renderer) {
    LAMBUI_LOGT(TAG, "Construct");
}

SDLExampleRenderer::~SDLExampleRenderer() {
    LAMBUI_LOGT(TAG, "Destroy");
    for (const auto& entry : m_fonts) SDL_DestroyTexture(entry.second.second);
}

bool SDLExampleRenderer::LoadFont(const FontAtlas& atlas, void* fontHandle) {
    LAMBUI_LOGT(TAG, "LoadFont({}, {}x{})", fmt::ptr(fontHandle), atlas.GetAtlasWidth(), atlas.GetAtlasHeight());
    if (atlas.GetAtlasPixels().empty() || m_fonts.count(fontHandle)) return false;

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

    auto* texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STATIC, width, height);
    if (!texture) return false;

    if (!SDL_UpdateTexture(texture, nullptr, rgba.data(), width * static_cast<int>(sizeof(uint32_t))) ||
        !SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) ||
        !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR)) {
        SDL_DestroyTexture(texture);
        return false;
    }
    m_fonts.emplace(fontHandle, std::make_pair(&atlas, texture));
    return true;
}

void SDLExampleRenderer::DrawString(const UIRenderCommand& cmd) {
    auto found = m_fonts.find(cmd.fontHandle);
    if (found == m_fonts.end()) found = m_fonts.find(nullptr);
    if (found == m_fonts.end()) return;
    const auto& [atlas, texture] = found->second;

    const Uint8 r = static_cast<Uint8>((cmd.color >> 24) & 0xFF);
    const Uint8 g = static_cast<Uint8>((cmd.color >> 16) & 0xFF);
    const Uint8 b = static_cast<Uint8>((cmd.color >> 8) & 0xFF);
    const Uint8 a = static_cast<Uint8>(cmd.color & 0xFF);
    SDL_SetTextureColorMod(texture, r, g, b);
    SDL_SetTextureAlphaMod(texture, a);

    const float atlasW = static_cast<float>(atlas->GetAtlasWidth());
    const float atlasH = static_cast<float>(atlas->GetAtlasHeight());
    float penX = cmd.x;
    const float penY = cmd.y + atlas->GetAscent();

    for (unsigned char c : cmd.text) {
        const GlyphInfo* glyph = atlas->FindGlyph(static_cast<char32_t>(c));
        if (!glyph) continue;

        if (glyph->width > 0.0f && glyph->height > 0.0f) {
            SDL_FRect src{glyph->u0 * atlasW, glyph->v0 * atlasH,
                          (glyph->u1 - glyph->u0) * atlasW, (glyph->v1 - glyph->v0) * atlasH};
            SDL_FRect dst{penX + glyph->bearingX, penY + glyph->bearingY, glyph->width, glyph->height};
            SDL_RenderTexture(m_renderer, texture, &src, &dst);
        }
        penX += glyph->advance;
    }
}

void SDLExampleRenderer::SubmitRenderCommands(const std::vector<UIRenderCommand>& commands) {
    LAMBUI_LOGT(TAG, "SubmitRenderCommands({})", commands.size());
    std::vector<SDL_Rect> clips;
    SDL_SetRenderClipRect(m_renderer, nullptr);
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
                              std::max(0, static_cast<int>(cmd.width)),
                              std::max(0, static_cast<int>(cmd.height))};
                if (!clips.empty()) {
                    SDL_Rect intersection{};
                    if (!SDL_GetRectIntersection(&clips.back(), &clip, &intersection)) {
                        intersection = {};
                    }
                    clip = intersection;
                }
                clips.push_back(clip);
                SDL_SetRenderClipRect(m_renderer, &clip);
                break;
            }
            case RenderCommandType::PopScissor:
                if (!clips.empty()) clips.pop_back();
                SDL_SetRenderClipRect(m_renderer, clips.empty() ? nullptr : &clips.back());
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
