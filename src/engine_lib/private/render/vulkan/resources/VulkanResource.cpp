#include "VulkanResource.h"

// Standard.
#include <stdexcept>
#include <format>

// Custom.
#include "render/vulkan/resources/VulkanResourceManager.h"
#include "io/Logger.h"
#include "render/vulkan/VulkanRenderer.h"
#include "render/vulkan/resources/KtxLoadingCallbackManager.h"

// External.
#include "vulkan/vk_enum_string_helper.h"

namespace ne {

    VulkanResource::VulkanResource(
        VulkanResourceManager* pResourceManager,
        const std::string& sResourceName,
        std::variant<VkBuffer, VkImage> pInternalResource,
        VmaAllocation pResourceMemory) {
        // Initialize fields.
        this->pResourceManager = pResourceManager;
        this->sResourceName = sResourceName;
        mtxResourceMemory.second = pResourceMemory;

        // Save resource.
        if (std::holds_alternative<VkBuffer>(pInternalResource)) {
            pBufferResource = std::get<VkBuffer>(pInternalResource);
        } else {
            pImageResource = std::get<VkImage>(pInternalResource);
        }

        // Increment counter of alive objects.
        pResourceManager->iAliveResourceCount.fetch_add(1);
    }

    VulkanResource::VulkanResource(
        VulkanResourceManager* pResourceManager,
        const std::string& sResourceName,
        ktxVulkanTexture ktxTexture) {
        // Initialize fields.
        this->pResourceManager = pResourceManager;
        this->sResourceName = sResourceName;
        mtxResourceMemory.second = nullptr;

        // Save resource.
        pImageResource = ktxTexture.image;

        // Save KTX image data.
        optionalKtxTexture = ktxTexture;

        // Increment counter of alive objects.
        pResourceManager->iAliveResourceCount.fetch_add(1);
    }

    VulkanResource::~VulkanResource() {
        // Don't log here to avoid spamming.

        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pResourceManager->getRenderer());
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

        // Make sure the GPU is not using this resource.
        pVulkanRenderer->waitForGpuToFinishWorkUpToThisPoint();

        std::scoped_lock guard(mtxResourceMemory.first);

        if (pImageResource != nullptr) {
            // Destroy image view.
            if (pImageView != nullptr) {
                vkDestroyImageView(pLogicalDevice, pImageView, nullptr);
            }

            // Destroy image.
            if (optionalKtxTexture.has_value()) {
                auto callbacks = KtxLoadingCallbackManager::getKtxSubAllocatorCallbacks();
                auto ktxTexture = optionalKtxTexture.value();
                ktxVulkanTexture_Destruct_WithSuballocator(
                    &ktxTexture, pVulkanRenderer->getLogicalDevice(), nullptr, &callbacks);
            } else {
                vmaDestroyImage(pResourceManager->pMemoryAllocator, pImageResource, mtxResourceMemory.second);
            }
        } else {
            // Destroy the resource and its memory.
            vmaDestroyBuffer(pResourceManager->pMemoryAllocator, pBufferResource, mtxResourceMemory.second);
        }

        // Decrement counter of alive objects.
        const auto iPreviousTotalResourceCount = pResourceManager->iAliveResourceCount.fetch_sub(1);

        // Self check: make sure the counter is not below zero.
        if (iPreviousTotalResourceCount == 0) [[unlikely]] {
            Logger::get().error("total alive Vulkan resource counter just went below zero");
        }
    }

    std::string VulkanResource::getResourceName() const { return sResourceName; }

    VulkanResourceManager* VulkanResource::getResourceManager() const { return pResourceManager; }

    std::variant<std::unique_ptr<VulkanResource>, Error> VulkanResource::create(
        VulkanResourceManager* pResourceManager,
        const std::string& sResourceName,
        VmaAllocator pMemoryAllocator,
        const VkBufferCreateInfo& bufferInfo,
        const VmaAllocationCreateInfo& allocationInfo) {
        // Prepare variables for created data.
        VkBuffer pCreatedBuffer = nullptr;
        VmaAllocation pCreatedMemory = nullptr;

        // Create buffer.
        const auto result = vmaCreateBuffer(
            pMemoryAllocator, &bufferInfo, &allocationInfo, &pCreatedBuffer, &pCreatedMemory, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to create buffer \"{}\", error: {}", sResourceName, string_VkResult(result)));
        }

        // Set allocation name.
        vmaSetAllocationName(pMemoryAllocator, pCreatedMemory, sResourceName.c_str());

        return std::unique_ptr<VulkanResource>(
            new VulkanResource(pResourceManager, sResourceName, pCreatedBuffer, pCreatedMemory));
    }

    std::variant<std::unique_ptr<VulkanResource>, Error> VulkanResource::create(
        VulkanResourceManager* pResourceManager,
        const std::string& sResourceName,
        VmaAllocator pMemoryAllocator,
        const VkImageCreateInfo& imageInfo,
        const VmaAllocationCreateInfo& allocationInfo,
        std::optional<VkImageAspectFlags> viewDescription) {
        // Prepare variables for created data.
        VkImage pCreatedImage = nullptr;
        VmaAllocation pCreatedMemory = nullptr;

        // Create image.
        const auto result = vmaCreateImage(
            pMemoryAllocator, &imageInfo, &allocationInfo, &pCreatedImage, &pCreatedMemory, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to create image \"{}\", error: {}", sResourceName, string_VkResult(result)));
        }

        // Set allocation name.
        vmaSetAllocationName(pMemoryAllocator, pCreatedMemory, sResourceName.c_str());

        auto pCreatedImageResource = std::unique_ptr<VulkanResource>(
            new VulkanResource(pResourceManager, sResourceName, pCreatedImage, pCreatedMemory));

        if (viewDescription.has_value()) {
            // Convert renderer.
            const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pResourceManager->getRenderer());
            if (pVulkanRenderer == nullptr) [[unlikely]] {
                return Error("expected a Vulkan renderer");
            }

            // Get logical device.
            const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
            if (pLogicalDevice == nullptr) [[unlikely]] {
                return Error("expected logical device to be valid");
            }

            // Describe image view.
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = pCreatedImageResource->pImageResource;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = imageInfo.format;
            viewInfo.subresourceRange.aspectMask = viewDescription.value();
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (imageInfo.imageType != VK_IMAGE_TYPE_2D && viewInfo.viewType == VK_IMAGE_VIEW_TYPE_2D)
                [[unlikely]] {
                return Error("image type / view type mismatch");
            }

            // Create image view.
            const auto result =
                vkCreateImageView(pLogicalDevice, &viewInfo, nullptr, &pCreatedImageResource->pImageView);
            if (result != VK_SUCCESS) [[unlikely]] {
                return Error(std::format("failed to create image view, error: {}", string_VkResult(result)));
            }
        }

        return pCreatedImageResource;
    }

    std::variant<std::unique_ptr<VulkanResource>, Error> VulkanResource::create(
        VulkanResourceManager* pResourceManager,
        const std::string& sResourceName,
        ktxVulkanTexture ktxTexture) {
        return std::unique_ptr<VulkanResource>(
            new VulkanResource(pResourceManager, sResourceName, ktxTexture));
    }

} // namespace ne
