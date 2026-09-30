#pragma once

#include "LambUI/IRenderer.h"

// Placeholder backend: proves the widget tree, layout solver, and input
// injection pipeline are 100% reusable across renderers with zero changes.
// Actually turning the command bucket into a Vulkan pipeline (vertex/index
// buffers, a simple textured-quad shader, per-frame descriptor sets) is left
// as the next implementation step for this example.
class VulkanExampleRenderer : public LambUI::IRenderer {
public:
    void SubmitRenderCommands(const std::vector<LambUI::UIRenderCommand>& commands) override;
};
