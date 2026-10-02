#include "VulkanExampleRenderer.h"
#include "LambUI/UILog.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <unordered_map>

using namespace LambUI;

namespace {
constexpr const char* TAG = "VulkanExampleRenderer";
void Check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string(operation) + " failed: " + std::to_string(result));
}
struct Vertex { float x, y, u, v; uint32_t color; };
struct Settings { float width, height, distanceScale, mode, edge; };
struct FieldSettings { float x, y, width, height, viewportWidth, viewportHeight, time, strength; };
VkImageSubresourceRange ColorRange() { return {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}; }
std::vector<uint32_t> ReadShader(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Cannot open shader: " + path);
    const std::streamoff size = input.tellg();
    if (size <= 0 || size % 4 != 0) throw std::runtime_error("Invalid SPIR-V: " + path);
    std::vector<uint32_t> bytes(static_cast<size_t>(size) / 4);
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), size)) throw std::runtime_error("Cannot read shader: " + path);
    return bytes;
}
}

struct VulkanExampleRenderer::State {
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    unsigned validationErrors = 0;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t family = 0;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore acquired = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkSemaphore> finished;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout fieldLayout = VK_NULL_HANDLE;
    VkPipeline fieldPipeline = VK_NULL_HANDLE;
    bool inCanvas = false;
    UIRect canvasBounds{};
    VkRect2D canvasClip{};
    VkSampler sampler = VK_NULL_HANDLE;
    VkBuffer vertices = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    size_t vertexCapacity = 0;
    VkBuffer readback = VK_NULL_HANDLE;
    VkDeviceMemory readbackMemory = VK_NULL_HANDLE;
    std::vector<uint8_t> capture;
    uint32_t captureWidth = 0, captureHeight = 0, imageIndex = 0;
    bool active = false, recreate = false, canCapture = false, submitted = false;
    int windowWidth = 0, windowHeight = 0;
    struct Texture {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDescriptorSet descriptor = VK_NULL_HANDLE;
    };
    std::vector<Texture> textures;
    struct Font { const FontAtlas* atlas; size_t texture; };
    std::unordered_map<void*, Font> fonts;

