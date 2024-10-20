#include "VulkanShadowMapArrayIndexManager.h"

// Custom.
#include "shader/general/DescriptorConstants.hpp"
#include "render/general/resource/GpuResource.h"
#include "render/vulkan/VulkanRenderer.h"
#include "render/general/resource/shadow/ShadowMapHandle.h"
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
        Renderer* pRenderer, const std::string& sShaderArrayResourceName)
        : ShadowMapArrayIndexManager(pRenderer, sShaderArrayResourceName) {
        // Create index manager.
        mtxInternalData.second.pIndexManager = std::make_unique<ShaderArrayIndexManager>(
            sShaderArrayResourceName, DescriptorConstants::iBindlessTextureArrayDescriptorCount);
    }

    std::optional<Error>
    VulkanShadowMapArrayIndexManager::registerShadowMapResource(ShadowMapHandle* pShadowMapHandle) {
        // Get resource.
        const auto pMtxResources = pShadowMapHandle->getResources();

        std::scoped_lock guard(mtxInternalData.first, pMtxResources->first);

        // Self check: make sure this resource was not registered yet.
        if (mtxInternalData.second.registeredShadowMaps.find(pShadowMapHandle) !=
            mtxInternalData.second.registeredShadowMaps.end()) [[unlikely]] {
            return Error(std::format(
                "\"{}\" was requested to register a shadow map handle \"{}\" but this shadow map was already "
                "registered",
                getShaderArrayResourceName(),
                pMtxResources->second.pDepthTexture->getResourceName()));
        }

        // Reserve a new index.
        auto pReservedIndex = mtxInternalData.second.pIndexManager->reserveIndex();

        // Save index value.
        const auto iIndex = pReservedIndex->getActualIndex();

        // Add registered pair.
        mtxInternalData.second.registeredShadowMaps[pShadowMapHandle] = std::move(pReservedIndex);

        // Notify shadow map user about array index initialized.
        changeShadowMapArrayIndex(pShadowMapHandle, iIndex);

        // Bind new shadow map to all pipelines that use shadow maps.
        auto optionalError = bindShadowMapsToAllPipelines(pShadowMapHandle);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error>
    VulkanShadowMapArrayIndexManager::unregisterShadowMapResource(ShadowMapHandle* pShadowMapHandle) {
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
        return bindShadowMapsToPipeline(pPipeline, nullptr);
    }

    std::optional<Error> VulkanShadowMapArrayIndexManager::bindShadowMapsToAllPipelines() {
        return bindShadowMapsToAllPipelines(nullptr);
    }

    std::optional<Error>
    VulkanShadowMapArrayIndexManager::bindShadowMapsToAllPipelines(ShadowMapHandle* pOnlyBindThisShadowMap) {
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
                    auto optionalError = bindShadowMapsToPipeline(pPipeline.get(), pOnlyBindThisShadowMap);
                    if (optionalError.has_value()) [[unlikely]] {
                        optionalError->addCurrentLocationToErrorStack();
                        return optionalError;
                    }
                }
            }
        }

        return {};
    }

    std::optional<Error> VulkanShadowMapArrayIndexManager::bindShadowMapsToPipeline(
        Pipeline* pPipeline, ShadowMapHandle* pOnlyBindThisShadowMap) {
        std::scoped_lock guard(mtxInternalData.first);

        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pPipeline->getRenderer());
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Convert pipeline.
        const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pPipeline);
        if (pVulkanPipeline == nullptr) [[unlikely]] {
            return Error("expected a Vulkan pipeline");
        }

        // Get shadow sampler.
        const auto pShadowTextureSampler = pVulkanRenderer->getShadowTextureSampler();
        if (pShadowTextureSampler == nullptr) [[unlikely]] {
            return Error("shadow texture sampler is `nullptr`");
        }

        if (pOnlyBindThisShadowMap == nullptr) {
            // Bind all registered shadow maps.
            for (const auto& [pShadowMapHandle, pReservedIndex] :
                 mtxInternalData.second.registeredShadowMaps) {
                // Bind shadow map to pipeline.
                auto optionalError =
                    bindShadowMapToPipelineIfUsed(pShadowMapHandle, pVulkanPipeline, pShadowTextureSampler);
                if (optionalError.has_value()) [[unlikely]] {
                    optionalError->addCurrentLocationToErrorStack();
                    return optionalError;
                }
            }
        } else {
            // Bind one shadow map.
            auto optionalError =
                bindShadowMapToPipelineIfUsed(pOnlyBindThisShadowMap, pVulkanPipeline, pShadowTextureSampler);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        return {};
    }

    std::optional<Error> VulkanShadowMapArrayIndexManager::bindShadowMapToPipelineIfUsed(
        ShadowMapHandle* pShadowMapHandle, VulkanPipeline* pPipeline, VkSampler pSampler) {
        // Make sure handle is not nullptr.
        if (pShadowMapHandle == nullptr) [[unlikely]] {
            return Error(std::format(
                "\"{}\" index manager received a `nullptr` as a shadow map handle",
                getShaderArrayResourceName()));
        }

        std::scoped_lock guard(mtxInternalData.first);

        // Make sure this handle is registered.
        const auto shadowMapIt = mtxInternalData.second.registeredShadowMaps.find(pShadowMapHandle);
        if (shadowMapIt == mtxInternalData.second.registeredShadowMaps.end()) [[unlikely]] {
            return Error(std::format(
                "\"{}\" index manager expected the specified shadow map handle to be already registered",
                getShaderArrayResourceName()));
        }

        {
            // Get resources.
            const auto pMtxResources = pShadowMapHandle->getResources();
            std::scoped_lock shadowHandleGuard(pMtxResources->first);

            // Determine which texture to bind.
            auto pImageToBind = pMtxResources->second.pDepthTexture;
            if (pMtxResources->second.pColorTexture != nullptr) {
                // Bind point light's cubemap instead (because for point lights "color" cubemap is used and
                // not depth image).
                pImageToBind = pMtxResources->second.pColorTexture;
            }

            // Bind.
            auto optionalError = pPipeline->bindImageIfUsed(
                pImageToBind,
                getShaderArrayResourceName(),
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                pSampler);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        return {};
    }

}
