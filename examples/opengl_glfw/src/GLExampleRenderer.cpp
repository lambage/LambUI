#include "GLExampleRenderer.h"

#if defined(_WIN32)
#include <windows.h>
#endif
#include <GL/gl.h>

using namespace LambUI;

namespace {
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

void GLExampleRenderer::SubmitRenderCommands(const std::vector<UIRenderCommand>& commands) {
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

            case RenderCommandType::DrawString: {
                // TODO: replace with an MSDF text pipeline (goals.txt item 5).
                // Draw a faint placeholder box so text regions stay visible for now.
                UIRenderCommand placeholder = cmd;
                placeholder.color = 0x80808080u;
                placeholder.width = placeholder.width > 0.0f ? placeholder.width : 80.0f;
                placeholder.height = placeholder.height > 0.0f ? placeholder.height : 16.0f;
                DrawQuad(placeholder);
                break;
            }

            case RenderCommandType::PushScissor: {
                glEnable(GL_SCISSOR_TEST);
                const int scissorY = m_viewportHeight - static_cast<int>(cmd.y + cmd.height);
                glScissor(static_cast<int>(cmd.x), scissorY,
                          static_cast<int>(cmd.width), static_cast<int>(cmd.height));
                break;
            }

            case RenderCommandType::PopScissor:
                glDisable(GL_SCISSOR_TEST);
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
