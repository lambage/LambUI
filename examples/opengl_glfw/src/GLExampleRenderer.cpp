#include "GLExampleRenderer.h"
#include "LambUI/UILog.h"
#include <algorithm>
#include <array>
#include <cmath>

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

GLExampleRenderer::GLExampleRenderer() {
    LAMBUI_LOGT(TAG, "Construct");
}

GLExampleRenderer::~GLExampleRenderer() {
    LAMBUI_LOGT(TAG, "Destroy");
    for (const auto& entry : m_fonts) glDeleteTextures(1, &entry.second.second);
}

void GLExampleRenderer::SetViewportSize(int width, int height, int framebufferWidth, int framebufferHeight) {
    if (framebufferWidth < 0) framebufferWidth = width;
    if (framebufferHeight < 0) framebufferHeight = height;
    if (width == m_viewportWidth && height == m_viewportHeight &&
        framebufferWidth == m_framebufferWidth && framebufferHeight == m_framebufferHeight) return;
    LAMBUI_LOGT(TAG, "SetViewportSize({}, {}, {}, {})", width, height, framebufferWidth, framebufferHeight);
    m_viewportWidth = width;
    m_viewportHeight = height;
    m_framebufferWidth = framebufferWidth;
    m_framebufferHeight = framebufferHeight;
}

bool GLExampleRenderer::LoadFont(const FontAtlas& atlas, void* fontHandle) {
    LAMBUI_LOGT(TAG, "LoadFont({}, {}x{})", fmt::ptr(fontHandle), atlas.GetAtlasWidth(), atlas.GetAtlasHeight());
    if (atlas.GetAtlasPixels().empty() || m_fonts.count(fontHandle)) return false;
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    // GL_INTENSITY replicates the single SDF channel into R,G,B,A so vertex
    // color tinting (glColor4f) and GL_ALPHA_TEST both work against it.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_INTENSITY, atlas.GetAtlasWidth(), atlas.GetAtlasHeight(),
                 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, atlas.GetAtlasPixels().data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return false;
    }
    m_fonts.emplace(fontHandle, std::make_pair(&atlas, texture));
    return true;
}

void GLExampleRenderer::DrawString(const UIRenderCommand& cmd) {
    auto found = m_fonts.find(cmd.fontHandle);
    if (found == m_fonts.end()) found = m_fonts.find(nullptr);
    if (found == m_fonts.end()) return;
    const auto* atlas = found->second.first;
    const auto texture = found->second.second;

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    // Threshold slightly below the baked on-edge value for a touch of AA falloff.
    glAlphaFunc(GL_GREATER, static_cast<float>(atlas->GetOnEdgeValue()) / 255.0f - 0.15f);
    glBindTexture(GL_TEXTURE_2D, texture);
    SetGLColor(cmd.color);

    float penX = cmd.x;
    const float penY = cmd.y + atlas->GetAscent();

    glBegin(GL_QUADS);
    for (unsigned char c : cmd.text) {
        const GlyphInfo* glyph = atlas->FindGlyph(static_cast<char32_t>(c));
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
    if (m_viewportWidth <= 0 || m_viewportHeight <= 0 || m_framebufferWidth <= 0 || m_framebufferHeight <= 0) return;
    glViewport(0, 0, m_framebufferWidth, m_framebufferHeight);
    const float scaleX = static_cast<float>(m_framebufferWidth) / m_viewportWidth;
    const float scaleY = static_cast<float>(m_framebufferHeight) / m_viewportHeight;
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
                GLint left = static_cast<GLint>(std::floor(cmd.x * scaleX));
                GLint bottom = m_framebufferHeight - static_cast<GLint>(std::ceil((cmd.y + cmd.height) * scaleY));
                GLint right = static_cast<GLint>(std::ceil((cmd.x + cmd.width) * scaleX));
                GLint top = m_framebufferHeight - static_cast<GLint>(std::floor(cmd.y * scaleY));
                const auto parent = clips.empty()
                    ? std::array<GLint, 4>{0, 0, m_framebufferWidth, m_framebufferHeight} : clips.back();
                left = (std::max)(left, parent[0]);
                bottom = (std::max)(bottom, parent[1]);
                right = (std::min)(right, parent[0] + parent[2]);
                top = (std::min)(top, parent[1] + parent[3]);
                if (cmd.width <= 0.0f) right = left;
                if (cmd.height <= 0.0f) top = bottom;
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
