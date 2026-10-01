#include "GLExampleRenderer.h"
#include "LambUI/UILog.h"
#include <algorithm>
#include <array>

#if defined(_WIN32)
#include <windows.h>
#endif
#include <GL/gl.h>

using namespace LambUI;

namespace {
constexpr const char* TAG = "GLExampleRenderer";
void SetGLColor(uint32_t packedRGBA) {
    const float r = static_cast<float>((packedRGBA >> 24) & 0xFF) / 255.0f;
    const float g = static_cast<float>((packedRGBA >> 16) & 0xFF) / 255.0f;
    const float b = static_cast<float>((packedRGBA >> 8) & 0xFF) / 255.0f;
    const float a = static_cast<float>(packedRGBA & 0xFF) / 255.0f;
    glColor4f(r, g, b, a);
}

void DrawQuad(const UIRenderCommand& cmd) {
    SetGLColor(cmd.color);
    glBegin(GL_QUADS);
    glVertex2f(cmd.x, cmd.y);
    glVertex2f(cmd.x + cmd.width, cmd.y);
    glVertex2f(cmd.x + cmd.width, cmd.y + cmd.height);
    glVertex2f(cmd.x, cmd.y + cmd.height);
    glEnd();
}
} // namespace

void GLExampleRenderer::SetViewportSize(int width, int height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
}

bool GLExampleRenderer::LoadFont(const FontAtlas& atlas) {
    m_fontAtlas = &atlas;

    if (m_fontTexture == 0) {
        glGenTextures(1, &m_fontTexture);
    }
    glBindTexture(GL_TEXTURE_2D, m_fontTexture);
    // GL_INTENSITY replicates the single SDF channel into R,G,B,A so vertex
    // color tinting (glColor4f) and GL_ALPHA_TEST both work against it.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_INTENSITY, atlas.GetAtlasWidth(), atlas.GetAtlasHeight(),
                 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, atlas.GetAtlasPixels().data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void GLExampleRenderer::DrawString(const UIRenderCommand& cmd) {
    if (!m_fontAtlas || m_fontTexture == 0) return;

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    // Threshold slightly below the baked on-edge value for a touch of AA falloff.
    glAlphaFunc(GL_GREATER, static_cast<float>(m_fontAtlas->GetOnEdgeValue()) / 255.0f - 0.15f);
    glBindTexture(GL_TEXTURE_2D, m_fontTexture);
    SetGLColor(cmd.color);

    float penX = cmd.x;
    const float penY = cmd.y + m_fontAtlas->GetAscent();

    glBegin(GL_QUADS);
    for (unsigned char c : cmd.text) {
        const GlyphInfo* glyph = m_fontAtlas->FindGlyph(static_cast<char32_t>(c));
        if (!glyph) continue;

        if (glyph->width > 0.0f && glyph->height > 0.0f) {
            const float qx = penX + glyph->bearingX;
            const float qy = penY + glyph->bearingY;
            glTexCoord2f(glyph->u0, glyph->v0); glVertex2f(qx, qy);
            glTexCoord2f(glyph->u1, glyph->v0); glVertex2f(qx + glyph->width, qy);
            glTexCoord2f(glyph->u1, glyph->v1); glVertex2f(qx + glyph->width, qy + glyph->height);
            glTexCoord2f(glyph->u0, glyph->v1); glVertex2f(qx, qy + glyph->height);
        }
        penX += glyph->advance;
    }
    glEnd();

    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
}

void GLExampleRenderer::SubmitRenderCommands(const std::vector<UIRenderCommand>& commands) {
    LAMBUI_LOGT(TAG, "SubmitRenderCommands({})", commands.size());
    std::vector<std::array<GLint, 4>> clips;
    glDisable(GL_SCISSOR_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // Top-left origin, y-down, matching LambUI's screen-space convention.
    glOrtho(0.0, m_viewportWidth, m_viewportHeight, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const auto& cmd : commands) {
        switch (cmd.type) {
            case RenderCommandType::DrawQuad:
                DrawQuad(cmd);
                break;

            case RenderCommandType::DrawString:
                DrawString(cmd);
                break;

            case RenderCommandType::PushScissor: {
                GLint left = static_cast<GLint>(cmd.x);
                GLint bottom = m_viewportHeight - static_cast<GLint>(cmd.y + cmd.height);
                GLint right = left + (std::max)(0, static_cast<GLint>(cmd.width));
                GLint top = bottom + (std::max)(0, static_cast<GLint>(cmd.height));
                if (!clips.empty()) {
                    const auto& parent = clips.back();
                    left = (std::max)(left, parent[0]);
                    bottom = (std::max)(bottom, parent[1]);
                    right = (std::min)(right, parent[0] + parent[2]);
                    top = (std::min)(top, parent[1] + parent[3]);
                }
                clips.push_back({left, bottom, (std::max)(0, right - left), (std::max)(0, top - bottom)});
                glEnable(GL_SCISSOR_TEST);
                const auto& clip = clips.back();
                glScissor(clip[0], clip[1], clip[2], clip[3]);
                break;
            }

            case RenderCommandType::PopScissor:
                if (!clips.empty()) clips.pop_back();
                if (clips.empty()) {
                    glDisable(GL_SCISSOR_TEST);
                } else {
                    const auto& clip = clips.back();
                    glScissor(clip[0], clip[1], clip[2], clip[3]);
                }
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
