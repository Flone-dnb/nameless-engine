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
        VmaAllocation pResourceMemory,
        unsigned int iElementSizeInBytes,
        unsigned int iElementCount)
        : GpuResource(pResourceManager, sResourceName, iElementSizeInBytes, iElementCount) {
        mtxResourceMemory.second = pResourceMemory;

        // Save resource.
        if (std::holds_alternative<VkBuffer>(pInternalResource)) {
            pBufferResource = std::get<VkBuffer>(pInternalResource);
        } else {
            pImageResource = std::get<VkImage>(pInternalResource);
        }
    }

    VulkanResource::VulkanResource(
        VulkanResourceManager* pResourceManager,
        const std::string& sResourceName,
        ktxVulkanTexture ktxTexture)
        : GpuResource(pResourceManager, sResourceName, 0, 0) {
        mtxResourceMemory.second = nullptr;

        // Save resource.
        pImageResource = ktxTexture.image;

        // Save KTX image data.
        optionalKtxTexture = ktxTexture;
    }

    VulkanResource::~VulkanResource() {
        // Don't log here to avoid spamming.

        // Get resource manager.
        const auto pResourceManager = dynamic_cast<VulkanResourceManager*>(getResourceManager());
        if (pResourceManager == nullptr) [[unlikely]] {
            Error error("invalid resource manager");
            error.showError();
            return; // don't throw in destructor
        }

        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pResourceManager->getRenderer());
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            Error error("expected a Vulkan renderer");
            error.showError();
            return; // don't throw in destructor
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

        if (pShadowMappingFramebuffer != nullptr) {
            // Destroy shadow mapping framebuffer.
            vkDestroyFramebuffer(pLogicalDevice, pShadowMappingFramebuffer, nullptr);
            pShadowMappingFramebuffer = nullptr;
        }

        if (pImageResource != nullptr) {
            // Destroy image view.
            if (pImageView != nullptr) {
                vkDestroyImageView(pLogicalDevice, pImageView, nullptr);
            }
            if (pDepthAspectImageView != nullptr) {
                vkDestroyImageView(pLogicalDevice, pDepthAspectImageView, nullptr);
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
    }

    std::variant<std::unique_ptr<VulkanResource>, Error> VulkanResource::create(
        VulkanResourceManager* pResourceManager,
        const std::string& sResourceName,
        VmaAllocator pMemoryAllocator,
        const VkBufferCreateInfo& bufferInfo,
        const VmaAllocationCreateInfo& allocationInfo,
        unsigned int iElementSizeInBytes,
        unsigned int iElementCount) {
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

        return std::unique_ptr<VulkanResource>(new VulkanResource(
            pResourceManager,
            sResourceName,
            pCreatedBuffer,
            pCreatedMemory,
            iElementSizeInBytes,
            iElementCount));
    }

    std::variant<std::unique_ptr<VulkanResource>, Error> VulkanResource::create(
        VulkanResourceManager* pResourceManager,
        const std::string& sResourceName,
        VmaAllocator pMemoryAllocator,
        const VkImageCreateInfo& imageInfo,
        const VmaAllocationCreateInfo& allocationInfo,
        std::optional<VkImageAspectFlags> viewDescription,
        bool bIsCubeMapView,
        bool bCreateShadowMappingFramebuffer) {
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
            new VulkanResource(pResourceManager, sResourceName, pCreatedImage, pCreatedMemory, 0, 0));

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
            viewInfo.viewType = bIsCubeMapView ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = imageInfo.format;
            viewInfo.subresourceRange.aspectMask = viewDescription.value();
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = imageInfo.arrayLayers;

            if (imageInfo.imageType != VK_IMAGE_TYPE_2D && viewInfo.viewType == VK_IMAGE_VIEW_TYPE_2D)
                [[unlikely]] {
                return Error(std::format("image type / view type mismatch on image \"{}\"", sResourceName));
            }

            // Create image view.
            auto result =
                vkCreateImageView(pLogicalDevice, &viewInfo, nullptr, &pCreatedImageResource->pImageView);
            if (result != VK_SUCCESS) [[unlikely]] {
                return Error(std::format(
                    "failed to create image view for image \"{}\", error: {}",
                    sResourceName,
                    string_VkResult(result)));
            }

            if ((viewDescription.value() & VK_IMAGE_ASPECT_DEPTH_BIT) != 0 &&
                (viewDescription.value() & VK_IMAGE_ASPECT_STENCIL_BIT) != 0) {
                // Create a depth only view.
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

                // Create image view.
                result = vkCreateImageView(
                    pLogicalDevice, &viewInfo, nullptr, &pCreatedImageResource->pDepthAspectImageView);
                if (result != VK_SUCCESS) [[unlikely]] {
                    return Error(std::format(
                        "failed to create depth aspect image view for image \"{}\", error: {}",
                        sResourceName,
                        string_VkResult(result)));
                }
            }

            if (bCreateShadowMappingFramebuffer) {
                // Get shadow mapping render pass.
                const auto pShadowMappingRenderPass = pVulkanRenderer->getShadowMappingRenderPass();
                if (pShadowMappingRenderPass == nullptr) [[unlikely]] {
                    return Error(std::format(
                        "expected shadow mapping render pass to be valid when creation image \"{}\"",
                        sResourceName));
                }

                // Describe framebuffer.
                VkFramebufferCreateInfo framebufferInfo{};
                framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = pShadowMappingRenderPass;
                framebufferInfo.attachmentCount = 1;
                framebufferInfo.pAttachments = &pCreatedImageResource->pImageView;
                framebufferInfo.width = imageInfo.extent.width;
                framebufferInfo.height = imageInfo.extent.height;
                framebufferInfo.layers = 1;

                // Create main render pass framebuffer.
                auto result = vkCreateFramebuffer(
                    pLogicalDevice,
                    &framebufferInfo,
                    nullptr,
                    &pCreatedImageResource->pShadowMappingFramebuffer);
                if (result != VK_SUCCESS) [[unlikely]] {
                    return Error(std::format(
                        "failed to create a framebuffer for a swapchain image view, error: {}",
                        string_VkResult(result)));
                }
            }
        } else if (bCreateShadowMappingFramebuffer) [[unlikely]] {
            return Error(std::format(
                "possible error found during image \"{}\" creation: image view creation info is not "
                "specified but a framebuffer creation was requested",
                sResourceName));
        }

        return pCreatedImageResource;
    }

    std::variant<std::unique_ptr<VulkanResource>, Error> VulkanResource::create(
        VulkanResourceManager* pResourceManager,
        const std::string& sResourceName,
        ktxVulkanTexture ktxTexture) {
        // Create resource.
        auto pCreatedResource =
            std::unique_ptr<VulkanResource>(new VulkanResource(pResourceManager, sResourceName, ktxTexture));

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
        viewInfo.image = pCreatedResource->pImageResource;
        viewInfo.viewType = ktxTexture.viewType;
        viewInfo.format = ktxTexture.imageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = ktxTexture.levelCount;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = ktxTexture.layerCount;

        // Create image view.
        const auto result =
            vkCreateImageView(pLogicalDevice, &viewInfo, nullptr, &pCreatedResource->pImageView);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create image view, error: {}", string_VkResult(result)));
        }

        return pCreatedResource;
    }

} // namespace ne
