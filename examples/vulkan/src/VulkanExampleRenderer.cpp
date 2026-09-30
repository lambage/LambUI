#include "VulkanExampleRenderer.h"
#include <iostream>

using namespace LambUI;

void VulkanExampleRenderer::SubmitRenderCommands(const std::vector<UIRenderCommand>& commands) {
    // TODO: translate `commands` into a Vulkan quad/text pipeline (vertex
    // buffer upload, bind pipeline, vkCmdDrawIndexed per draw, scissor via
    // vkCmdSetScissor). For now this just proves the frame loop is wired up.
    static bool loggedOnce = false;
    if (!loggedOnce && !commands.empty()) {
        std::cout << "[Vulkan example] received " << commands.size()
                  << " UI render commands this frame (rendering not yet implemented)\n";
        loggedOnce = true;
    }
}
