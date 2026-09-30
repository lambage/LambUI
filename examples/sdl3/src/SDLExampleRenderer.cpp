#include "SDLExampleRenderer.h"
#include <SDL3/SDL.h>

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
            case RenderCommandType::DrawString: {
                // TODO: replace with an MSDF text pipeline (goals.txt item 5).
                ApplyColor(m_renderer, 0x80808080u);
                SDL_FRect rect{cmd.x, cmd.y, cmd.width > 0.0f ? cmd.width : 80.0f,
                               cmd.height > 0.0f ? cmd.height : 16.0f};
                SDL_RenderFillRect(m_renderer, &rect);
                break;
            }
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
