#pragma once

#include "LambUI/IRenderer.h"
#include "LambUI/UIFontAtlas.h"
#include "LambUI/UITextureAtlas.h"
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
    void* UploadTextureAtlas(const LambUI::TextureAtlas& atlas);
    void ReleaseTextureAtlas(const LambUI::TextureAtlas& atlas);
    void SetEffect(float time, float strength);
    void DrawEffect(const LambUI::UICustomRenderArgs& args);
    void SubmitRenderCommands(const std::vector<LambUI::UIRenderCommand>& commands) override;
    size_t GetDrawCallCount() const { return m_drawCallCount; }
    size_t GetQuadCount() const { return m_quadCount; }
    size_t GetUploadCount() const { return m_uploadCount; }
    size_t GetTextureUploadCount() const { return m_textureUploadCount; }

private:
    void BindPipeline();
    void FlushBatch();
    void DrawQuad(const LambUI::UIRenderCommand& command, int mode, GLuint texture = 0,
                  float edge = 0.5f, float distanceScale = 0.0f);
    void DrawString(const LambUI::UIRenderCommand& command);

    GLuint m_program = 0;
    GLuint m_vertexArray = 0;
    struct CachedBatch {
        GLuint buffer = 0;
        std::vector<float> instances;
    };
    std::vector<CachedBatch> m_cachedBatches;
    std::vector<float> m_instances;
    int m_batchMode = 0;
    GLuint m_batchTexture = 0;
    float m_batchEdge = 0.5f;
    float m_batchDistanceScale = 0.0f;
    size_t m_drawCallCount = 0;
    size_t m_quadCount = 0;
    size_t m_uploadCount = 0;
    std::unordered_map<void*, std::pair<const LambUI::FontAtlas*, GLuint>> m_fonts;
    std::unordered_map<const LambUI::TextureAtlas*, std::pair<GLuint, uint64_t>> m_atlases;
    size_t m_textureUploadCount = 0;
    GLint m_displayLocation = -1;
    GLint m_modeLocation = -1;
    GLint m_edgeLocation = -1;
    GLint m_distanceScaleLocation = -1;
    GLint m_timeLocation = -1;
    GLint m_strengthLocation = -1;
    int m_width = 1;
    int m_height = 1;
    int m_framebufferWidth = 1;
    int m_framebufferHeight = 1;
    float m_time = 0.0f;
    float m_strength = 0.5f;
};