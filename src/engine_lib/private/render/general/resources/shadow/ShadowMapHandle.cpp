#include "ShadowMapHandle.h"

// Custom.
#include "render/general/resources/shadow/ShadowMapManager.h"
#include "render/vulkan/VulkanRenderer.h"

// External.
#include "vulkan/vk_enum_string_helper.h"

namespace ne {

    ShadowMapHandle::~ShadowMapHandle() {
        std::scoped_lock guard(mtxResources.first);

        if (mtxResources.second.pDepthTexture == nullptr) [[unlikely]] {
            // Unexpected.
            Error error("shadow map handle has `nullptr` resource pointer");
            error.showError();
            return; // don't throw in destructor
        }

        // Get renderer.
        const auto pRenderer = pManager->getRenderer();

        // Notify manager.
        pManager->onShadowMapHandleBeingDestroyed(this);

        // Make sure we are running Vulkan renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pRenderer);
        if (pVulkanRenderer == nullptr) {
            // DirectX renderer does not need framebuffers.
            return;
        }

        // Get logical device.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            Error error("expected logical device to be valid");
            error.showError();
            return; // don't throw in destructor
        }

        // Create a short reference.
        auto& vFramebuffers = mtxResources.second.vShadowMappingFramebuffers;

        // Destroy previous framebuffers (if any).
        for (const auto& pFramebuffer : vFramebuffers) {
            vkDestroyFramebuffer(pLogicalDevice, pFramebuffer, nullptr);
        }
        vFramebuffers.clear();
    }

    ShadowMapHandle::ShadowMapHandle(
        ShadowMapManager* pManager,
        GpuResource* pDepthTexture,
        ShadowMapType type,
        size_t iTextureSize,
        const std::function<void(unsigned int)>& onArrayIndexChanged,
        GpuResource* pColorTexture)
        : onArrayIndexChanged(onArrayIndexChanged), shadowMapType(type) {
        // Save manager.
        this->pManager = pManager;

        // Just in case check for `nullptr`.
        if (pDepthTexture == nullptr) [[unlikely]] {
            // Unexpected.
            Error error("unexpected `nullptr` resource pointer");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Save resource and size.
        mtxResources.second.pDepthTexture = pDepthTexture;
        mtxResources.second.pColorTexture = pColorTexture;
        iShadowMapSize = iTextureSize;

        // Create framebuffers.
        recreateFramebuffers();
    }

    void ShadowMapHandle::changeArrayIndex(unsigned int iNewArrayIndex) {
        onArrayIndexChanged(iNewArrayIndex);
    }

    void ShadowMapHandle::setUpdatedResources(
        GpuResource* pDepthTexture, size_t iShadowMapSize, GpuResource* pColorTexture) {
        std::scoped_lock guard(mtxResources.first);

        // Save new data.
        this->iShadowMapSize = iShadowMapSize;
        mtxResources.second.pDepthTexture = pDepthTexture;
        mtxResources.second.pColorTexture = pColorTexture;

        // Update framebuffers.
        recreateFramebuffers();
    }

    void ShadowMapHandle::recreateFramebuffers() {
        // Make sure we are running Vulkan renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pManager->getRenderer());
        if (pVulkanRenderer == nullptr) {
            // DirectX renderer does not need framebuffers.
            return;
        }

        // Get logical device.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            Error error("expected logical device to be valid");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        std::scoped_lock guard(mtxResources.first);

        // Determine if we are handling point light shadows or not.
        const auto bIsColorTargetValid = mtxResources.second.pColorTexture != nullptr;

        // Get shadow mapping render pass.
        const auto pShadowMappingRenderPass =
            pVulkanRenderer->getShadowMappingRenderPass(bIsColorTargetValid);
        if (pShadowMappingRenderPass == nullptr) [[unlikely]] {
            Error error(std::format(
                "expected shadow mapping render pass to be valid (shadow handle \"{}\")",
                mtxResources.second.pDepthTexture->getResourceName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Create a short reference.
        auto& vFramebuffers = mtxResources.second.vShadowMappingFramebuffers;

        // Convert resources.
        const auto pDepthTexture = reinterpret_cast<VulkanResource*>(mtxResources.second.pDepthTexture);
        const auto pColorTexture = reinterpret_cast<VulkanResource*>(mtxResources.second.pColorTexture);

        // Destroy previous framebuffers (if any).
        for (const auto& pFramebuffer : vFramebuffers) {
            vkDestroyFramebuffer(pLogicalDevice, pFramebuffer, nullptr);
        }
        vFramebuffers.clear();

        // Describe framebuffer.
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = pShadowMappingRenderPass;
        framebufferInfo.width = static_cast<uint32_t>(iShadowMapSize);
        framebufferInfo.height = framebufferInfo.width;
        framebufferInfo.layers = 1;

        // Prepare array of attachments.
        std::vector<VkImageView> vAttachments;
        vAttachments.push_back(pDepthTexture->getInternalImageView());

        // Specify attachments.
        framebufferInfo.pAttachments = vAttachments.data();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(vAttachments.size());

        if (!bIsColorTargetValid) {
            // Prepare to create 1 framebuffer.
            vFramebuffers.resize(1);

            // Create framebuffer.
            const auto result =
                vkCreateFramebuffer(pLogicalDevice, &framebufferInfo, nullptr, vFramebuffers.data());
            if (result != VK_SUCCESS) [[unlikely]] {
                Error error(
                    std::format("failed to create a framebuffer, error: {}", string_VkResult(result)));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Name this framebuffer.
            VulkanRenderer::setObjectDebugOnlyName(
                pVulkanRenderer,
                vFramebuffers[0],
                VK_OBJECT_TYPE_FRAMEBUFFER,
                std::format(
                    "shadow mapping framebuffer for resource \"{}\"", pDepthTexture->getResourceName()));

            // Done.
            return;
        }

        // Prepare to create framebuffer per each cubemap face.
        vFramebuffers.resize(6); // NOLINT

        // Prepare 1 additional attachment.
        const size_t iCubemapViewAttachmentIndex = vAttachments.size();
        vAttachments.resize(vAttachments.size() + 1);

        // Update attachments info.
        framebufferInfo.pAttachments = vAttachments.data();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(vAttachments.size());

        for (size_t i = 0; i < vFramebuffers.size(); i++) {
            // Set cubemap face image view.
            vAttachments[iCubemapViewAttachmentIndex] = pColorTexture->getInternalCubemapImageView(i);

            // Create framebuffer.
            const auto result =
                vkCreateFramebuffer(pLogicalDevice, &framebufferInfo, nullptr, &vFramebuffers[i]);
            if (result != VK_SUCCESS) [[unlikely]] {
                Error error(
                    std::format("failed to create a framebuffer, error: {}", string_VkResult(result)));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Name this framebuffer.
            VulkanRenderer::setObjectDebugOnlyName(
                pVulkanRenderer,
                vFramebuffers[i],
                VK_OBJECT_TYPE_FRAMEBUFFER,
                std::format(
                    "shadow mapping framebuffer for cubemap face #{} for resource \"{}\"",
                    i,
                    pColorTexture->getResourceName()));
        }
    }

}
