#pragma once

#include "LambUI/IRenderer.h"
#include "LambUI/UIFontAtlas.h"
#include <vulkan/vulkan.h>
#include <memory>

struct GLFWwindow;

class VulkanExampleRenderer : public LambUI::IRenderer {
public:
    VulkanExampleRenderer(GLFWwindow* window, bool validation);
    ~VulkanExampleRenderer() override;
    VulkanExampleRenderer(const VulkanExampleRenderer&) = delete;
    VulkanExampleRenderer& operator=(const VulkanExampleRenderer&) = delete;
    void LoadFont(const LambUI::FontAtlas& atlas, void* handle = nullptr);
    void* UploadTexture(int width, int height, const std::vector<uint8_t>& rgba);
    bool BeginFrame();
    void EndFrame(bool capture = false);
    void SubmitRenderCommands(const std::vector<LambUI::UIRenderCommand>& commands) override;
    void SaveCapture(const std::string& path) const;
    const std::vector<uint8_t>& GetCapture() const;
    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    unsigned GetValidationErrors() const;
    VkCommandBuffer GetCommandBuffer() const;
    void DrawField(float time, float strength);
private:
    struct State;
    std::unique_ptr<State> m_state;
};
