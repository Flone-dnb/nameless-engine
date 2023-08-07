#include "VulkanFrameResource.h"

// Custom.
#include "render/vulkan/VulkanRenderer.h"

// External.
#include "vulkan/vk_enum_string_helper.h"

namespace ne {

    std::optional<Error> VulkanFrameResource::initialize(Renderer* pRenderer) {
        // Convert renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pRenderer);
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Get logical device.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("expected logical device to be valid");
        }

        // Get command pool.
        const auto pCommandPool = pVulkanRenderer->getCommandPool();
        if (pCommandPool == nullptr) [[unlikely]] {
            return Error("expected command pool to be valid");
        }

        // Describe command buffer.
        VkCommandBufferAllocateInfo commandBuffersInfo{};
        commandBuffersInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBuffersInfo.commandPool = pCommandPool;
        commandBuffersInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBuffersInfo.commandBufferCount = 1;

        // Create command buffers.
        const auto result = vkAllocateCommandBuffers(pLogicalDevice, &commandBuffersInfo, &pCommandBuffer);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format("failed to create command buffer, error: {}", string_VkResult(result)));
        }

        return {};
    }

} // namespace ne
