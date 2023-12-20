#include "VulkanShadowMapArrayIndexManager.h"

// Custom.
#include "shader/general/DescriptorConstants.hpp"
#include "render/general/resources/GpuResource.h"
#include "render/vulkan/VulkanRenderer.h"
#include "render/general/resources/shadow/ShadowMapHandle.h"
#include "render/general/pipeline/PipelineManager.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"

namespace ne {

    VulkanShadowMapArrayIndexManager::~VulkanShadowMapArrayIndexManager() {
        std::scoped_lock guard(mtxInternalData.first);

        // Make sure no shadow map is registered.
        if (!mtxInternalData.second.registeredShadowMaps.empty()) [[unlikely]] {
            Error error(std::format(
                "\"{}\" index manager is being destroyed but there are still {} registered shadow map "
                "handle(s) alive",
                getShaderArrayResourceName(),
                mtxInternalData.second.registeredShadowMaps.size()));
            error.showError();
            return; // don't throw in destructor
        }
    }

    VulkanShadowMapArrayIndexManager::VulkanShadowMapArrayIndexManager(
        Renderer* pRenderer, const std::string& sArrayName)
        : ShadowMapArrayIndexManager(pRenderer, sArrayName) {
        // Create index manager.
        mtxInternalData.second.pIndexManager = std::make_unique<ShaderBindlessArrayIndexManager>(
            sArrayName, DescriptorConstants::iBindlessTextureArrayDescriptorCount);
    }

    std::optional<Error>
    VulkanShadowMapArrayIndexManager::registerShadowMap(ShadowMapHandle* pShadowMapHandle) {
        std::scoped_lock guard(mtxInternalData.first);

        // Self check: make sure this resource was not registered yet.
        if (mtxInternalData.second.registeredShadowMaps.find(pShadowMapHandle) !=
            mtxInternalData.second.registeredShadowMaps.end()) [[unlikely]] {
            return Error(std::format(
                "\"{}\" was requested to register a shadow map handle \"{}\" but this shadow map was already "
                "registered",
                getShaderArrayResourceName(),
                pShadowMapHandle->getResource()->getResourceName()));
        }

        // Reserve a new index.
        auto pReservedIndex = mtxInternalData.second.pIndexManager->reserveIndex();

        // Save index value.
        const auto iIndex = pReservedIndex->getActualIndex();

        // Add registered pair.
        mtxInternalData.second.registeredShadowMaps[pShadowMapHandle] = std::move(pReservedIndex);

        // Notify shadow map user about array index initialized.
        changeShadowMapArrayIndex(pShadowMapHandle, iIndex);

        return {};
    }

    std::optional<Error>
    VulkanShadowMapArrayIndexManager::unregisterShadowMap(ShadowMapHandle* pShadowMapHandle) {
        std::scoped_lock guard(mtxInternalData.first);

        // Make sure this shadow map was previously registered.
        const auto shadowMapIt = mtxInternalData.second.registeredShadowMaps.find(pShadowMapHandle);
        if (shadowMapIt == mtxInternalData.second.registeredShadowMaps.end()) [[unlikely]] {
            return Error(std::format(
                "\"{}\" index manager is unable to unregister the specified shadow map handle because it was "
                "not registered previously",
                getShaderArrayResourceName()));
        }

        // Remove handle (also frees reserved index).
        mtxInternalData.second.registeredShadowMaps.erase(shadowMapIt);

        return {};
    }

    std::optional<Error> VulkanShadowMapArrayIndexManager::bindShadowMapsToPipeline(Pipeline* pPipeline) {
        std::scoped_lock guard(mtxInternalData.first);

        // Convert pipeline.
        const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pPipeline);
        if (pVulkanPipeline == nullptr) [[unlikely]] {
            return Error("expected a Vulkan pipeline");
        }

        // Get pipeline's internal resources.
        const auto pMtxPipelineInternalResources = pVulkanPipeline->getInternalResources();
        std::scoped_lock pipelineResourcesGuard(pMtxPipelineInternalResources->first);

