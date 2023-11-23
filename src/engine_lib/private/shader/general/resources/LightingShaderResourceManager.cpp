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

    ShaderLightArray* LightingShaderResourceManager::getPointLightDataArray() {
        return pPointLightDataArray.get();
    }

    ShaderLightArray* LightingShaderResourceManager::getDirectionalLightDataArray() {
        return pDirectionalLightDataArray.get();
    }

    ShaderLightArray* LightingShaderResourceManager::getSpotlightDataArray() {
        return pSpotlightDataArray.get();
    }

    std::optional<Error> LightingShaderResourceManager::bindDescriptorsToRecreatedPipelineResources() {
        // Notify point light array.
        auto optionalError = pPointLightDataArray->updateBindingsInAllPipelines();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Notify directional light array.
        optionalError = pDirectionalLightDataArray->updateBindingsInAllPipelines();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Notify spotlight array.
        optionalError = pSpotlightDataArray->updateBindingsInAllPipelines();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

#if defined(DEBUG) && defined(WIN32)
        static_assert(
            sizeof(LightingShaderResourceManager) == 352, "consider notifying new arrays here"); // NOLINT
#elif defined(DEBUG)
        static_assert(
            sizeof(LightingShaderResourceManager) == 144, "consider notifying new arrays here"); // NOLINT
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
        auto optionalError = pPointLightDataArray->updatePipelineBinding(pPipeline);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Notify directional light array.
        optionalError = pDirectionalLightDataArray->updatePipelineBinding(pPipeline);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Notify spotlight array.
        optionalError = pSpotlightDataArray->updatePipelineBinding(pPipeline);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

#if defined(DEBUG) && defined(WIN32)
        static_assert(
            sizeof(LightingShaderResourceManager) == 352, "consider notifying new arrays here"); // NOLINT
#elif defined(DEBUG)
        static_assert(
            sizeof(LightingShaderResourceManager) == 144, "consider notifying new arrays here"); // NOLINT
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
        pPointLightDataArray->updateSlotsMarkedAsNeedsUpdate(iCurrentFrameResourceIndex);
        pDirectionalLightDataArray->updateSlotsMarkedAsNeedsUpdate(iCurrentFrameResourceIndex);
        pSpotlightDataArray->updateSlotsMarkedAsNeedsUpdate(iCurrentFrameResourceIndex);

#if defined(DEBUG) && defined(WIN32)
        static_assert(
            sizeof(LightingShaderResourceManager) == 352, "consider notifying new arrays here"); // NOLINT
#elif defined(DEBUG)
        static_assert(
            sizeof(LightingShaderResourceManager) == 144, "consider notifying new arrays here"); // NOLINT
#endif

        // Copy general lighting info (maybe changed, since that data is very small it should be OK to
        // update it every frame).
        copyDataToGpu(iCurrentFrameResourceIndex);

        // Get point light arrays (we don't really need to lock mutexes here since we are inside
        // `drawNextFrame`).
        const auto pPointLightArrayResource = pPointLightDataArray->getInternalResources()
                                                  ->second.vGpuResources[iCurrentFrameResourceIndex]
                                                  ->getInternalResource();
        const auto pSpotlightArrayResource = pSpotlightDataArray->getInternalResources()
                                                 ->second.vGpuResources[iCurrentFrameResourceIndex]
                                                 ->getInternalResource();
        const auto pDirectionalLightArrayResource = pDirectionalLightDataArray->getInternalResources()
                                                        ->second.vGpuResources[iCurrentFrameResourceIndex]
                                                        ->getInternalResource();

        // Queue light culling shader (should be called every frame).
        std::scoped_lock dataGuard(mtxGpuData.first);
        auto optionalError = lightCullingComputeShaderData.queueExecutionForNextFrame(
            pRenderer,
            pCurrentFrameResource,
            iCurrentFrameResourceIndex,
            mtxGpuData.second.vGeneralDataGpuResources[iCurrentFrameResourceIndex]->getInternalResource(),
            pPointLightArrayResource,
            pSpotlightArrayResource,
            pDirectionalLightArrayResource);
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
        mtxGpuData.second.generalData.iPointLightCount = static_cast<unsigned int>(iNewSize);

        // Copy updated data to the GPU resources.
        for (size_t i = 0; i < mtxGpuData.second.vGeneralDataGpuResources.size(); i++) {
            copyDataToGpu(i);
        }
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
        mtxGpuData.second.generalData.iSpotlightCount = static_cast<unsigned int>(iNewSize);

        // Copy updated data to the GPU resources.
        for (size_t i = 0; i < mtxGpuData.second.vGeneralDataGpuResources.size(); i++) {
            copyDataToGpu(i);
        }
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
            // Under DirectX we will bind CBV to a specific root signature index inside of the `draw`
            // function.
            return {};
        }

        // Lock resources.
        std::scoped_lock guard(mtxGpuData.first);

        // Self check: make sure GPU resources are valid.
        for (const auto& pUploadBuffer : mtxGpuData.second.vGeneralDataGpuResources) {
            if (pUploadBuffer == nullptr) [[unlikely]] {
                return Error("lighting shader resource manager has not created its GPU resources yet");
            }
        }

        // Get internal GPU resources.
        std::array<VkBuffer, FrameResourcesManager::getFrameResourcesCount()> vInternalBuffers;
        for (size_t i = 0; i < vInternalBuffers.size(); i++) {
            // Convert to Vulkan resource.
            const auto pVulkanResource = dynamic_cast<VulkanResource*>(
                mtxGpuData.second.vGeneralDataGpuResources[i]->getInternalResource());
            if (pVulkanResource == nullptr) [[unlikely]] {
                return Error("expected a Vulkan resource");
            }

            // Save buffer resource.
            vInternalBuffers[i] = pVulkanResource->getInternalBufferResource();
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

        // Get graphics pipelines.
        const auto pMtxGraphicsPipelines = pPipelineManager->getGraphicsPipelines();
        std::scoped_lock pipelinesGuard(pMtxGraphicsPipelines->first);

        // Iterate over graphics pipelines of all types.
        for (auto& pipelinesOfSpecificType : pMtxGraphicsPipelines->second.vPipelineTypes) {

            // Iterate over all active shader combinations.
            for (const auto& [sShaderNames, pipelines] : pipelinesOfSpecificType) {

                // Iterate over all active unique material macros combinations (for example:
                // if we have 2 materials where one uses diffuse texture (defined DIFFUSE_TEXTURE
                // macro for shaders) and the second one is not we will have 2 pipelines here).
                for (const auto& [materialMacros, pPipeline] : pipelines.shaderPipelines) {

                    // Convert to a Vulkan pipeline.
                    const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pPipeline.get());
                    if (pVulkanPipeline == nullptr) [[unlikely]] {
                        return Error("expected a Vulkan pipeline");
                    }

                    // Get pipeline's internal resources.
                    const auto pMtxPipelineInternalResources = pVulkanPipeline->getInternalResources();
                    std::scoped_lock pipelineResourcesGuard(pMtxPipelineInternalResources->first);

                    // See if this pipeline uses the resource we are handling.
                    auto it = pMtxPipelineInternalResources->second.resourceBindings.find(
                        sGeneralLightingDataShaderResourceName);
                    if (it == pMtxPipelineInternalResources->second.resourceBindings.end()) {
                        continue;
                    }

                    // Update one descriptor in set per frame resource.
                    for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
                        // Prepare info to bind storage buffer slot to descriptor.
                        VkDescriptorBufferInfo bufferInfo{};
                        bufferInfo.buffer = vInternalBuffers[i];
                        bufferInfo.offset = 0;
                        bufferInfo.range = sizeof(GeneralLightingShaderData);

                        // Bind reserved space to descriptor.
                        VkWriteDescriptorSet descriptorUpdateInfo{};
                        descriptorUpdateInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        descriptorUpdateInfo.dstSet = pMtxPipelineInternalResources->second
                                                          .vDescriptorSets[i]; // descriptor set to update
                        descriptorUpdateInfo.dstBinding = it->second;          // descriptor binding index
                        descriptorUpdateInfo.dstArrayElement = 0; // first descriptor in array to update
                        descriptorUpdateInfo.descriptorType = generalLightingDataDescriptorType;
                        descriptorUpdateInfo.descriptorCount = 1; // how much descriptors in array to update
                        descriptorUpdateInfo.pBufferInfo = &bufferInfo; // descriptor refers to buffer data

                        // Update descriptor.
                        vkUpdateDescriptorSets(pLogicalDevice, 1, &descriptorUpdateInfo, 0, nullptr);
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

        // Lock resources.
        std::scoped_lock guard(mtxGpuData.first);

        // Self check: make sure GPU resources are valid.
        for (const auto& pUploadBuffer : mtxGpuData.second.vGeneralDataGpuResources) {
            if (pUploadBuffer == nullptr) [[unlikely]] {
                return Error("lighting shader resource manager has not created its GPU resources yet");
            }
        }

        // Get internal GPU resources.
        std::array<VkBuffer, FrameResourcesManager::getFrameResourcesCount()> vInternalBuffers;
        for (size_t i = 0; i < vInternalBuffers.size(); i++) {
            // Convert to Vulkan resource.
            const auto pVulkanResource = dynamic_cast<VulkanResource*>(
                mtxGpuData.second.vGeneralDataGpuResources[i]->getInternalResource());
            if (pVulkanResource == nullptr) [[unlikely]] {
                return Error("expected a Vulkan resource");
            }

            // Save buffer resource.
            vInternalBuffers[i] = pVulkanResource->getInternalBufferResource();
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

        // Get pipeline's internal resources.
        const auto pMtxPipelineInternalResources = pVulkanPipeline->getInternalResources();
        std::scoped_lock pipelineResourcesGuard(pMtxPipelineInternalResources->first);

        // See if this pipeline uses the resource we are handling.
        auto it = pMtxPipelineInternalResources->second.resourceBindings.find(
            sGeneralLightingDataShaderResourceName);
        if (it == pMtxPipelineInternalResources->second.resourceBindings.end()) {
            return {};
        }

        // Update one descriptor in set per frame resource.
        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
            // Prepare info to bind storage buffer slot to descriptor.
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = vInternalBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(GeneralLightingShaderData);

            // Bind reserved space to descriptor.
            VkWriteDescriptorSet descriptorUpdateInfo{};
            descriptorUpdateInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorUpdateInfo.dstSet =
                pMtxPipelineInternalResources->second.vDescriptorSets[i]; // descriptor set to update
            descriptorUpdateInfo.dstBinding = it->second;                 // descriptor binding index
            descriptorUpdateInfo.dstArrayElement = 0; // first descriptor in array to update
            descriptorUpdateInfo.descriptorType = generalLightingDataDescriptorType;
            descriptorUpdateInfo.descriptorCount = 1;       // how much descriptors in array to update
            descriptorUpdateInfo.pBufferInfo = &bufferInfo; // descriptor refers to buffer data

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
        pPointLightDataArray =
            ShaderLightArray::create(pRenderer, sPointLightsShaderResourceName, [this](size_t iNewSize) {
                onPointLightArraySizeChanged(iNewSize);
            });

        // Create directional light array.
        pDirectionalLightDataArray = ShaderLightArray::create(
            pRenderer, sDirectionalLightsShaderResourceName, [this](size_t iNewSize) {
                onDirectionalLightArraySizeChanged(iNewSize);
            });

        // Create spotlight array.
        pSpotlightDataArray =
            ShaderLightArray::create(pRenderer, sSpotlightsShaderResourceName, [this](size_t iNewSize) {
                onSpotlightArraySizeChanged(iNewSize);
            });

#if defined(DEBUG) && defined(WIN32)
        static_assert(
            sizeof(LightingShaderResourceManager) == 352, "consider creating new arrays here"); // NOLINT
#elif defined(DEBUG)
        static_assert(
            sizeof(LightingShaderResourceManager) == 144, "consider creating new arrays here"); // NOLINT
#endif
    }

    [[nodiscard]] std::optional<Error>
    LightingShaderResourceManager::ComputeShaderData::FrustumGridComputeShader::ComputeShader::updateData(
        Renderer* pRenderer,
        const std::pair<unsigned int, unsigned int>& renderResolution,
        const glm::mat4& inverseProjectionMatrix,
        bool bQueueShaderExecution) {
        // Make sure engine shaders were compiled and we created compute interface.
        if (pComputeInterface == nullptr) [[unlikely]] {
            return Error("expected compute interface to be created at this point");
        }

        // Make sure the GPU is not using resources that we will update.
        std::scoped_lock renderGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Get tile size (this value also describes threads in one thread group).
        size_t iTileSizeInPixels = 0;
        try {
            iTileSizeInPixels = std::stoull(
                EngineShaderConstantMacros::ForwardPlus::FrustumGridThreadsInGroupXyMacro::sValue);
        } catch (const std::exception& exception) {
            return Error(std::format(
                "failed to convert frustum grid tile size to an integer, error: {}", exception.what()));
        };

        // Calculate tile count (using INT/INT to "floor" if not divisible equally).
        const auto iTileCountX = static_cast<unsigned int>(renderResolution.first / iTileSizeInPixels);
        const auto iTileCountY = static_cast<unsigned int>(renderResolution.second / iTileSizeInPixels);

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
        computeInfo.maxDepth = Renderer::getMaxDepth();
        resources.pComputeInfo->copyDataToElement(0, &computeInfo, sizeof(computeInfo));

        // Update screen to view resource.
        ScreenToViewData screenToViewData;
        screenToViewData.iRenderResolutionWidth = renderResolution.first;
        screenToViewData.iRenderResolutionHeight = renderResolution.second;
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

        if (bQueueShaderExecution) {
            // Queue frustum grid recalculation shader.
            pComputeInterface->submitForExecution(iThreadGroupCountX, iThreadGroupCountY, 1);
        }

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
            EngineShaderNames::ForwardPlus::sCalculateFrustumGridComputeShaderName,
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
            EngineShaderNames::ForwardPlus::sLightCullingComputeShaderName,
            ComputeExecutionStage::AFTER_DEPTH_PREPASS,
            ComputeExecutionGroup::SECOND); // runs after compute shader that calculates grid frustums
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

        // Create a resource to store global counters.
        for (auto& pCountersResource : resources.vGlobalCountersIntoLightIndexList) {
            // Create resource.
            result = pRenderer->getResourceManager()->createResourceWithCpuWriteAccess(
                "light culling - global counters",
                sizeof(ComputeShaderData::LightCullingComputeShader::GlobalCountersIntoLightIndexList),
                1,
                true);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            pCountersResource = std::get<std::unique_ptr<UploadBuffer>>(std::move(result));

            // Set initial (zero) value.
            GlobalCountersIntoLightIndexList counters{};
            pCountersResource->copyDataToElement(0, &counters, sizeof(GlobalCountersIntoLightIndexList));
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

    std::optional<Error>
    LightingShaderResourceManager::ComputeShaderData::LightCullingComputeShader::ComputeShader::
        createLightIndexListsAndLightGrid(Renderer* pRenderer, size_t iTileCountX, size_t iTileCountY) {
        // Make sure tile count was changed.
        if (iTileCountX == resources.iLightGridTileCountX && iTileCountY == resources.iLightGridTileCountY) {
            // Tile count was not changed - nothing to do.
            return {};
        }

        // First convert all constants (shader macros) from strings to integers.
        size_t iAveragePointLightNumPerTile = 0;
        size_t iAverageSpotLightNumPerTile = 0;
        size_t iAverageDirectionalLightNumPerTile = 0;
        try {
            // Point lights.
            iAveragePointLightNumPerTile = std::stoull(
                EngineShaderConstantMacros::ForwardPlus::AveragePointLightNumPerTileMacro::sValue);

            // Spotlights.
            iAverageSpotLightNumPerTile =
                std::stoull(EngineShaderConstantMacros::ForwardPlus::AverageSpotLightNumPerTileMacro::sValue);

            // Directional lights.
            iAverageDirectionalLightNumPerTile = std::stoull(
                EngineShaderConstantMacros::ForwardPlus::AverageDirectionalLightNumPerTileMacro::sValue);
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
                return optionalError;
            }
        }

        // Save new tile count.
        resources.iLightGridTileCountX = iTileCountX;
        resources.iLightGridTileCountY = iTileCountY;

        return {};
    }

    std::optional<Error> LightingShaderResourceManager::ComputeShaderData::LightCullingComputeShader::
        ComputeShader::queueExecutionForNextFrame(
            Renderer* pRenderer,
            FrameResource* pCurrentFrameResource,
            size_t iCurrentFrameResourceIndex,
            GpuResource* pGeneralLightingData,
            GpuResource* pPointLightArray,
            GpuResource* pSpotlightArray,
            GpuResource* pDirectionalLightArray) {
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

        // Get renderer's depth texture pointer (this pointer can change every frame).
        const auto pDepthTexture = pRenderer->getDepthTextureNoMultisampling();

        // Check if it is different from the one we binded the last time.
        if (resources.pLastBindedDepthTexture != pDepthTexture) {
            // Save new pointer.
            resources.pLastBindedDepthTexture = pDepthTexture;

            // (Re)bind renderer's depth image.
            auto optionalError = pComputeInterface->bindResource(
                resources.pLastBindedDepthTexture,
                sDepthTextureShaderResourceName,
                ComputeResourceUsage::READ_ONLY_TEXTURE);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        // (Re)bind current frame resource (because the resource will change every frame).
        auto optionalError = pComputeInterface->bindResource(
            pCurrentFrameResource->pFrameConstantBuffer->getInternalResource(),
            "frameData",
            ComputeResourceUsage::CONSTANT_BUFFER);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // (Re)bind general light data (because the resource will change every frame).
        optionalError = pComputeInterface->bindResource(
            pGeneralLightingData,
            LightingShaderResourceManager::getGeneralLightingDataShaderResourceName(),
            ComputeResourceUsage::CONSTANT_BUFFER);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // (Re)bind point light array (because the resource will change every frame).
        optionalError = pComputeInterface->bindResource(
            pPointLightArray,
            LightingShaderResourceManager::getPointLightsShaderResourceName(),
            ComputeResourceUsage::READ_ONLY_ARRAY_BUFFER);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // (Re)bind spotlight array (because the resource will change every frame).
        optionalError = pComputeInterface->bindResource(
            pSpotlightArray,
            LightingShaderResourceManager::getSpotlightsShaderResourceName(),
            ComputeResourceUsage::READ_ONLY_ARRAY_BUFFER);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // (Re)bind directional light array (because the resource will change every frame).
        optionalError = pComputeInterface->bindResource(
            pDirectionalLightArray,
            LightingShaderResourceManager::getDirectionalLightsShaderResourceName(),
            ComputeResourceUsage::READ_ONLY_ARRAY_BUFFER);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Reset global counters to zero.
        GlobalCountersIntoLightIndexList counters{};
        resources.vGlobalCountersIntoLightIndexList[iCurrentFrameResourceIndex]->copyDataToElement(
            0, &counters, sizeof(GlobalCountersIntoLightIndexList));

        // Bind global counters of the current frame resource.
        optionalError = pComputeInterface->bindResource(
            resources.vGlobalCountersIntoLightIndexList[iCurrentFrameResourceIndex]->getInternalResource(),
            sGlobalCountersIntoLightIndexListShaderResourceName,
            ComputeResourceUsage::READ_WRITE_ARRAY_BUFFER);
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
        const std::pair<unsigned int, unsigned int>& renderResolution,
        const glm::mat4& inverseProjectionMatrix) {
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
        }

        // Update frustum grid shader data.
        auto optionalError = frustumGridComputeShaderData.updateData(
            pRenderer, renderResolution, inverseProjectionMatrix, true);
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
        optionalError = lightCullingComputeShaderData.createLightIndexListsAndLightGrid(
            pRenderer,
            frustumGridComputeShaderData.iLastUpdateTileCountX,
            frustumGridComputeShaderData.iLastUpdateTileCountY);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
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
        pPointLightDataArray = nullptr;
        pDirectionalLightDataArray = nullptr;
        pSpotlightDataArray = nullptr;

#if defined(DEBUG) && defined(WIN32)
        static_assert(
            sizeof(LightingShaderResourceManager) == 352, "consider resetting new arrays here"); // NOLINT
#elif defined(DEBUG)
        static_assert(
            sizeof(LightingShaderResourceManager) == 144, "consider resetting new arrays here"); // NOLINT
#endif

        // Make sure light culling shader is destroyed first because it uses resources from compute
        // shader that calculates grid of frustums.
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

    std::unique_ptr<LightingShaderResourceManager>
    LightingShaderResourceManager::create(Renderer* pRenderer) {
        return std::unique_ptr<LightingShaderResourceManager>(new LightingShaderResourceManager(pRenderer));
    }
}