    State() { LAMBUI_LOGT(TAG, "Construct state"); }
    ~State() {
        LAMBUI_LOGT(TAG, "Destroy state");
        if (device) vkDeviceWaitIdle(device);
        DestroySwapchain();
        for (auto& texture : textures) {
            if (texture.view) vkDestroyImageView(device, texture.view, nullptr);
            if (texture.image) vkDestroyImage(device, texture.image, nullptr);
            if (texture.memory) vkFreeMemory(device, texture.memory, nullptr);
        }
        if (vertices) vkDestroyBuffer(device, vertices, nullptr);
        if (vertexMemory) vkFreeMemory(device, vertexMemory, nullptr);
        if (sampler) vkDestroySampler(device, sampler, nullptr);
        if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        if (fieldLayout) vkDestroyPipelineLayout(device, fieldLayout, nullptr);
        if (descriptorPool) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        if (descriptorLayout) vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
        if (acquired) vkDestroySemaphore(device, acquired, nullptr);
        if (fence) vkDestroyFence(device, fence, nullptr);
        if (pool) vkDestroyCommandPool(device, pool, nullptr);
        if (device) vkDestroyDevice(device, nullptr);
        if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
        if (messenger) {
            auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroy) destroy(instance, messenger, nullptr);
        }
        if (instance) vkDestroyInstance(instance, nullptr);
    }
    static VKAPI_ATTR VkBool32 VKAPI_CALL Debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void* context) {
        auto& state = *static_cast<State*>(context);
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++state.validationErrors;
        LAMBUI_LOGW(TAG, "Vulkan validation: {}", data->pMessage);
        return VK_FALSE;
    }
    uint32_t MemoryType(uint32_t bits, VkMemoryPropertyFlags flags) const {
        VkPhysicalDeviceMemoryProperties properties{};
        vkGetPhysicalDeviceMemoryProperties(physical, &properties);
        for (uint32_t index = 0; index < properties.memoryTypeCount; ++index)
            if ((bits & (1u << index)) && (properties.memoryTypes[index].propertyFlags & flags) == flags) return index;
        throw std::runtime_error("No compatible Vulkan memory type");
    }
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory) {
        LAMBUI_LOGT(TAG, "Create buffer: {} bytes", size);
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size;
        info.usage = usage;
        Check(vkCreateBuffer(device, &info, nullptr, &buffer), "vkCreateBuffer");
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        Check(vkAllocateMemory(device, &allocation, nullptr, &memory), "vkAllocateMemory(buffer)");
        Check(vkBindBufferMemory(device, buffer, memory, 0), "vkBindBufferMemory");
    }
    void Init(GLFWwindow* host, bool validation) {
        LAMBUI_LOGT(TAG, "Initialize, validation={}", validation);
        window = host;
        uint32_t extensionCount = 0;
        const auto required = glfwGetRequiredInstanceExtensions(&extensionCount);
        if (!required) throw std::runtime_error("GLFW cannot find Vulkan surface extensions");
        std::vector<const char*> extensions(required, required + extensionCount);
        const char* layer = "VK_LAYER_KHRONOS_validation";
        VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug.pfnUserCallback = Debug;
        debug.pUserData = this;
        if (validation) {
            uint32_t count = 0;
            Check(vkEnumerateInstanceLayerProperties(&count, nullptr), "enumerate layers");
            std::vector<VkLayerProperties> layers(count);
            Check(vkEnumerateInstanceLayerProperties(&count, layers.data()), "enumerate layers");
            if (std::none_of(layers.begin(), layers.end(), [layer](const VkLayerProperties& value) {
                return std::strcmp(value.layerName, layer) == 0;
            })) throw std::runtime_error("--validation requires VK_LAYER_KHRONOS_validation (Vulkan SDK)");
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        application.pApplicationName = "LambUI Expedition Planner";
        application.apiVersion = VK_API_VERSION_1_0;
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        info.pApplicationInfo = &application;
        info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        info.ppEnabledExtensionNames = extensions.data();
        info.enabledLayerCount = validation ? 1 : 0;
        info.ppEnabledLayerNames = validation ? &layer : nullptr;
        info.pNext = validation ? &debug : nullptr;
        Check(vkCreateInstance(&info, nullptr, &instance), "vkCreateInstance");
        if (validation) {
            auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
            if (!create) throw std::runtime_error("Missing debug messenger entry point");
            Check(create(instance, &debug, nullptr, &messenger), "create debug messenger");
        }
        Check(glfwCreateWindowSurface(instance, window, nullptr, &surface), "glfwCreateWindowSurface");
        uint32_t count = 0;
        Check(vkEnumeratePhysicalDevices(instance, &count, nullptr), "enumerate devices");
        std::vector<VkPhysicalDevice> devices(count);
        Check(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "enumerate devices");
        for (auto candidate : devices) {
            uint32_t availableCount = 0;
            Check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &availableCount, nullptr), "device extensions");
            std::vector<VkExtensionProperties> available(availableCount);
            Check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &availableCount, available.data()), "device extensions");
            if (std::none_of(available.begin(), available.end(), [](const VkExtensionProperties& item) {
                return std::strcmp(item.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
            })) continue;
            uint32_t familyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
            std::vector<VkQueueFamilyProperties> families(familyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());
            for (uint32_t index = 0; index < familyCount; ++index) {
                VkBool32 present = VK_FALSE;
                Check(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, index, surface, &present), "surface support");
                if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) { physical = candidate; family = index; break; }
            }
            if (physical) break;
        }
        if (!physical) throw std::runtime_error("No Vulkan device with a combined graphics/present queue");
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physical, &properties);
        LAMBUI_LOGI(TAG, "GPU: {}", properties.deviceName);
        float priority = 1;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        const char* swapchainExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = 1;
        deviceInfo.ppEnabledExtensionNames = &swapchainExtension;
        Check(vkCreateDevice(physical, &deviceInfo, nullptr, &device), "vkCreateDevice");
        vkGetDeviceQueue(device, family, 0, &queue);
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = family;
        Check(vkCreateCommandPool(device, &poolInfo, nullptr, &pool), "vkCreateCommandPool");
        VkCommandBufferAllocateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        commandInfo.commandPool = pool;
        commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandInfo.commandBufferCount = 1;
        Check(vkAllocateCommandBuffers(device, &commandInfo, &command), "vkAllocateCommandBuffers");
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        Check(vkCreateFence(device, &fenceInfo, nullptr, &fence), "vkCreateFence");
        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        Check(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &acquired), "vkCreateSemaphore");
        VkDescriptorSetLayoutBinding binding{};
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;
        Check(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorLayout), "descriptor layout");
        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128};
        VkDescriptorPoolCreateInfo descriptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        descriptorInfo.maxSets = 128;
        descriptorInfo.poolSizeCount = 1;
        descriptorInfo.pPoolSizes = &poolSize;
        Check(vkCreateDescriptorPool(device, &descriptorInfo, nullptr, &descriptorPool), "descriptor pool");
        VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Settings)};
        VkPipelineLayoutCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineInfo.setLayoutCount = 1;
        pipelineInfo.pSetLayouts = &descriptorLayout;
        pipelineInfo.pushConstantRangeCount = 1;
        pipelineInfo.pPushConstantRanges = &push;
        Check(vkCreatePipelineLayout(device, &pipelineInfo, nullptr, &pipelineLayout), "pipeline layout");
        push.size = sizeof(FieldSettings);
        pipelineInfo.setLayoutCount = 0;
        pipelineInfo.pSetLayouts = nullptr;
        Check(vkCreatePipelineLayout(device, &pipelineInfo, nullptr, &fieldLayout), "field pipeline layout");
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        Check(vkCreateSampler(device, &samplerInfo, nullptr, &sampler), "vkCreateSampler");
        Upload(1, 1, std::vector<uint8_t>{255, 255, 255, 255}, VK_FORMAT_R8G8B8A8_UNORM);
    }
    void Barrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags source,
        VkAccessFlags destination, VkPipelineStageFlags before, VkPipelineStageFlags after) {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcAccessMask = source;
        barrier.dstAccessMask = destination;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = ColorRange();
        vkCmdPipelineBarrier(command, before, after, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    size_t Upload(int width, int height, const std::vector<uint8_t>& pixels, VkFormat imageFormat) {
        LAMBUI_LOGT(TAG, "Upload texture {}x{}", width, height);
        if (active) throw std::logic_error("Upload textures before BeginFrame");
        const size_t channels = imageFormat == VK_FORMAT_R8_UNORM ? 1 : 4;
        if (width <= 0 || height <= 0 || pixels.size() != size_t(width) * size_t(height) * channels)
            throw std::invalid_argument("Invalid texture dimensions/pixels");
        if (textures.size() >= 128) throw std::runtime_error("Example descriptor pool is full (128 textures)");
        Check(vkDeviceWaitIdle(device), "texture upload idle");
        VkBuffer staging = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        textures.emplace_back();
        auto& texture = textures.back();
        try {
            CreateBuffer(pixels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging, stagingMemory);
            void* mapped = nullptr;
            Check(vkMapMemory(device, stagingMemory, 0, pixels.size(), 0, &mapped), "map texture staging");
            std::memcpy(mapped, pixels.data(), pixels.size());
            vkUnmapMemory(device, stagingMemory);
            VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.format = imageFormat;
            imageInfo.extent = {uint32_t(width), uint32_t(height), 1};
            imageInfo.mipLevels = imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            Check(vkCreateImage(device, &imageInfo, nullptr, &texture.image), "vkCreateImage");
            VkMemoryRequirements requirements{};
            vkGetImageMemoryRequirements(device, texture.image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(vkAllocateMemory(device, &allocation, nullptr, &texture.memory), "allocate texture");
            Check(vkBindImageMemory(device, texture.image, texture.memory, 0), "bind texture");
            Check(vkResetCommandBuffer(command, 0), "reset upload command");
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            Check(vkBeginCommandBuffer(command, &begin), "begin upload");
            Barrier(texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy region{};
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageExtent = imageInfo.extent;
            vkCmdCopyBufferToImage(command, staging, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
            Barrier(texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            Check(vkEndCommandBuffer(command), "end upload");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &command;
            Check(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "submit upload");
            Check(vkQueueWaitIdle(queue), "wait upload");
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = texture.image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = imageFormat;
            viewInfo.subresourceRange = ColorRange();
            Check(vkCreateImageView(device, &viewInfo, nullptr, &texture.view), "texture view");
            VkDescriptorSetAllocateInfo descriptor{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            descriptor.descriptorPool = descriptorPool;
            descriptor.descriptorSetCount = 1;
            descriptor.pSetLayouts = &descriptorLayout;
            Check(vkAllocateDescriptorSets(device, &descriptor, &texture.descriptor), "allocate texture descriptor");
            VkDescriptorImageInfo image{sampler, texture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = texture.descriptor;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo = &image;
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        } catch (...) {
            vkDeviceWaitIdle(device);
            if (staging) vkDestroyBuffer(device, staging, nullptr);
            if (stagingMemory) vkFreeMemory(device, stagingMemory, nullptr);
            throw;
        }
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        return textures.size() - 1;
    }
    void DestroySwapchain() {
        LAMBUI_LOGT(TAG, "Destroy swapchain resources");
        if (readback) vkDestroyBuffer(device, readback, nullptr);
        if (readbackMemory) vkFreeMemory(device, readbackMemory, nullptr);
        readback = VK_NULL_HANDLE;
        readbackMemory = VK_NULL_HANDLE;
        if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
        if (fieldPipeline) vkDestroyPipeline(device, fieldPipeline, nullptr);
        fieldPipeline = VK_NULL_HANDLE;
        for (auto framebuffer : framebuffers) if (framebuffer) vkDestroyFramebuffer(device, framebuffer, nullptr);
        framebuffers.clear();
        if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
        for (auto view : views) if (view) vkDestroyImageView(device, view, nullptr);
        views.clear();
        for (auto semaphore : finished) if (semaphore) vkDestroySemaphore(device, semaphore, nullptr);
        finished.clear();
        if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
    void CreatePipeline(bool field = false) {
        LAMBUI_LOGT(TAG, "Create graphics pipeline, field={}", field);
        std::array<VkShaderModule, 2> modules{};
        try {
            std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
            for (size_t index = 0; index < modules.size(); ++index) {
                const auto bytes = ReadShader(std::string(LAMBUI_VULKAN_ASSET_DIR) + (field ? "/field" : "/ui") + (index == 0 ? ".vert.spv" : ".frag.spv"));
                VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
                module.codeSize = bytes.size() * sizeof(uint32_t);
                module.pCode = bytes.data();
                Check(vkCreateShaderModule(device, &module, nullptr, &modules[index]), "create shader module");
                stages[index].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stages[index].stage = index == 0 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
                stages[index].module = modules[index];
                stages[index].pName = "main";
            }
            VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
            VkVertexInputAttributeDescription attributes[] = {{0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, x)},
                {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, u)}, {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(Vertex, color)}};
            VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            vertex.vertexBindingDescriptionCount = field ? 0 : 1;
            vertex.pVertexBindingDescriptions = &binding;
            vertex.vertexAttributeDescriptionCount = field ? 0 : 3;
            vertex.pVertexAttributeDescriptions = attributes;
            VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            viewport.viewportCount = viewport.scissorCount = 1;
            VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.cullMode = VK_CULL_MODE_NONE;
            raster.lineWidth = 1;
            VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState blend{};
            blend.blendEnable = VK_TRUE;
            blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.colorBlendOp = blend.alphaBlendOp = VK_BLEND_OP_ADD;
            blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.colorWriteMask = 15;
            VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            blending.attachmentCount = 1;
            blending.pAttachments = &blend;
            VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            dynamic.dynamicStateCount = 2;
            dynamic.pDynamicStates = dynamicStates;
            VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            info.stageCount = 2;
            info.pStages = stages.data();
            info.pVertexInputState = &vertex;
            info.pInputAssemblyState = &assembly;
            info.pViewportState = &viewport;
            info.pRasterizationState = &raster;
            info.pMultisampleState = &multisample;
            info.pColorBlendState = &blending;
            info.pDynamicState = &dynamic;
            info.layout = field ? fieldLayout : pipelineLayout;
            info.renderPass = renderPass;
            Check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, field ? &fieldPipeline : &pipeline), "create graphics pipeline");
        } catch (...) {
            for (auto module : modules) if (module) vkDestroyShaderModule(device, module, nullptr);
            throw;
        }
        for (auto module : modules) vkDestroyShaderModule(device, module, nullptr);
    }
    void CreateSwapchain() {
        LAMBUI_LOGT(TAG, "Create swapchain");
        Check(vkDeviceWaitIdle(device), "resize idle");
        DestroySwapchain();
        VkSurfaceCapabilitiesKHR capabilities{};
        Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &capabilities), "surface capabilities");
        uint32_t count = 0;
        Check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, nullptr), "surface formats");
        std::vector<VkSurfaceFormatKHR> formats(count);
        Check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, formats.data()), "surface formats");
        if (formats.empty()) throw std::runtime_error("Surface has no formats");
        auto selected = formats.front();
        for (const auto& candidate : formats) {
            if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM || candidate.format == VK_FORMAT_R8G8B8A8_UNORM) { selected = candidate; break; }
        }
        if (selected.format == VK_FORMAT_UNDEFINED) selected.format = VK_FORMAT_B8G8R8A8_UNORM;
        format = selected.format;
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        extent = capabilities.currentExtent;
        if (extent.width == UINT32_MAX) {
            extent.width = boost::algorithm::clamp(uint32_t(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            extent.height = boost::algorithm::clamp(uint32_t(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }
        count = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount) count = std::min(count, capabilities.maxImageCount);
        canCapture = (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) &&
            (format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB);
        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        info.surface = surface;
        info.minImageCount = count;
        info.imageFormat = format;
        info.imageColorSpace = selected.colorSpace;
        info.imageExtent = extent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (canCapture ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = capabilities.currentTransform;
        for (auto alpha : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                           VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR}) {
            if (capabilities.supportedCompositeAlpha & alpha) { info.compositeAlpha = alpha; break; }
        }
        info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        info.clipped = VK_TRUE;
        Check(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain), "create swapchain");
        Check(vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr), "swapchain images");
        images.resize(count);
        Check(vkGetSwapchainImagesKHR(device, swapchain, &count, images.data()), "swapchain images");
        VkAttachmentDescription attachment{};
        attachment.format = format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color;
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        pass.attachmentCount = 1;
        pass.pAttachments = &attachment;
        pass.subpassCount = 1;
        pass.pSubpasses = &subpass;
        pass.dependencyCount = 1;
        pass.pDependencies = &dependency;
        Check(vkCreateRenderPass(device, &pass, nullptr, &renderPass), "create render pass");
        views.resize(count, VK_NULL_HANDLE);
        framebuffers.resize(count, VK_NULL_HANDLE);
        finished.resize(count, VK_NULL_HANDLE);
        for (uint32_t index = 0; index < count; ++index) {
            VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            view.image = images[index];
            view.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view.format = format;
            view.subresourceRange = ColorRange();
            Check(vkCreateImageView(device, &view, nullptr, &views[index]), "swapchain view");
            VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            framebuffer.renderPass = renderPass;
            framebuffer.attachmentCount = 1;
            framebuffer.pAttachments = &views[index];
            framebuffer.width = extent.width;
            framebuffer.height = extent.height;
            framebuffer.layers = 1;
            Check(vkCreateFramebuffer(device, &framebuffer, nullptr, &framebuffers[index]), "create framebuffer");
            VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            Check(vkCreateSemaphore(device, &semaphore, nullptr, &finished[index]), "present semaphore");
        }
        CreatePipeline();
        CreatePipeline(true);
        recreate = false;
        LAMBUI_LOGI(TAG, "Swapchain {}x{}, {} images", extent.width, extent.height, count);
    }
};