        // See if this pipeline uses the resource we are handling.
        auto it = pMtxPipelineInternalResources->second.resourceBindings.find(getShaderArrayResourceName());
        if (it == pMtxPipelineInternalResources->second.resourceBindings.end()) {
            // This pipeline does not use this resource.
            return {};
        }

        // Get Vulkan renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(getRenderer());
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Get logical device to be used later.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("logical device is `nullptr`");
        }

        // Get pipeline manager.
        const auto pPipelineManager = pVulkanRenderer->getPipelineManager();
        if (pPipelineManager == nullptr) [[unlikely]] {
            return Error("pipeline manager is `nullptr`");
        }

        // Get shadow sampler.
        const auto pShadowTextureSampler = pVulkanRenderer->getShadowTextureSampler();
        if (pShadowTextureSampler == nullptr) [[unlikely]] {
            return Error("shadow texture sampler is `nullptr`");
        }

        for (const auto& [pShadowMapHandle, pReservedIndex] : mtxInternalData.second.registeredShadowMaps) {
            // Prepare array of VkBuffers for light index lists.
            std::array<VkImageView, FrameResourcesManager::getFrameResourcesCount()> vImagesToBind;

            // Convert to Vulkan resource.
            const auto pVulkanResource = dynamic_cast<VulkanResource*>(pShadowMapHandle->getResource());
            if (pVulkanResource == nullptr) [[unlikely]] {
                return Error("expected a Vulkan resource");
            }

            // Fill array of images.
            for (size_t i = 0; i < vImagesToBind.size(); i++) {
                // Get resource image view.
                const auto pImageView = pVulkanResource->getInternalImageView();
                if (pImageView == nullptr) [[unlikely]] {
                    return Error(std::format(
                        "expected resource \"{}\" to have an image view",
                        pVulkanResource->getResourceName()));
                }

                vImagesToBind[i] = pImageView;
            }

            // Update one descriptor in set per frame resource.
            for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
                // Prepare info to bind an image view to descriptor.
                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                imageInfo.imageView = vImagesToBind[i];
                imageInfo.sampler = pShadowTextureSampler;

                // Bind reserved space to descriptor.
                VkWriteDescriptorSet descriptorUpdateInfo{};
                descriptorUpdateInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorUpdateInfo.dstSet =
                    pMtxPipelineInternalResources->second.vDescriptorSets[i]; // descriptor set to update
                descriptorUpdateInfo.dstBinding = it->second;                 // descriptor binding index
                descriptorUpdateInfo.dstArrayElement = 0; // first descriptor in array to update
                descriptorUpdateInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorUpdateInfo.descriptorCount = 1;     // how much descriptors in array to update
                descriptorUpdateInfo.pImageInfo = &imageInfo; // descriptor refers to image data

                // Update descriptor.
                vkUpdateDescriptorSets(pLogicalDevice, 1, &descriptorUpdateInfo, 0, nullptr);
            }
        }

        return {};
    }

    std::optional<Error> VulkanShadowMapArrayIndexManager::bindShadowMapsToAllPipelines() {
        std::scoped_lock guard(mtxInternalData.first);

        // Get pipeline manager.
        const auto pPipelineManager = getRenderer()->getPipelineManager();
        if (pPipelineManager == nullptr) [[unlikely]] {
            return Error("pipeline manager is `nullptr`");
        }

        // Get graphics pipelines.
        const auto pMtxGraphicsPipelines = pPipelineManager->getGraphicsPipelines();
        std::scoped_lock pipelinesGuard(pMtxGraphicsPipelines->first);

        // Iterate over graphics pipelines of all types.
        for (auto& pipelinesOfSpecificType : pMtxGraphicsPipelines->second.vPipelineTypes) {

            // Iterate over all active shader combinations.
            for (const auto& [sShaderNames, pipelines] : pipelinesOfSpecificType) {

                // Iterate over all active unique material macros combinations.
                for (const auto& [materialMacros, pPipeline] : pipelines.shaderPipelines) {
                    // Bind array to pipeline.
                    auto optionalError = bindShadowMapsToPipeline(pPipeline.get());
                    if (optionalError.has_value()) [[unlikely]] {
                        optionalError->addCurrentLocationToErrorStack();
                        return optionalError;
                    }
                }
            }
        }

        return {};
    }

}
