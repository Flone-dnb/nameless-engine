#include "LightingShaderResourceManager.h"

// Custom.
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif
#include "render/vulkan/VulkanRenderer.h"
#include "render/general/pipeline/PipelineManager.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"
#include "shader/ComputeShaderInterface.h"
#include "shader/general/EngineShaderConstantMacros.hpp"
#include "shader/general/EngineShaderNames.hpp"
#include "misc/Profiler.hpp"

namespace ne {

    ShaderLightArray* LightingShaderResourceManager::getPointLightDataArray() const {
        return lightArrays.pPointLightDataArray.get();
    }

    ShaderLightArray* LightingShaderResourceManager::getDirectionalLightDataArray() const {
        return lightArrays.pDirectionalLightDataArray.get();
    }

    ShaderLightArray* LightingShaderResourceManager::getSpotlightDataArray() const {
        return lightArrays.pSpotlightDataArray.get();
    }

    std::optional<Error> LightingShaderResourceManager::bindDescriptorsToRecreatedPipelineResources() {
        // Notify point light array.
        auto optionalError = lightArrays.pPointLightDataArray->updateBindingsInAllPipelines();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Notify directional light array.
        optionalError = lightArrays.pDirectionalLightDataArray->updateBindingsInAllPipelines();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Notify spotlight array.
        optionalError = lightArrays.pSpotlightDataArray->updateBindingsInAllPipelines();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

#if defined(DEBUG) && defined(WIN32)
        static_assert(sizeof(LightArrays) == 24, "consider notifying new arrays here"); // NOLINT
#elif defined(DEBUG)
        static_assert(sizeof(LightArrays) == 24, "consider notifying new arrays here"); // NOLINT
#endif

        // Rebind general lighting data.
        optionalError = rebindGpuDataToAllPipelines();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error>
    LightingShaderResourceManager::updateDescriptorsForPipelineResource(Pipeline* pPipeline) {
        // Notify point light array.
        auto optionalError = lightArrays.pPointLightDataArray->updatePipelineBinding(pPipeline);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Notify directional light array.
        optionalError = lightArrays.pDirectionalLightDataArray->updatePipelineBinding(pPipeline);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Notify spotlight array.
        optionalError = lightArrays.pSpotlightDataArray->updatePipelineBinding(pPipeline);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

#if defined(DEBUG) && defined(WIN32)
        static_assert(sizeof(LightArrays) == 24, "consider notifying new arrays here"); // NOLINT
#elif defined(DEBUG)
        static_assert(sizeof(LightArrays) == 24, "consider notifying new arrays here"); // NOLINT
#endif

        // Rebind general lighting data.
        optionalError = rebindGpuDataToPipeline(pPipeline);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::pair<std::recursive_mutex, LightingShaderResourceManager::GpuData>*
    LightingShaderResourceManager::getInternalResources() {
        return &mtxGpuData;
    }

    void LightingShaderResourceManager::updateResources(
        FrameResource* pCurrentFrameResource, size_t iCurrentFrameResourceIndex) {
        PROFILE_FUNC;

        // Notify light arrays.
        lightArrays.pPointLightDataArray->updateSlotsMarkedAsNeedsUpdate(iCurrentFrameResourceIndex);
        lightArrays.pDirectionalLightDataArray->updateSlotsMarkedAsNeedsUpdate(iCurrentFrameResourceIndex);
        lightArrays.pSpotlightDataArray->updateSlotsMarkedAsNeedsUpdate(iCurrentFrameResourceIndex);

#if defined(DEBUG) && defined(WIN32)
        static_assert(sizeof(LightArrays) == 24, "consider notifying new arrays here"); // NOLINT
#elif defined(DEBUG)
        static_assert(sizeof(LightArrays) == 24, "consider notifying new arrays here"); // NOLINT
#endif

        // Copy general lighting info (maybe changed, since that data is very small it should be OK to
        // update it every frame).
        copyDataToGpu(iCurrentFrameResourceIndex);

        // Get point light arrays (we don't really need to lock mutexes here since we are inside
        // `drawNextFrame`).
        auto& pointLightArrayData = lightArrays.pPointLightDataArray->getInternalResources()->second;
        auto& spotlightArrayData = lightArrays.pSpotlightDataArray->getInternalResources()->second;

        // Get point lights.
        const auto pPointLightArrayResource =
            pointLightArrayData.vGpuArrayLightDataResources[iCurrentFrameResourceIndex]
                ->getInternalResource();

        // Get spot lights.
        const auto pSpotlightArrayResource =
            spotlightArrayData.vGpuArrayLightDataResources[iCurrentFrameResourceIndex]->getInternalResource();

        // Get indices to non-culled point lights.
        const auto pNonCulledPointLightsIndicesArrayResource =
            pointLightArrayData.lightsInFrustum.vGpuResources[iCurrentFrameResourceIndex]
                ->getInternalResource();

        // Get indices to non-culled spotlights.
        const auto pNonCulledSpotlightsIndicesArrayResource =
            spotlightArrayData.lightsInFrustum.vGpuResources[iCurrentFrameResourceIndex]
                ->getInternalResource();

        // Queue shader that will reset global counters for light culling (should be called every frame).
        pPrepareLightCullingComputeInterface->submitForExecution(1, 1, 1);

        // Queue light culling shader (should be called every frame).
        std::scoped_lock dataGuard(mtxGpuData.first);
        auto optionalError = lightCullingComputeShaderData.queueExecutionForNextFrame(
            pRenderer,
            pCurrentFrameResource,
            iCurrentFrameResourceIndex,
            mtxGpuData.second.vGeneralDataGpuResources[iCurrentFrameResourceIndex]->getInternalResource(),
            pPointLightArrayResource,
            pSpotlightArrayResource,
            pNonCulledPointLightsIndicesArrayResource,
            pNonCulledSpotlightsIndicesArrayResource);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }
    }

    void LightingShaderResourceManager::onPointLightArraySizeChanged(size_t iNewSize) {
        // Pause the rendering and make sure our resources are not used by the GPU
        // because we will update data in all GPU resources
        // (locking both mutexes to avoid a deadlock)
        // (most likely light array resizing already did that but do it again just to be extra sure).
        std::scoped_lock drawingGuard(*pRenderer->getRenderResourcesMutex(), mtxGpuData.first);
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Self check: make sure the number of light sources will not hit type limit.
        if (iNewSize >= std::numeric_limits<unsigned int>::max()) [[unlikely]] {
            Error error(std::format("new point light array size of {} will exceed type limit", iNewSize));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Update total point light count.
        mtxGpuData.second.generalData.iPointLightCountInCameraFrustum = static_cast<unsigned int>(iNewSize);

        // Copy updated data to the GPU resources.
        for (size_t i = 0; i < mtxGpuData.second.vGeneralDataGpuResources.size(); i++) {
            copyDataToGpu(i);
        }
    }

    void LightingShaderResourceManager::onPointLightsInFrustumCulled(size_t iCurrentFrameResourceIndex) {
        // Prepare a short reference.
        auto& lightArrayData = lightArrays.pPointLightDataArray->mtxResources;

        // Lock both general and specified light array data.
        std::scoped_lock guard(mtxGpuData.first, lightArrayData.first);

        // Make sure we don't hit type limit.
        if (lightArrayData.second.lightsInFrustum.vLightIndicesInFrustum.size() >
            std::numeric_limits<unsigned int>::max()) [[unlikely]] {
            Error error(std::format(
                "point light index array size {} exceed type limit",
                lightArrayData.second.lightsInFrustum.vLightIndicesInFrustum.size()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Copy new array size to general data.
        mtxGpuData.second.generalData.iPointLightCountInCameraFrustum =
            static_cast<unsigned int>(lightArrayData.second.lightsInFrustum.vLightIndicesInFrustum.size());

        // Copy general data to GPU resource of the current frame resource.
        copyDataToGpu(iCurrentFrameResourceIndex);
    }

    void LightingShaderResourceManager::onDirectionalLightArraySizeChanged(size_t iNewSize) {
        // Pause the rendering and make sure our resources are not used by the GPU
        // because we will update data in all GPU resources
        // (locking both mutexes to avoid a deadlock)
        // (most likely light array resizing already did that but do it again just to be extra sure).
        std::scoped_lock drawingGuard(*pRenderer->getRenderResourcesMutex(), mtxGpuData.first);
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Self check: make sure the number of light sources will not hit type limit.
        if (iNewSize >= std::numeric_limits<unsigned int>::max()) [[unlikely]] {
            Error error(
                std::format("new directional light array size of {} will exceed type limit", iNewSize));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Update total directional light count.
        mtxGpuData.second.generalData.iDirectionalLightCount = static_cast<unsigned int>(iNewSize);

        // Copy updated data to the GPU resources.
        for (size_t i = 0; i < mtxGpuData.second.vGeneralDataGpuResources.size(); i++) {
            copyDataToGpu(i);
        }
    }

    void LightingShaderResourceManager::onSpotlightArraySizeChanged(size_t iNewSize) {
        // Pause the rendering and make sure our resources are not used by the GPU
        // because we will update data in all GPU resources
        // (locking both mutexes to avoid a deadlock)
        // (most likely light array resizing already did that but do it again just to be extra sure).
        std::scoped_lock drawingGuard(*pRenderer->getRenderResourcesMutex(), mtxGpuData.first);
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Self check: make sure the number of light sources will not hit type limit.
        if (iNewSize >= std::numeric_limits<unsigned int>::max()) [[unlikely]] {
            Error error(std::format("new spotlight array size of {} will exceed type limit", iNewSize));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Update total spotlight count.
        mtxGpuData.second.generalData.iSpotLightCountInCameraFrustum = static_cast<unsigned int>(iNewSize);

        // Copy updated data to the GPU resources.
        for (size_t i = 0; i < mtxGpuData.second.vGeneralDataGpuResources.size(); i++) {
            copyDataToGpu(i);
        }
    }

    void LightingShaderResourceManager::onSpotlightsInFrustumCulled(size_t iCurrentFrameResourceIndex) {
        // Prepare a short reference.
        auto& lightArrayData = lightArrays.pSpotlightDataArray->mtxResources;

        // Lock both general and specified light array data.
        std::scoped_lock guard(mtxGpuData.first, lightArrayData.first);

        // Make sure we don't hit type limit.
        if (lightArrayData.second.lightsInFrustum.vLightIndicesInFrustum.size() >
            std::numeric_limits<unsigned int>::max()) [[unlikely]] {
            Error error(std::format(
                "spotlight index array size {} exceed type limit",
                lightArrayData.second.lightsInFrustum.vLightIndicesInFrustum.size()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Copy new array size to general data.
        mtxGpuData.second.generalData.iSpotLightCountInCameraFrustum =
            static_cast<unsigned int>(lightArrayData.second.lightsInFrustum.vLightIndicesInFrustum.size());

        // Copy general data to GPU resource of the current frame resource.
        copyDataToGpu(iCurrentFrameResourceIndex);
    }

    void LightingShaderResourceManager::copyDataToGpu(size_t iCurrentFrameResourceIndex) {
        std::scoped_lock guard(mtxGpuData.first);

        // Copy.
        mtxGpuData.second.vGeneralDataGpuResources[iCurrentFrameResourceIndex]->copyDataToElement(
            0, &mtxGpuData.second.generalData, sizeof(GeneralLightingShaderData));
    }

    std::optional<Error> LightingShaderResourceManager::rebindGpuDataToAllPipelines() {
        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pRenderer);
        if (pVulkanRenderer == nullptr) {
            // Under DirectX we will bind SRV to a specific root signature index inside of the `draw`
            // function.
            return {};
        }

        // Get pipeline manager.
        const auto pPipelineManager = pRenderer->getPipelineManager();
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
                    // Rebind resources to pipeline.
                    auto optionalError = rebindGpuDataToPipeline(pPipeline.get());
                    if (optionalError.has_value()) [[unlikely]] {
                        optionalError->addCurrentLocationToErrorStack();
                        return optionalError;
                    }
                }
            }
        }

        return {};
    }

    std::optional<Error> LightingShaderResourceManager::rebindGpuDataToPipeline(Pipeline* pPipeline) {
        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pRenderer);
        if (pVulkanRenderer == nullptr) {
            // Under DirectX we will bind SRV to a specific root signature index inside of the `draw`
            // function.
            return {};
        }

        // Get logical device to be used later.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("logical device is `nullptr`");
        }

        // Convert to a Vulkan pipeline.
        const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pPipeline);
        if (pVulkanPipeline == nullptr) [[unlikely]] {
            return Error("expected a Vulkan pipeline");
        }

        // Lock resources.
        std::scoped_lock guard(mtxGpuData.first);

        // Self check: make sure GPU resources are valid.
        for (const auto& pUploadBuffer : mtxGpuData.second.vGeneralDataGpuResources) {
            if (pUploadBuffer == nullptr) [[unlikely]] {
                return Error("expected the GPU resources to be created");
            }
        }

        // Get internal GPU resources.
        std::array<VkBuffer, FrameResourcesManager::getFrameResourcesCount()> vGeneralLightingDataBuffers;
        for (size_t i = 0; i < vGeneralLightingDataBuffers.size(); i++) {
            // Convert to Vulkan resource.
            const auto pVulkanResource = dynamic_cast<VulkanResource*>(
                mtxGpuData.second.vGeneralDataGpuResources[i]->getInternalResource());
            if (pVulkanResource == nullptr) [[unlikely]] {
                return Error("expected a Vulkan resource");
            }

            // Save buffer resource.
            vGeneralLightingDataBuffers[i] = pVulkanResource->getInternalBufferResource();
        }

        // Rebind general lighting info resource.
        auto optionalError = rebindBufferResourceToPipeline(
            pVulkanPipeline,
            pLogicalDevice,
            sGeneralLightingDataShaderResourceName,
            sizeof(GeneralLightingShaderData),
            generalLightingDataDescriptorType,
            vGeneralLightingDataBuffers);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Self check: make sure at least one light index list is valid.
        if (lightCullingComputeShaderData.resources.pOpaquePointLightIndexList == nullptr) {
            // Not initialized yet.
            return {};
        }

        // Prepare a lambda to bind resource.
        const auto bindLightIndexListResource = [&](const std::string& sShaderResourceName,
                                                    GpuResource* pResourceToBind) -> std::optional<Error> {
            // Prepare array of VkBuffers for light index lists.
            std::array<VkBuffer, FrameResourcesManager::getFrameResourcesCount()> vBuffersToBind;

            // Convert to Vulkan resource.
            const auto pVulkanResource = dynamic_cast<VulkanResource*>(pResourceToBind);
            if (pVulkanResource == nullptr) [[unlikely]] {
                return Error("expected a Vulkan resource");
            }

            // Fill array of buffers.
            for (size_t i = 0; i < vBuffersToBind.size(); i++) {
                vBuffersToBind[i] = pVulkanResource->getInternalBufferResource();
            }

            // Rebind to pipeline.
            return rebindBufferResourceToPipeline(
                pVulkanPipeline,
                pLogicalDevice,
                sShaderResourceName,
                pResourceToBind->getElementCount() * pResourceToBind->getElementSizeInBytes(),
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                vBuffersToBind);
        };

        // Bind opaque point light index list.
        optionalError = bindLightIndexListResource(
            "opaquePointLightIndexList",
            lightCullingComputeShaderData.resources.pOpaquePointLightIndexList.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Bind opaque spot light index list.
        optionalError = bindLightIndexListResource(
            "opaqueSpotLightIndexList",
            lightCullingComputeShaderData.resources.pOpaqueSpotLightIndexList.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Bind opaque directional light index list.
        optionalError = bindLightIndexListResource(
            "opaqueDirectionalLightIndexList",
            lightCullingComputeShaderData.resources.pOpaqueDirectionalLightIndexList.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Bind transparent point light index list.
        optionalError = bindLightIndexListResource(
            "transparentPointLightIndexList",
            lightCullingComputeShaderData.resources.pTransparentPointLightIndexList.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Bind transparent spot light index list.
        optionalError = bindLightIndexListResource(
            "transparentSpotLightIndexList",
            lightCullingComputeShaderData.resources.pTransparentSpotLightIndexList.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Bind transparent directional light index list.
        optionalError = bindLightIndexListResource(
            "transparentDirectionalLightIndexList",
            lightCullingComputeShaderData.resources.pTransparentDirectionalLightIndexList.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Prepare a lambda to bind resource.
        const auto bindLightGridResource = [&](const std::string& sShaderResourceName,
                                               GpuResource* pResourceToBind) -> std::optional<Error> {
            // Prepare array of VkBuffers for light index lists.
            std::array<VkImageView, FrameResourcesManager::getFrameResourcesCount()> vImagesToBind;

            // Convert to Vulkan resource.
            const auto pVulkanResource = dynamic_cast<VulkanResource*>(pResourceToBind);
            if (pVulkanResource == nullptr) [[unlikely]] {
                return Error("expected a Vulkan resource");
            }

            // Fill array of images.
            for (size_t i = 0; i < vImagesToBind.size(); i++) {
                vImagesToBind[i] = pVulkanResource->getInternalImageView();
            }

            // Rebind to pipeline.
            return rebindImageResourceToPipeline(
                pVulkanPipeline,
                pLogicalDevice,
                sShaderResourceName,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                VK_IMAGE_LAYOUT_GENERAL,
                pVulkanRenderer->getComputeTextureSampler(),
                vImagesToBind);
        };

        // Bind opaque directional light grid.
        optionalError = bindLightGridResource(
            "opaquePointLightGrid", lightCullingComputeShaderData.resources.pOpaquePointLightGrid.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Bind opaque spot light grid.
        optionalError = bindLightGridResource(
            "opaqueSpotLightGrid", lightCullingComputeShaderData.resources.pOpaqueSpotLightGrid.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Bind opaque directional light grid.
        optionalError = bindLightGridResource(
            "opaqueDirectionalLightGrid",
            lightCullingComputeShaderData.resources.pOpaqueDirectionalLightGrid.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Bind transparent point light grid.
        optionalError = bindLightGridResource(
            "transparentPointLightGrid",
            lightCullingComputeShaderData.resources.pTransparentPointLightGrid.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Bind transparent spot light grid.
        optionalError = bindLightGridResource(
            "transparentSpotLightGrid",
            lightCullingComputeShaderData.resources.pTransparentSpotLightGrid.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Bind transparent directional light grid.
        optionalError = bindLightGridResource(
            "transparentDirectionalLightGrid",
            lightCullingComputeShaderData.resources.pTransparentDirectionalLightGrid.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> LightingShaderResourceManager::rebindBufferResourceToPipeline(
        VulkanPipeline* pVulkanPipeline,
        VkDevice pLogicalDevice,
        const std::string& sShaderResourceName,
        VkDeviceSize iResourceSize,
        VkDescriptorType descriptorType,
        std::array<VkBuffer, FrameResourcesManager::getFrameResourcesCount()> vBuffersToBind) {
        // Get pipeline's internal resources.
        const auto pMtxPipelineInternalResources = pVulkanPipeline->getInternalResources();
        std::scoped_lock pipelineResourcesGuard(pMtxPipelineInternalResources->first);

        // See if this pipeline uses the resource we are handling.
        auto it = pMtxPipelineInternalResources->second.resourceBindings.find(sShaderResourceName);
        if (it == pMtxPipelineInternalResources->second.resourceBindings.end()) {
            // This pipeline does not use the specified resource.
            return {};
        }

        // Update one descriptor in set per frame resource.
        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
            // Prepare info to bind storage buffer slot to descriptor.
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = vBuffersToBind[i];
            bufferInfo.offset = 0;
            bufferInfo.range = iResourceSize;

            // Bind reserved space to descriptor.
            VkWriteDescriptorSet descriptorUpdateInfo{};
            descriptorUpdateInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorUpdateInfo.dstSet =
                pMtxPipelineInternalResources->second.vDescriptorSets[i]; // descriptor set to update
            descriptorUpdateInfo.dstBinding = it->second;                 // descriptor binding index
            descriptorUpdateInfo.dstArrayElement = 0; // first descriptor in array to update
            descriptorUpdateInfo.descriptorType = descriptorType;
            descriptorUpdateInfo.descriptorCount = 1;       // how much descriptors in array to update
            descriptorUpdateInfo.pBufferInfo = &bufferInfo; // descriptor refers to buffer data

            // Update descriptor.
            vkUpdateDescriptorSets(pLogicalDevice, 1, &descriptorUpdateInfo, 0, nullptr);
        }

        return {};
    }

    std::optional<Error> LightingShaderResourceManager::rebindImageResourceToPipeline(
        VulkanPipeline* pVulkanPipeline,
        VkDevice pLogicalDevice,
        const std::string& sShaderResourceName,
        VkDescriptorType descriptorType,
        VkImageLayout imageLayout,
        VkSampler pSampler,
        std::array<VkImageView, FrameResourcesManager::getFrameResourcesCount()> vImagesToBind) {
        // Get pipeline's internal resources.
        const auto pMtxPipelineInternalResources = pVulkanPipeline->getInternalResources();
        std::scoped_lock pipelineResourcesGuard(pMtxPipelineInternalResources->first);

        // See if this pipeline uses the resource we are handling.
        auto it = pMtxPipelineInternalResources->second.resourceBindings.find(sShaderResourceName);
        if (it == pMtxPipelineInternalResources->second.resourceBindings.end()) {
            // This pipeline does not use the specified resource.
            return {};
        }

        // Update one descriptor in set per frame resource.
        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
            // Prepare info to bind an image view to descriptor.
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = imageLayout;
            imageInfo.imageView = vImagesToBind[i];
            imageInfo.sampler = pSampler;

            // Bind reserved space to descriptor.
            VkWriteDescriptorSet descriptorUpdateInfo{};
            descriptorUpdateInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorUpdateInfo.dstSet =
                pMtxPipelineInternalResources->second.vDescriptorSets[i]; // descriptor set to update
            descriptorUpdateInfo.dstBinding = it->second;                 // descriptor binding index
            descriptorUpdateInfo.dstArrayElement = 0; // first descriptor in array to update
            descriptorUpdateInfo.descriptorType = descriptorType;
            descriptorUpdateInfo.descriptorCount = 1;     // how much descriptors in array to update
            descriptorUpdateInfo.pImageInfo = &imageInfo; // descriptor refers to image data

            // Update descriptor.
            vkUpdateDescriptorSets(pLogicalDevice, 1, &descriptorUpdateInfo, 0, nullptr);
        }

        return {};
    }

    LightingShaderResourceManager::LightingShaderResourceManager(Renderer* pRenderer) {
        // Save renderer.
        this->pRenderer = pRenderer;

        // Pause the rendering and make sure our resources are not used by the GPU
        // (locking both mutexes to avoid a deadlock).
        std::scoped_lock drawingGuard(*pRenderer->getRenderResourcesMutex(), mtxGpuData.first);
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Get resource manager.
        const auto pResourceManager = pRenderer->getResourceManager();
        if (pResourceManager == nullptr) [[unlikely]] {
            Error error("expected resource manager to be valid");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Prepare data size.
        const auto iDataSizeInBytes = sizeof(GeneralLightingShaderData);
        constexpr auto bUseFastButSmallShaderResource = true;
        static_assert(
            iDataSizeInBytes < 1024 * 62 && bUseFastButSmallShaderResource == true && // NOLINT
                generalLightingDataDescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            "we can no longer use fast shader resource: update boolean and descriptor type");

        // Create GPU resources.
        for (size_t i = 0; i < mtxGpuData.second.vGeneralDataGpuResources.size(); i++) {
            // Create a new resource with the specified size.
            auto result = pResourceManager->createResourceWithCpuWriteAccess(
                std::format("lighting general data frame #{}", i),
                iDataSizeInBytes,
                1,
                !bUseFastButSmallShaderResource);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
            mtxGpuData.second.vGeneralDataGpuResources[i] =
                std::get<std::unique_ptr<UploadBuffer>>(std::move(result));
        }

        // Copy initial data to the GPU resource.
        for (size_t i = 0; i < mtxGpuData.second.vGeneralDataGpuResources.size(); i++) {
            copyDataToGpu(i);
        }

#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            // Bind CBV to the created resource.
            for (auto& pUploadBuffer : mtxGpuData.second.vGeneralDataGpuResources) {
                // Convert to DirectX resource.
                const auto pDirectXResource =
                    dynamic_cast<DirectXResource*>(pUploadBuffer->getInternalResource());
                if (pDirectXResource == nullptr) [[unlikely]] {
                    Error error("expected a DirectX resource");
                    error.showError();
                    throw std::runtime_error(error.getFullErrorMessage());
                }

                // Bind CBV.
                auto optionalError = pDirectXResource->bindDescriptor(DirectXDescriptorType::CBV);
                if (optionalError.has_value()) [[unlikely]] {
                    optionalError->addCurrentLocationToErrorStack();
                    optionalError->showError();
                    throw std::runtime_error(optionalError->getFullErrorMessage());
                }
            }
        }
#endif

        // (Re)bind the (re)created resource to descriptors of all pipelines that use this resource.
        auto optionalError = rebindGpuDataToAllPipelines();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }

        // Create point light array.
        lightArrays.pPointLightDataArray = ShaderLightArray::create(
            pRenderer,
            sPointLightsShaderResourceName,
            [this](size_t iNewSize) { onPointLightArraySizeChanged(iNewSize); },
            std::pair<std::function<void(size_t)>, std::string>{
                [this](size_t iCurrentFrameResourceIndex) {
                    onPointLightsInFrustumCulled(iCurrentFrameResourceIndex);
                },
                sPointLightsInCameraFrustumIndicesShaderResourceName});

        // Create directional light array.
        lightArrays.pDirectionalLightDataArray = ShaderLightArray::create(
            pRenderer,
            sDirectionalLightsShaderResourceName,
            [this](size_t iNewSize) { onDirectionalLightArraySizeChanged(iNewSize); },
            {});

        // Create spotlight array.
        lightArrays.pSpotlightDataArray = ShaderLightArray::create(
            pRenderer,
            sSpotlightsShaderResourceName,
            [this](size_t iNewSize) { onSpotlightArraySizeChanged(iNewSize); },
            std::pair<std::function<void(size_t)>, std::string>{
                [this](size_t iCurrentFrameResourceIndex) {
                    onSpotlightsInFrustumCulled(iCurrentFrameResourceIndex);
                },
                sSpotlightsInCameraFrustumIndicesShaderResourceName});

#if defined(DEBUG) && defined(WIN32)
        static_assert(sizeof(LightArrays) == 24, "consider creating new arrays here"); // NOLINT
#elif defined(DEBUG)
        static_assert(sizeof(LightArrays) == 24, "consider creating new arrays here"); // NOLINT
#endif
    }

    std::optional<Error> LightingShaderResourceManager::ComputeShaderData::FrustumGridComputeShader::
        ComputeShader::updateDataAndSubmitShader(
            Renderer* pRenderer,
            const std::pair<unsigned int, unsigned int>& renderTargetSize,
            const glm::mat4& inverseProjectionMatrix) {
        // Make sure engine shaders were compiled and we created compute interface.
        if (pComputeInterface == nullptr) [[unlikely]] {
            return Error("expected compute interface to be created at this point");
        }

        // Make sure the GPU is not using resources that we will update.
        std::scoped_lock renderGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Get tile size (this value also describes the number of threads in one thread group).
        size_t iTileSizeInPixels = 0;
        try {
            iTileSizeInPixels =
                std::stoull(EngineShaderConstantMacros::ForwardPlus::getLightGridTileSizeMacro().second);
        } catch (const std::exception& exception) {
            return Error(std::format(
                "failed to convert light grid tile size to an integer, error: {}", exception.what()));
        };

        // Calculate tile count.
        const auto iTileCountX = static_cast<unsigned int>(
            std::ceil(static_cast<float>(renderTargetSize.first) / static_cast<float>(iTileSizeInPixels)));
        const auto iTileCountY = static_cast<unsigned int>(
            std::ceil(static_cast<float>(renderTargetSize.second) / static_cast<float>(iTileSizeInPixels)));

        // Calculate frustum count.
        const size_t iFrustumCount = iTileCountX * iTileCountY;

        // Calculate thread group count (we should dispatch 1 thread per tile).
        const auto iThreadGroupCountX = static_cast<unsigned int>(
            std::ceil(static_cast<float>(iTileCountX) / static_cast<float>(iTileSizeInPixels)));
        const auto iThreadGroupCountY = static_cast<unsigned int>(
            std::ceil(static_cast<float>(iTileCountY) / static_cast<float>(iTileSizeInPixels)));

        // Update compute info resource.
        ComputeInfo computeInfo;
        computeInfo.iThreadGroupCountX = iThreadGroupCountX;
        computeInfo.iThreadGroupCountY = iThreadGroupCountY;
        computeInfo.iTileCountX = iTileCountX;
        computeInfo.iTileCountY = iTileCountY;
        resources.pComputeInfo->copyDataToElement(0, &computeInfo, sizeof(computeInfo));

        // Update screen to view resource.
        ScreenToViewData screenToViewData;
        screenToViewData.iRenderTargetWidth = renderTargetSize.first;
        screenToViewData.iRenderTargetHeight = renderTargetSize.second;
        screenToViewData.inverseProjectionMatrix = inverseProjectionMatrix;
        resources.pScreenToViewData->copyDataToElement(0, &screenToViewData, sizeof(screenToViewData));

        // Recreate resource to store array of frustums with new size.
        auto frustumsResourceResult = pRenderer->getResourceManager()->createResource(
            "light grid of frustums",
            sizeof(ComputeShaderData::Frustum),
            iFrustumCount,
            ResourceUsageType::ARRAY_BUFFER,
            true);
        if (std::holds_alternative<Error>(frustumsResourceResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(frustumsResourceResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        resources.pCalculatedFrustums =
            std::get<std::unique_ptr<GpuResource>>(std::move(frustumsResourceResult));

        // Rebind GPU resource for frustums because we recreated it.
        auto optionalError = pComputeInterface->bindResource(
            resources.pCalculatedFrustums.get(),
            sCalculatedFrustumsShaderResourceName,
            ComputeResourceUsage::READ_WRITE_ARRAY_BUFFER);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Queue frustum grid recalculation shader.
        pComputeInterface->submitForExecution(iThreadGroupCountX, iThreadGroupCountY, 1);

        // Save tile count to be used by light culling shader.
        iLastUpdateTileCountX = iTileCountX;
        iLastUpdateTileCountY = iTileCountY;

        return {};
    }

    std::optional<Error>
    LightingShaderResourceManager::ComputeShaderData::FrustumGridComputeShader::ComputeShader::initialize(
        Renderer* pRenderer) {
        // Make sure the struct is not initialized yet.
        if (bIsInitialized) [[unlikely]] {
            return Error("already initialized");
        }

        // Create compute interface for calculating grid of frustums for light culling.
        auto computeCreationResult = ComputeShaderInterface::createUsingGraphicsQueue(
            pRenderer,
            EngineShaderNames::ForwardPlus::getCalculateFrustumGridComputeShaderName(),
            ComputeExecutionStage::AFTER_DEPTH_PREPASS,
            ComputeExecutionGroup::FIRST); // runs before light culling compute shader
        if (std::holds_alternative<Error>(computeCreationResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(computeCreationResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        pComputeInterface =
            std::get<std::unique_ptr<ComputeShaderInterface>>(std::move(computeCreationResult));

        // Create compute info resource for shader.
        auto result = pRenderer->getResourceManager()->createResourceWithCpuWriteAccess(
            "light grid of frustums - compute info",
            sizeof(ComputeShaderData::FrustumGridComputeShader::ComputeInfo),
            1,
            false);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        resources.pComputeInfo = std::get<std::unique_ptr<UploadBuffer>>(std::move(result));

        // Bind the resource.
        auto optionalError = pComputeInterface->bindResource(
            resources.pComputeInfo->getInternalResource(),
            sComputeInfoShaderResourceName,
            ComputeResourceUsage::CONSTANT_BUFFER);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create screen to view resource.
        result = pRenderer->getResourceManager()->createResourceWithCpuWriteAccess(
            "light grid of frustums - screen to view data",
            sizeof(ComputeShaderData::FrustumGridComputeShader::ScreenToViewData),
            1,
            false);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        resources.pScreenToViewData = std::get<std::unique_ptr<UploadBuffer>>(std::move(result));

        // Bind the resource.
        optionalError = pComputeInterface->bindResource(
            resources.pScreenToViewData->getInternalResource(),
            sScreenToViewDataShaderResourceName,
            ComputeResourceUsage::CONSTANT_BUFFER);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Resource for calculated frustums will be created when we will update resources.

        // Done.
        bIsInitialized = true;

        return {};
    }

    std::optional<Error>
    LightingShaderResourceManager::ComputeShaderData::LightCullingComputeShader::ComputeShader::initialize(
        Renderer* pRenderer, const FrustumGridComputeShader::ComputeShader& frustumGridShader) {
        // Make sure the struct is not initialized yet.
        if (bIsInitialized) [[unlikely]] {
            return Error("already initialized");
        }

        // Create compute interface for light culling.
        auto computeCreationResult = ComputeShaderInterface::createUsingGraphicsQueue(
            pRenderer,
            EngineShaderNames::ForwardPlus::getLightCullingComputeShaderName(),
            ComputeExecutionStage::AFTER_DEPTH_PREPASS,
            ComputeExecutionGroup::SECOND); // runs after compute shader that calculates grid frustums and
                                            // after shader that resets global counters
        if (std::holds_alternative<Error>(computeCreationResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(computeCreationResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        pComputeInterface =
            std::get<std::unique_ptr<ComputeShaderInterface>>(std::move(computeCreationResult));

        // Create thread group info resource for shader.
        auto result = pRenderer->getResourceManager()->createResourceWithCpuWriteAccess(
            "light culling - thread group count",
            sizeof(ComputeShaderData::LightCullingComputeShader::ThreadGroupCount),
            1,
            false);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        resources.pThreadGroupCount = std::get<std::unique_ptr<UploadBuffer>>(std::move(result));

        // Bind thread group count resource.
        auto optionalError = pComputeInterface->bindResource(
            resources.pThreadGroupCount->getInternalResource(),
            sThreadGroupCountShaderResourceName,
            ComputeResourceUsage::CONSTANT_BUFFER);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create resource.
        auto countersResourceResult = pRenderer->getResourceManager()->createResource(
            "light culling - global counters",
            sizeof(ComputeShaderData::LightCullingComputeShader::GlobalCountersIntoLightIndexList),
            1,
            ResourceUsageType::ARRAY_BUFFER,
            true);
        if (std::holds_alternative<Error>(countersResourceResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(countersResourceResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        resources.pGlobalCountersIntoLightIndexList =
            std::get<std::unique_ptr<GpuResource>>(std::move(countersResourceResult));

        // Bind global counters.
        optionalError = pComputeInterface->bindResource(
            resources.pGlobalCountersIntoLightIndexList.get(),
            sGlobalCountersIntoLightIndexListShaderResourceName,
            ComputeResourceUsage::READ_WRITE_ARRAY_BUFFER);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Bind screen to view data.
        optionalError = pComputeInterface->bindResource(
            frustumGridShader.resources.pScreenToViewData->getInternalResource(),
            FrustumGridComputeShader::ComputeShader::sScreenToViewDataShaderResourceName,
            ComputeResourceUsage::CONSTANT_BUFFER);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Done.
        bIsInitialized = true;

        return {};
    }

    std::variant<bool, Error>
    LightingShaderResourceManager::ComputeShaderData::LightCullingComputeShader::ComputeShader::
        createLightIndexListsAndLightGrid(Renderer* pRenderer, size_t iTileCountX, size_t iTileCountY) {
        // Make sure tile count was changed.
        if (iTileCountX == resources.iLightGridTileCountX && iTileCountY == resources.iLightGridTileCountY) {
            // Tile count was not changed - nothing to do.
            return false;
        }

        // First convert all constants (shader macros) from strings to integers.
        size_t iAveragePointLightNumPerTile = 0;
        size_t iAverageSpotLightNumPerTile = 0;
        size_t iAverageDirectionalLightNumPerTile = 0;
        try {
            // Point lights.
            iAveragePointLightNumPerTile = std::stoull(
                EngineShaderConstantMacros::ForwardPlus::getAveragePointLightNumPerTileMacro().second);

            // Spotlights.
            iAverageSpotLightNumPerTile = std::stoull(
                EngineShaderConstantMacros::ForwardPlus::getAverageSpotLightNumPerTileMacro().second);

            // Directional lights.
            iAverageDirectionalLightNumPerTile = std::stoull(
                EngineShaderConstantMacros::ForwardPlus::getAverageDirectionalLightNumPerTileMacro().second);
        } catch (const std::exception& exception) {
            return Error(std::format("failed to convert string to integer, error: {}", exception.what()));
        }

        // Calculate light index list sizes.
        const auto iLightGridSize = iTileCountX * iTileCountY;
        const auto iPointLightIndexListSize = iLightGridSize * iAveragePointLightNumPerTile;
        const auto iSpotLightIndexListSize = iLightGridSize * iAverageSpotLightNumPerTile;
        const auto iDirectionalLightIndexListSize = iLightGridSize * iAverageDirectionalLightNumPerTile;

        // Get resource manager.
        const auto pResourceManager = pRenderer->getResourceManager();

        // Prepare pointers to light index lists to create.
        struct LightIndexListCreationInfo {
            LightIndexListCreationInfo(
                std::unique_ptr<GpuResource>* pResource,
                const std::string& sResourceDescription,
                const std::string& sShaderResourceName,
                size_t iResourceElementCount) {
                this->pResource = pResource;
                this->sResourceDescription = sResourceDescription;
                this->sShaderResourceName = sShaderResourceName;
                this->iResourceElementCount = iResourceElementCount;
            }

            std::unique_ptr<GpuResource>* pResource = nullptr;
            std::string sResourceDescription;
            std::string sShaderResourceName;
            size_t iResourceElementCount = 0;
        };
        const std::array<LightIndexListCreationInfo, 6> vResourcesToCreate = {
            LightIndexListCreationInfo(
                &resources.pOpaquePointLightIndexList,
                "opaque point",
                "opaquePointLightIndexList",
                iPointLightIndexListSize),
            LightIndexListCreationInfo(
                &resources.pOpaqueSpotLightIndexList,
                "opaque spot",
                "opaqueSpotLightIndexList",
                iSpotLightIndexListSize),
            LightIndexListCreationInfo(
                &resources.pOpaqueDirectionalLightIndexList,
                "opaque directional",
                "opaqueDirectionalLightIndexList",
                iDirectionalLightIndexListSize),
            LightIndexListCreationInfo(
                &resources.pTransparentPointLightIndexList,
                "transparent point",
                "transparentPointLightIndexList",
                iPointLightIndexListSize),
            LightIndexListCreationInfo(
                &resources.pTransparentSpotLightIndexList,
                "transparent spot",
                "transparentSpotLightIndexList",
                iSpotLightIndexListSize),
            LightIndexListCreationInfo(
                &resources.pTransparentDirectionalLightIndexList,
                "transparent directional",
                "transparentDirectionalLightIndexList",
                iDirectionalLightIndexListSize)};

        // Create light index lists.
        for (const auto& info : vResourcesToCreate) {
            // Create resource.
            auto result = pResourceManager->createResource(
                std::format("light culling - {} light index list", info.sResourceDescription),
                info.iResourceElementCount,
                sizeof(unsigned int),
                ResourceUsageType::ARRAY_BUFFER,
                true);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            (*info.pResource) = std::get<std::unique_ptr<GpuResource>>(std::move(result));

            // Bind to shader.
            auto optionalError = pComputeInterface->bindResource(
                info.pResource->get(),
                info.sShaderResourceName,
                ComputeResourceUsage::READ_WRITE_ARRAY_BUFFER);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }
        }

        // Prepare pointers to light grids to create.
        struct LightGridCreationInfo {
            LightGridCreationInfo(
                std::unique_ptr<GpuResource>* pResource,
                const std::string& sResourceDescription,
                const std::string& sShaderResourceName) {
                this->pResource = pResource;
                this->sResourceDescription = sResourceDescription;
                this->sShaderResourceName = sShaderResourceName;
            }

            std::unique_ptr<GpuResource>* pResource = nullptr;
            std::string sResourceDescription;
            std::string sShaderResourceName;
        };
        const std::array<LightGridCreationInfo, 6> vGridsToCreate = {
            LightGridCreationInfo(&resources.pOpaquePointLightGrid, "opaque point", "opaquePointLightGrid"),
            LightGridCreationInfo(&resources.pOpaqueSpotLightGrid, "opaque spot", "opaqueSpotLightGrid"),
            LightGridCreationInfo(
                &resources.pOpaqueDirectionalLightGrid, "opaque directional", "opaqueDirectionalLightGrid"),
            LightGridCreationInfo(
                &resources.pTransparentPointLightGrid, "transparent point", "transparentPointLightGrid"),
            LightGridCreationInfo(
                &resources.pTransparentSpotLightGrid, "transparent spot", "transparentSpotLightGrid"),
            LightGridCreationInfo(
                &resources.pTransparentDirectionalLightGrid,
                "transparent directional",
                "transparentDirectionalLightGrid")};

        // Create light grids.
        for (const auto& info : vGridsToCreate) {
            // Create texture.
            auto result = pResourceManager->createShaderReadWriteTextureResource(
                std::format("light culling - {} light grid", info.sResourceDescription),
                static_cast<unsigned int>(iTileCountX),
                static_cast<unsigned int>(iTileCountY),
                ShaderReadWriteTextureResourceFormat::R32G32_UINT);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            (*info.pResource) = std::get<std::unique_ptr<GpuResource>>(std::move(result));

            // Bind to shader.
            auto optionalError = pComputeInterface->bindResource(
                info.pResource->get(), info.sShaderResourceName, ComputeResourceUsage::READ_WRITE_TEXTURE);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }
        }

        // Save new tile count.
        resources.iLightGridTileCountX = iTileCountX;
        resources.iLightGridTileCountY = iTileCountY;

        return true;
    }

    std::optional<Error> LightingShaderResourceManager::ComputeShaderData::LightCullingComputeShader::
        ComputeShader::queueExecutionForNextFrame(
            Renderer* pRenderer,
            FrameResource* pCurrentFrameResource,
            size_t iCurrentFrameResourceIndex,
            GpuResource* pGeneralLightingData,
            GpuResource* pPointLightArray,
            GpuResource* pSpotlightArray,
            GpuResource* pNonCulledPointLightsIndicesArray,
            GpuResource* pNonCulledSpotlightsIndicesArray) {
        PROFILE_FUNC;

        // Resources that store:
        // - calculated grid of frustums
        // - light grids
        // - light index lists
        // were binded by lighting shader resource manager (after frustum grid shader was updated).

        // Resources that store:
        // - screen to view data
        // - thread group count
        // were binded in our initialize function.

        // Get renderer's depth texture pointer.
        const auto pDepthTexture = pRenderer->getDepthTextureNoMultisampling();
        if (pDepthTexture == nullptr) [[unlikely]] {
            return Error("depth texture is `nullptr`");
        }

        // (Re)bind renderer's depth image (this pointer may change every frame,
        // don't check if pointer is the same (we had this and it made us skip rebinding sometimes after
        // non-MSAA depth texture was recreated), just force rebind every frame).
        auto optionalError = pComputeInterface->bindResource(
            pDepthTexture, sDepthTextureShaderResourceName, ComputeResourceUsage::READ_ONLY_TEXTURE, true);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // (Re)bind current frame resource (because the resource will change every frame).
        optionalError = pComputeInterface->bindResource(
            pCurrentFrameResource->pFrameConstantBuffer->getInternalResource(),
            "frameData",
            ComputeResourceUsage::CONSTANT_BUFFER,
            true);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // (Re)bind general light data (because the resource will change every frame).
        optionalError = pComputeInterface->bindResource(
            pGeneralLightingData,
            LightingShaderResourceManager::getGeneralLightingDataShaderResourceName(),
            ComputeResourceUsage::CONSTANT_BUFFER,
            true);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // (Re)bind point light array (because the resource will change every frame).
        optionalError = pComputeInterface->bindResource(
            pPointLightArray,
            LightingShaderResourceManager::getPointLightsShaderResourceName(),
            ComputeResourceUsage::READ_ONLY_ARRAY_BUFFER,
            true);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // (Re)bind spotlight array (because the resource will change every frame).
        optionalError = pComputeInterface->bindResource(
            pSpotlightArray,
            LightingShaderResourceManager::getSpotlightsShaderResourceName(),
            ComputeResourceUsage::READ_ONLY_ARRAY_BUFFER,
            true);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // No need to bind directional lights array because it's not used (we just accept all directional
        // lights since there does not seem to be a reliable way to cull them).

        // (Re)bind non-culled indices to point lights array (because the resource will change every frame).
        optionalError = pComputeInterface->bindResource(
            pNonCulledPointLightsIndicesArray,
            LightingShaderResourceManager::getPointLightsInCameraFrustumIndicesShaderResourceName(),
            ComputeResourceUsage::READ_ONLY_ARRAY_BUFFER,
            true);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // (Re)bind non-culled indices to spotlights array (because the resource will change every frame).
        optionalError = pComputeInterface->bindResource(
            pNonCulledSpotlightsIndicesArray,
            LightingShaderResourceManager::getSpotlightsInCameraFrustumIndicesShaderResourceName(),
            ComputeResourceUsage::READ_ONLY_ARRAY_BUFFER,
            true);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Queue shader execution (we need to dispatch 1 thread group per tile).
        pComputeInterface->submitForExecution(
            threadGroupCount.iThreadGroupCountX, threadGroupCount.iThreadGroupCountY, 1);

        return {};
    }

    std::optional<Error> LightingShaderResourceManager::recalculateLightTileFrustums(
        const std::pair<unsigned int, unsigned int>& renderTargetSize,
        const glm::mat4& inverseProjectionMatrix) {
        // Self check: make sure render target size is not zero.
        if (renderTargetSize.first == 0 || renderTargetSize.second == 0) [[unlikely]] {
            return Error(std::format(
                "render target size cannot have zero size: {}x{}",
                renderTargetSize.first,
                renderTargetSize.second));
        }

        // Make sure compute interface is created.
        if (!frustumGridComputeShaderData.bIsInitialized) {
            // Check if the renderer compiled our compute shader or not.
            if (!bEngineShadersCompiled) {
                // Waiting for engine shaders to be compiled.
                return {};
            }

            // Initialize frustum grid shader.
            auto optionalError = frustumGridComputeShaderData.initialize(pRenderer);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }

            // Initialize light culling shader.
            optionalError = lightCullingComputeShaderData.initialize(pRenderer, frustumGridComputeShaderData);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }

            // Create compute interface for shader that resets global counters for light culling.
            auto computeCreationResult = ComputeShaderInterface::createUsingGraphicsQueue(
                pRenderer,
                EngineShaderNames::ForwardPlus::getPrepareLightCullingComputeShaderName(),
                ComputeExecutionStage::AFTER_DEPTH_PREPASS,
                ComputeExecutionGroup::FIRST); // we can use the same group as frustum grid calculation shader
                                               // because that shader does not use global counters
            if (std::holds_alternative<Error>(computeCreationResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(computeCreationResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            pPrepareLightCullingComputeInterface =
                std::get<std::unique_ptr<ComputeShaderInterface>>(std::move(computeCreationResult));

            // Bind global counters.
            optionalError = pPrepareLightCullingComputeInterface->bindResource(
                lightCullingComputeShaderData.resources.pGlobalCountersIntoLightIndexList.get(),
                ComputeShaderData::LightCullingComputeShader::ComputeShader::
                    sGlobalCountersIntoLightIndexListShaderResourceName,
                ComputeResourceUsage::READ_WRITE_ARRAY_BUFFER);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        // Update frustum grid shader data.
        auto optionalError = frustumGridComputeShaderData.updateDataAndSubmitShader(
            pRenderer, renderTargetSize, inverseProjectionMatrix);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Save new tile count for light culling shader.
        lightCullingComputeShaderData.threadGroupCount.iThreadGroupCountX =
            frustumGridComputeShaderData.iLastUpdateTileCountX;
        lightCullingComputeShaderData.threadGroupCount.iThreadGroupCountY =
            frustumGridComputeShaderData.iLastUpdateTileCountY;

        // Update the thread group count for light culling because it was updated.
        lightCullingComputeShaderData.resources.pThreadGroupCount->copyDataToElement(
            0,
            &lightCullingComputeShaderData.threadGroupCount,
            sizeof(ComputeShaderData::LightCullingComputeShader::ThreadGroupCount));

        // Rebind grid of frustums resource to light culling shader because it was re-created.
        optionalError = lightCullingComputeShaderData.pComputeInterface->bindResource(
            frustumGridComputeShaderData.resources.pCalculatedFrustums.get(),
            ComputeShaderData::FrustumGridComputeShader::ComputeShader::sCalculatedFrustumsShaderResourceName,
            ComputeResourceUsage::READ_ONLY_ARRAY_BUFFER);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // (Re)create light index lists and light grid if the tile count was changed.
        auto lightGridResult = lightCullingComputeShaderData.createLightIndexListsAndLightGrid(
            pRenderer,
            frustumGridComputeShaderData.iLastUpdateTileCountX,
            frustumGridComputeShaderData.iLastUpdateTileCountY);
        if (std::holds_alternative<Error>(lightGridResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(lightGridResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto bLightGridRecreated = std::get<bool>(lightGridResult);
        if (bLightGridRecreated) {
            // Rebind newly created resources to pipelines.
            optionalError = rebindGpuDataToAllPipelines();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        return {};
    }

    void LightingShaderResourceManager::onEngineShadersCompiled() { bEngineShadersCompiled = true; }

    void LightingShaderResourceManager::setAmbientLight(const glm::vec3& ambientLight) {
        std::scoped_lock guard(mtxGpuData.first);

        mtxGpuData.second.generalData.ambientLight = glm::vec4(ambientLight, 1.0F);
    }

    LightingShaderResourceManager::~LightingShaderResourceManager() {
        // Explicitly reset array pointers here to make double sure they will not trigger callback after
        // the manager is destroyed or is being destroyed.
        lightArrays.pPointLightDataArray = nullptr;
        lightArrays.pDirectionalLightDataArray = nullptr;
        lightArrays.pSpotlightDataArray = nullptr;

#if defined(DEBUG) && defined(WIN32)
        static_assert(sizeof(LightArrays) == 24, "consider resetting new arrays here"); // NOLINT
#elif defined(DEBUG)
        static_assert(sizeof(LightArrays) == 24, "consider resetting new arrays here"); // NOLINT
#endif

        // Make sure light culling shader is destroyed first because it uses resources from compute
        // shader that calculates grid of frustums.
        pPrepareLightCullingComputeInterface = nullptr;
        lightCullingComputeShaderData.pComputeInterface = nullptr;
    }

    std::string LightingShaderResourceManager::getGeneralLightingDataShaderResourceName() {
        return sGeneralLightingDataShaderResourceName;
    }

    std::string LightingShaderResourceManager::getPointLightsShaderResourceName() {
        return sPointLightsShaderResourceName;
    }

    std::string LightingShaderResourceManager::getDirectionalLightsShaderResourceName() {
        return sDirectionalLightsShaderResourceName;
    }

    std::string LightingShaderResourceManager::getSpotlightsShaderResourceName() {
        return sSpotlightsShaderResourceName;
    }

    std::string LightingShaderResourceManager::getPointLightsInCameraFrustumIndicesShaderResourceName() {
        return sPointLightsInCameraFrustumIndicesShaderResourceName;
    }

    std::string LightingShaderResourceManager::getSpotlightsInCameraFrustumIndicesShaderResourceName() {
        return sSpotlightsInCameraFrustumIndicesShaderResourceName;
    }

    std::unique_ptr<LightingShaderResourceManager>
    LightingShaderResourceManager::create(Renderer* pRenderer) {
        return std::unique_ptr<LightingShaderResourceManager>(new LightingShaderResourceManager(pRenderer));
    }
}