VulkanExampleRenderer::VulkanExampleRenderer(GLFWwindow* window, bool validation) : m_state(std::make_unique<State>()) {
    LAMBUI_LOGT(TAG, "Construct");
    m_state->Init(window, validation);
}
VulkanExampleRenderer::~VulkanExampleRenderer() { LAMBUI_LOGT(TAG, "Destroy"); }
void VulkanExampleRenderer::LoadFont(const FontAtlas& atlas, void* handle) {
    LAMBUI_LOGT(TAG, "Load font {}", fmt::ptr(handle));
    if (m_state->fonts.count(handle)) throw std::invalid_argument("Font handle already registered");
    const size_t texture = m_state->Upload(atlas.GetAtlasWidth(), atlas.GetAtlasHeight(), atlas.GetAtlasPixels(), VK_FORMAT_R8_UNORM);
    m_state->fonts.emplace(handle, State::Font{&atlas, texture});
}
void* VulkanExampleRenderer::UploadTexture(int width, int height, const std::vector<uint8_t>& rgba) {
    LAMBUI_LOGT(TAG, "Upload RGBA texture");
    return reinterpret_cast<void*>(m_state->Upload(width, height, rgba, VK_FORMAT_R8G8B8A8_UNORM));
}
bool VulkanExampleRenderer::BeginFrame() {
    LAMBUI_LOGT(TAG, "Begin frame");
    auto& state = *m_state;
    if (state.active) throw std::logic_error("Frame already active");
    if (glfwGetWindowAttrib(state.window, GLFW_ICONIFIED)) return false;
    int width = 0, height = 0;
    glfwGetFramebufferSize(state.window, &width, &height);
    glfwGetWindowSize(state.window, &state.windowWidth, &state.windowHeight);
    if (width <= 0 || height <= 0 || state.windowWidth <= 0 || state.windowHeight <= 0) return false;
    Check(vkWaitForFences(state.device, 1, &state.fence, VK_TRUE, UINT64_MAX), "wait frame fence");
    if (!state.swapchain || state.recreate || uint32_t(width) != state.extent.width || uint32_t(height) != state.extent.height) state.CreateSwapchain();
    const auto acquired = vkAcquireNextImageKHR(state.device, state.swapchain, UINT64_MAX, state.acquired, VK_NULL_HANDLE, &state.imageIndex);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) { state.recreate = true; return false; }
    if (acquired == VK_SUBOPTIMAL_KHR) state.recreate = true;
    else Check(acquired, "acquire image");
    Check(vkResetCommandBuffer(state.command, 0), "reset frame command");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    Check(vkBeginCommandBuffer(state.command, &begin), "begin frame command");
    VkClearValue clear{};
    clear.color = {{0.055f, 0.067f, 0.063f, 1}};
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = state.renderPass;
    pass.framebuffer = state.framebuffers[state.imageIndex];
    pass.renderArea.extent = state.extent;
    pass.clearValueCount = 1;
    pass.pClearValues = &clear;
    vkCmdBeginRenderPass(state.command, &pass, VK_SUBPASS_CONTENTS_INLINE);
    state.active = true;
    state.submitted = false;
    return true;
}
void VulkanExampleRenderer::SubmitRenderCommands(const std::vector<UIRenderCommand>& commands) {
    LAMBUI_LOGT(TAG, "Submit {} commands", commands.size());
    auto& state = *m_state;
    if (!state.active || state.submitted) throw std::logic_error("Submit exactly one command bucket per active frame");
    state.submitted = true;
    struct Draw { uint32_t first, count; size_t texture; Settings settings; VkRect2D clip; const UIRenderCommand* callback; };
    std::vector<Vertex> vertices;
    std::vector<Draw> draws;
    std::vector<UIRect> clips{{0, 0, float(state.windowWidth), float(state.windowHeight)}};
    auto scissor = [&]() {
        const auto& clip = clips.back();
        const float scaleX = float(state.extent.width) / state.windowWidth, scaleY = float(state.extent.height) / state.windowHeight;
        const int left = int(std::floor(clip.x * scaleX)), top = int(std::floor(clip.y * scaleY));
        const int right = std::min(int(state.extent.width), int(std::ceil((clip.x + clip.width) * scaleX)));
        const int bottom = std::min(int(state.extent.height), int(std::ceil((clip.y + clip.height) * scaleY)));
        return VkRect2D{{left, top}, {clip.width > 0 ? uint32_t(std::max(0, right-left)) : 0,
                                    clip.height > 0 ? uint32_t(std::max(0, bottom-top)) : 0}};
    };
    auto appendQuad = [&](float x, float y, float width, float height, float u0, float v0, float u1, float v1, uint32_t rgba) {
        const uint32_t packed = (rgba >> 24) | ((rgba >> 8) & 0xFF00) | ((rgba << 8) & 0xFF0000) | (rgba << 24);
        const Vertex quad[] = {{x,y,u0,v0,packed}, {x+width,y,u1,v0,packed}, {x+width,y+height,u1,v1,packed},
                               {x,y,u0,v0,packed}, {x+width,y+height,u1,v1,packed}, {x,y+height,u0,v1,packed}};
        vertices.insert(vertices.end(), std::begin(quad), std::end(quad));
    };
    for (const auto& item : commands) {
        if (item.type == RenderCommandType::PushScissor) {
            const auto parent = clips.back();
            const float left = std::min(parent.x + parent.width, std::max(parent.x, item.x));
            const float top = std::min(parent.y + parent.height, std::max(parent.y, item.y));
            const float right = std::min(parent.x + parent.width, item.x + std::max(0.0f, item.width));
            const float bottom = std::min(parent.y + parent.height, item.y + std::max(0.0f, item.height));
            clips.push_back({left, top, std::max(0.0f, right-left), std::max(0.0f, bottom-top)});
            continue;
        }
        if (item.type == RenderCommandType::PopScissor) { if (clips.size() > 1) clips.pop_back(); continue; }
        Draw draw{uint32_t(vertices.size()), 0, 0, {float(state.windowWidth), float(state.windowHeight), 0, 0, 0}, scissor(), nullptr};
        if (item.type == RenderCommandType::CustomCallback) draw.callback = &item;
        else if (item.type == RenderCommandType::DrawQuad) {
            draw.texture = reinterpret_cast<uintptr_t>(item.textureHandle);
            if (draw.texture >= state.textures.size()) throw std::invalid_argument("Unknown Vulkan texture handle");
            appendQuad(item.x, item.y, item.width, item.height, item.u0, item.v0, item.u1, item.v1, item.color);
        } else if (item.type == RenderCommandType::DrawString) {
            auto font = state.fonts.find(item.fontHandle);
            if (font == state.fonts.end()) font = state.fonts.find(nullptr);
            if (font == state.fonts.end()) continue;
            const auto& atlas = *font->second.atlas;
            draw.texture = font->second.texture;
            draw.settings.mode = 1;
            draw.settings.distanceScale = atlas.GetPixelDistanceScale() / 255.0f;
            draw.settings.edge = atlas.GetOnEdgeValue() / 255.0f;
            float penX = item.x, penY = item.y + atlas.GetAscent();
            for (size_t offset = 0; offset < item.text.size();) {
                const auto first = static_cast<unsigned char>(item.text[offset++]);
                char32_t codepoint = first;
                if (first >= 0xC0) {
                    int extra = first < 0xE0 ? 1 : (first < 0xF0 ? 2 : 3);
                    codepoint = first & ((1 << (6-extra)) - 1);
                    while (extra-- > 0 && offset < item.text.size() && (static_cast<unsigned char>(item.text[offset]) & 0xC0) == 0x80)
                        codepoint = (codepoint << 6) | (static_cast<unsigned char>(item.text[offset++]) & 0x3F);
                }
                if (codepoint == '\n') { penX = item.x; penY += atlas.GetLineHeight(); continue; }
                if (codepoint == '\r') continue;
                const auto* glyph = atlas.FindGlyph(codepoint);
                if (!glyph) glyph = atlas.FindGlyph('?');
                if (!glyph) continue;
                if (glyph->width > 0 && glyph->height > 0) appendQuad(penX + glyph->bearingX, penY + glyph->bearingY, glyph->width, glyph->height,
                    glyph->u0, glyph->v0, glyph->u1, glyph->v1, item.color);
                penX += glyph->advance;
            }
        }
        draw.count = uint32_t(vertices.size()) - draw.first;
        draws.push_back(draw);
    }
    const size_t bytes = vertices.size() * sizeof(Vertex);
    if (bytes > state.vertexCapacity) {
        if (state.vertices) vkDestroyBuffer(state.device, state.vertices, nullptr);
        if (state.vertexMemory) vkFreeMemory(state.device, state.vertexMemory, nullptr);
        state.vertices = VK_NULL_HANDLE;
        state.vertexMemory = VK_NULL_HANDLE;
        state.vertexCapacity = std::max<size_t>(bytes, 65536);
        state.CreateBuffer(state.vertexCapacity, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, state.vertices, state.vertexMemory);
    }
    if (bytes) {
        void* mapped = nullptr;
        Check(vkMapMemory(state.device, state.vertexMemory, 0, bytes, 0, &mapped), "map vertices");
        std::memcpy(mapped, vertices.data(), bytes);
        vkUnmapMemory(state.device, state.vertexMemory);
    }
    for (const auto& draw : draws) {
        VkViewport viewport{0, 0, float(state.extent.width), float(state.extent.height), 0, 1};
        vkCmdSetViewport(state.command, 0, 1, &viewport);
        vkCmdSetScissor(state.command, 0, 1, &draw.clip);
        if (draw.callback) {
            const auto& item = *draw.callback;
            state.canvasBounds = {item.x, item.y, item.width, item.height};
            state.canvasClip = draw.clip;
            state.inCanvas = true;
            try {
                if (item.customRenderFunc) item.customRenderFunc({item.x, item.y, item.width, item.height, item.customRenderUserData});
            } catch (...) {
                state.inCanvas = false;
                throw;
            }
            state.inCanvas = false;
            continue;
        }
        if (!draw.count || !draw.clip.extent.width || !draw.clip.extent.height) continue;
        vkCmdBindPipeline(state.command, VK_PIPELINE_BIND_POINT_GRAPHICS, state.pipeline);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(state.command, 0, 1, &state.vertices, &offset);
        vkCmdBindDescriptorSets(state.command, VK_PIPELINE_BIND_POINT_GRAPHICS, state.pipelineLayout, 0, 1, &state.textures[draw.texture].descriptor, 0, nullptr);
        vkCmdPushConstants(state.command, state.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Settings), &draw.settings);
        vkCmdDraw(state.command, draw.count, 1, draw.first, 0);
    }
}
void VulkanExampleRenderer::EndFrame(bool capture) {
    LAMBUI_LOGT(TAG, "End frame, capture={}", capture);
    auto& state = *m_state;
    if (!state.active) throw std::logic_error("No active frame");
    if (capture && !state.canCapture) throw std::runtime_error("Surface does not support RGBA transfer readback");
    vkCmdEndRenderPass(state.command);
    if (capture) {
        if (!state.readback) state.CreateBuffer(VkDeviceSize(state.extent.width) * state.extent.height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, state.readback, state.readbackMemory);
        state.Barrier(state.images[state.imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {state.extent.width, state.extent.height, 1};
        vkCmdCopyImageToBuffer(state.command, state.images[state.imageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, state.readback, 1, &region);
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(state.command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, nullptr, 0, nullptr);
    }
    state.Barrier(state.images[state.imageIndex], capture ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, capture ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
        capture ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    Check(vkEndCommandBuffer(state.command), "end frame command");
    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &state.acquired;
    submit.pWaitDstStageMask = &stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &state.command;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &state.finished[state.imageIndex];
    Check(vkResetFences(state.device, 1, &state.fence), "reset frame fence");
    Check(vkQueueSubmit(state.queue, 1, &submit, state.fence), "submit frame");
    state.active = false;
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &state.finished[state.imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &state.swapchain;
    present.pImageIndices = &state.imageIndex;
    const auto result = vkQueuePresentKHR(state.queue, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) state.recreate = true;
    else Check(result, "present frame");
    if (capture) {
        Check(vkWaitForFences(state.device, 1, &state.fence, VK_TRUE, UINT64_MAX), "wait capture");
        state.captureWidth = state.extent.width;
        state.captureHeight = state.extent.height;
        state.capture.resize(size_t(state.extent.width) * state.extent.height * 4);
        void* mapped = nullptr;
        Check(vkMapMemory(state.device, state.readbackMemory, 0, state.capture.size(), 0, &mapped), "map capture");
        std::memcpy(state.capture.data(), mapped, state.capture.size());
        vkUnmapMemory(state.device, state.readbackMemory);
        if (state.format == VK_FORMAT_B8G8R8A8_UNORM || state.format == VK_FORMAT_B8G8R8A8_SRGB)
            for (size_t offset = 0; offset < state.capture.size(); offset += 4) std::swap(state.capture[offset], state.capture[offset+2]);
    }
}
void VulkanExampleRenderer::SaveCapture(const std::string& path) const {
    LAMBUI_LOGT(TAG, "Save capture {}", path);
    const auto& state = *m_state;
    if (state.capture.empty()) throw std::logic_error("No captured frame");
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << state.captureWidth << ' ' << state.captureHeight << "\n255\n";
    for (size_t offset = 0; offset < state.capture.size(); offset += 4) {
        output.write(reinterpret_cast<const char*>(state.capture.data() + offset), 3);
    }
    if (!output) throw std::runtime_error("Cannot write capture: " + path);
}
const std::vector<uint8_t>& VulkanExampleRenderer::GetCapture() const { return m_state->capture; }
uint32_t VulkanExampleRenderer::GetWidth() const { return m_state->extent.width; }
uint32_t VulkanExampleRenderer::GetHeight() const { return m_state->extent.height; }
unsigned VulkanExampleRenderer::GetValidationErrors() const { return m_state->validationErrors; }
VkCommandBuffer VulkanExampleRenderer::GetCommandBuffer() const { return m_state->active ? m_state->command : VK_NULL_HANDLE; }

void VulkanExampleRenderer::DrawField(float time, float strength) {
    auto& state = *m_state;
    if (!state.active || !state.inCanvas) throw std::logic_error("DrawField requires an active canvas callback");
    if (!std::isfinite(time) || !std::isfinite(strength)) throw std::invalid_argument("Field parameters must be finite");
    const auto& bounds = state.canvasBounds;
    if (bounds.width <= 0 || bounds.height <= 0) return;
    const float scaleX = float(state.extent.width) / state.windowWidth;
    const float scaleY = float(state.extent.height) / state.windowHeight;
    const auto parent = state.canvasClip;
    const int left = std::min(parent.offset.x + int(parent.extent.width), std::max(parent.offset.x, int(std::floor(bounds.x * scaleX))));
    const int top = std::min(parent.offset.y + int(parent.extent.height), std::max(parent.offset.y, int(std::floor(bounds.y * scaleY))));
    const int right = std::min(parent.offset.x + int(parent.extent.width), int(std::ceil((bounds.x + bounds.width) * scaleX)));
    const int bottom = std::min(parent.offset.y + int(parent.extent.height), int(std::ceil((bounds.y + bounds.height) * scaleY)));
    VkRect2D clip{{left, top}, {uint32_t(std::max(0, right-left)), uint32_t(std::max(0, bottom-top))}};
    if (!clip.extent.width || !clip.extent.height) return;
    const FieldSettings settings{bounds.x, bounds.y, bounds.width, bounds.height, float(state.windowWidth),
        float(state.windowHeight), time, boost::algorithm::clamp(strength, 0.0f, 2.0f)};
    vkCmdBindPipeline(state.command, VK_PIPELINE_BIND_POINT_GRAPHICS, state.fieldPipeline);
    vkCmdSetScissor(state.command, 0, 1, &clip);
    vkCmdPushConstants(state.command, state.fieldLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(settings), &settings);
    vkCmdDraw(state.command, 6, 1, 0, 0);
}
