#include "VulkanFrameResource.h"

// Standard.
#include <format>

// Custom.
#include "render/vulkan/VulkanRenderer.h"

// External.
#include "vulkan/vk_enum_string_helper.h"

namespace ne {

    VulkanFrameResource::~VulkanFrameResource() {
        // Convert renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pRenderer);
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            Error error("expected a Vulkan renderer");
            error.showError();
            return; // don't throw in destructor, just quit
        }

        // Get logical device.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            Error error("expected logical device to be valid");
            error.showError();
            return; // don't throw in destructor, just quit
        }

        // Destroy fence (if was created).
        if (pFence != nullptr) {
            vkDestroyFence(pLogicalDevice, pFence, nullptr);
            pFence = nullptr;
        }

        // Destroy semaphores (if were created).
        if (pSemaphoreSwapChainImageAcquired != nullptr) {
            vkDestroySemaphore(pLogicalDevice, pSemaphoreSwapChainImageAcquired, nullptr);
            pSemaphoreSwapChainImageAcquired = nullptr;
        }
        if (pSemaphoreSwapChainImageDrawingFinished != nullptr) {
            vkDestroySemaphore(pLogicalDevice, pSemaphoreSwapChainImageDrawingFinished, nullptr);
            pSemaphoreSwapChainImageDrawingFinished = nullptr;
        }

        if (pCommandBuffer != nullptr) {
            // Get command pool.
            const auto pCommandPool = pVulkanRenderer->getCommandPool();
            if (pCommandPool == nullptr) [[unlikely]] {
                Error error("expected command pool to be valid");
                error.showError();
                return; // don't throw in destructor, just quit
            }

            // Free command buffer.
            vkFreeCommandBuffers(pLogicalDevice, pCommandPool, 1, &pCommandBuffer);
            pCommandBuffer = nullptr;
        }
    }

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
        auto result = vkAllocateCommandBuffers(pLogicalDevice, &commandBuffersInfo, &pCommandBuffer);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create command buffer, error: {}", string_VkResult(result)));
        }

        // Describe fence.
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // mark as "all previous commands were executed"

        // Create fence.
        result = vkCreateFence(pLogicalDevice, &fenceInfo, nullptr, &pFence);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create a fence, error: {}", string_VkResult(result)));
        }

        // Describe semaphore.
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        // Create semaphore 1.
        result =
            vkCreateSemaphore(pLogicalDevice, &semaphoreInfo, nullptr, &pSemaphoreSwapChainImageAcquired);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create a semaphore, error: {}", string_VkResult(result)));
        }

        // Create semaphore 2.
        result = vkCreateSemaphore(
            pLogicalDevice, &semaphoreInfo, nullptr, &pSemaphoreSwapChainImageDrawingFinished);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create a semaphore, error: {}", string_VkResult(result)));
        }

        // Save renderer.
        this->pRenderer = pRenderer;

        return {};
    }

} // namespace ne
