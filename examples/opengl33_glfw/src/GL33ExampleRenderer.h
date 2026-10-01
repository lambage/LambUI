#pragma once

#include "LambUI/IRenderer.h"
#include "LambUI/UIFontAtlas.h"
#include <glad/glad.h>
#include <unordered_map>
#include <utility>

class GL33ExampleRenderer final : public LambUI::IRenderer {
public:
    GL33ExampleRenderer();
    ~GL33ExampleRenderer() override;
    GL33ExampleRenderer(const GL33ExampleRenderer&) = delete;
    GL33ExampleRenderer& operator=(const GL33ExampleRenderer&) = delete;

    bool Initialize();
    void SetViewportSize(int width, int height, int framebufferWidth, int framebufferHeight);
    bool LoadFont(const LambUI::FontAtlas& atlas, void* fontHandle = nullptr);
    void SetEffect(float time, float strength);
    void DrawEffect(const LambUI::UICustomRenderArgs& args);
    void SubmitRenderCommands(const std::vector<LambUI::UIRenderCommand>& commands) override;

private:
    void BindPipeline();
    void DrawQuad(const LambUI::UIRenderCommand& command, int mode, GLuint texture = 0, float edge = 0.5f);
    void DrawString(const LambUI::UIRenderCommand& command);

    GLuint m_program = 0;
    GLuint m_vertexArray = 0;
    GLuint m_vertexBuffer = 0;
    std::unordered_map<void*, std::pair<const LambUI::FontAtlas*, GLuint>> m_fonts;
    GLint m_displayLocation = -1;
    GLint m_colorLocation = -1;
    GLint m_modeLocation = -1;
    GLint m_edgeLocation = -1;
    GLint m_timeLocation = -1;
    GLint m_strengthLocation = -1;
    int m_width = 1;
    int m_height = 1;
    int m_framebufferWidth = 1;
    int m_framebufferHeight = 1;
    float m_time = 0.0f;
    float m_strength = 0.5f;
};