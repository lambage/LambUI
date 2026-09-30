#include "VulkanExampleRenderer.h"
#include "LambUI/LambUI.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace LambUI;

namespace {

UIManager* g_uiManager = nullptr;

void CursorPosCallback(GLFWwindow*, double x, double y) {
    if (g_uiManager) g_uiManager->InjectMouseMove(static_cast<float>(x), static_cast<float>(y));
}

void MouseButtonCallback(GLFWwindow*, int button, int action, int) {
    if (!g_uiManager) return;
    MouseButton mapped = MouseButton::Left;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) mapped = MouseButton::Right;
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE) mapped = MouseButton::Middle;
    g_uiManager->InjectMouseButton(mapped, action == GLFW_PRESS);
}

void CharCallback(GLFWwindow*, unsigned int codepoint) {
    if (g_uiManager) g_uiManager->InjectCharacter(static_cast<char32_t>(codepoint));
}

void CheckResult(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string("Vulkan error in ") + what);
    }
}

} // namespace

// Minimal "hello swapchain" bring-up: instance -> surface -> device -> swapchain
// -> per-frame clear via vkCmdClearColorImage -> present. No render pass or
// pipeline yet; see VulkanExampleRenderer for what's still TODO.
int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "LambUI - Vulkan Example (WIP)", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    try {
        // --- Instance ---
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "LambUI Vulkan Example";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "LambUI";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &appInfo;
        instanceInfo.enabledExtensionCount = glfwExtensionCount;
        instanceInfo.ppEnabledExtensionNames = glfwExtensions;

        VkInstance instance = VK_NULL_HANDLE;
        CheckResult(vkCreateInstance(&instanceInfo, nullptr, &instance), "vkCreateInstance");

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        CheckResult(glfwCreateWindowSurface(instance, window, nullptr, &surface), "glfwCreateWindowSurface");

        // --- Physical device + queue family (assumes the first device has a
        // combined graphics+present queue family, true on the vast majority
        // of desktop GPUs/drivers). ---
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) throw std::runtime_error("no Vulkan-capable device found");
        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());
        VkPhysicalDevice physicalDevice = physicalDevices[0];

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        uint32_t queueFamilyIndex = UINT32_MAX;
        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                queueFamilyIndex = i;
                break;
            }
        }
        if (queueFamilyIndex == UINT32_MAX) throw std::runtime_error("no combined graphics+present queue family found");

        // --- Logical device ---
        const float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamilyIndex;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = 1;
        deviceInfo.ppEnabledExtensionNames = deviceExtensions;

        VkDevice device = VK_NULL_HANDLE;
        CheckResult(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device), "vkCreateDevice");

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

        // --- Swapchain ---
        VkSurfaceCapabilitiesKHR capabilities{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
        VkSurfaceFormatKHR surfaceFormat = formats[0];
        for (const auto& candidate : formats) {
            if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM &&
                candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surfaceFormat = candidate;
                break;
            }
        }

        VkExtent2D extent = capabilities.currentExtent;
        if (extent.width == UINT32_MAX) {
            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            extent.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            extent.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0) imageCount = std::min(imageCount, capabilities.maxImageCount);

        VkSwapchainCreateInfoKHR swapchainInfo{};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.surface = surface;
        swapchainInfo.minImageCount = imageCount;
        swapchainInfo.imageFormat = surfaceFormat.format;
        swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapchainInfo.imageExtent = extent;
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.preTransform = capabilities.currentTransform;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchainInfo.clipped = VK_TRUE;

        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        CheckResult(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain), "vkCreateSwapchainKHR");

        uint32_t swapchainImageCount = 0;
        vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
        std::vector<VkImage> swapchainImages(swapchainImageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

        // --- Command pool/buffer + sync objects ---
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndex;

        VkCommandPool commandPool = VK_NULL_HANDLE;
        CheckResult(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "vkCreateCommandPool");

        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        CheckResult(vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer), "vkAllocateCommandBuffers");

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkSemaphore renderFinished = VK_NULL_HANDLE;
        CheckResult(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable), "vkCreateSemaphore");
        CheckResult(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished), "vkCreateSemaphore");

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence inFlightFence = VK_NULL_HANDLE;
        CheckResult(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence), "vkCreateFence");

        // --- LambUI setup (identical wiring to the OpenGL/SDL3 examples) ---
        auto renderer = std::make_shared<VulkanExampleRenderer>();
        UIManager uiManager(renderer);
        g_uiManager = &uiManager;
        uiManager.SetDisplaySize(static_cast<float>(extent.width), static_cast<float>(extent.height));

        glfwSetCursorPosCallback(window, CursorPosCallback);
        glfwSetMouseButtonCallback(window, MouseButtonCallback);
        glfwSetCharCallback(window, CharCallback);

        UIWidget& root = uiManager.GetRoot();
        UIWidget* panel = root.CreateChild<UIWidget>("Panel");
        panel->SetSize(220.0f, 140.0f);
        panel->SetPoint(AnchorPoint::TopLeft, &root, AnchorPoint::TopLeft, 20.0f, 20.0f);

        UIButton* button = panel->CreateChild<UIButton>("DemoButton");
        button->SetSize(180.0f, 40.0f);
        button->SetPoint(AnchorPoint::Top, panel, AnchorPoint::Top, 0.0f, 16.0f);
        button->RegisterCallback(UIEventType::OnClick, [](const UIEventData&) {
            std::cout << "DemoButton clicked! (Vulkan quad rendering not yet implemented)\n";
        });

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            uiManager.Update(0.0f);
            uiManager.Render(); // no-op today; see VulkanExampleRenderer TODO

            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            uint32_t imageIndex = 0;
            vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex);

            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            VkImageSubresourceRange range{};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.levelCount = 1;
            range.layerCount = 1;

            VkImageMemoryBarrier toTransferDst{};
            toTransferDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransferDst.image = swapchainImages[imageIndex];
            toTransferDst.subresourceRange = range;
            toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransferDst);

            VkClearColorValue clearColor{{0.1f, 0.1f, 0.12f, 1.0f}};
            vkCmdClearColorImage(commandBuffer, swapchainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  &clearColor, 1, &range);

            VkImageMemoryBarrier toPresent{};
            toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            toPresent.image = swapchainImages[imageIndex];
            toPresent.subresourceRange = range;
            toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &toPresent);

            vkEndCommandBuffer(commandBuffer);

            VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &imageAvailable;
            submitInfo.pWaitDstStageMask = &waitStage;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &renderFinished;
            vkQueueSubmit(queue, 1, &submitInfo, inFlightFence);

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderFinished;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;
            vkQueuePresentKHR(queue, &presentInfo);
        }

        vkDeviceWaitIdle(device);
        g_uiManager = nullptr;

        vkDestroyFence(device, inFlightFence, nullptr);
        vkDestroySemaphore(device, renderFinished, nullptr);
        vkDestroySemaphore(device, imageAvailable, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    } catch (const std::exception& e) {
        std::cerr << "Vulkan example failed: " << e.what() << "\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
