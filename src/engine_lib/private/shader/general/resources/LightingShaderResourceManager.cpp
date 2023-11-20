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

namespace ne {

    ShaderLightArraySlot::ShaderLightArraySlot(
        ShaderLightArray* pArray,
        size_t iIndexIntoArray,
        const std::function<void*()>& startUpdateCallback,
        const std::function<void()>& finishUpdateCallback)
        : startUpdateCallback(startUpdateCallback), finishUpdateCallback(finishUpdateCallback) {
        this->pArray = pArray;
        this->iIndexIntoArray = iIndexIntoArray;
    }

    ShaderLightArraySlot::~ShaderLightArraySlot() { pArray->freeSlot(this); }

    void ShaderLightArraySlot::markAsNeedsUpdate() { pArray->markSlotAsNeedsUpdate(this); }

    ShaderLightArray::ShaderLightArray(
        Renderer* pRenderer,
        const std::string& sShaderLightResourceName,
        const std::function<void(size_t)>& onSizeChanged)
        : onSizeChanged(onSizeChanged), sShaderLightResourceName(sShaderLightResourceName) {
        this->pRenderer = pRenderer;

        // Initialize resources.
        auto optionalError = recreateArray(true);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }
    }

    ShaderLightArray::~ShaderLightArray() {
        std::scoped_lock guard(mtxResources.first);

        // Make sure there are no active slots.
        if (!mtxResources.second.activeSlots.empty()) [[unlikely]] {
            Error error(std::format(
                "shader light array \"{}\" is being destroyed but there are still {} active slot(s)",
                sShaderLightResourceName,
                mtxResources.second.activeSlots.size()));
            error.showError();
            return; // don't throw in destructor
        }

        // Make sure there are no "to be updated" slots.
        for (const auto& slots : mtxResources.second.vSlotsToBeUpdated) {
            if (!slots.empty()) [[unlikely]] {
                Error error(std::format(
                    "shader light array \"{}\" is being destroyed but there are still {} slot(s) marked as "
                    "\"to be updated\"",
                    sShaderLightResourceName,
                    mtxResources.second.activeSlots.size()));
                error.showError();
                return; // don't throw in destructor
            }
        }

        // Make sure that resources still exist.
        for (auto& pUploadBuffer : mtxResources.second.vGpuResources) {
            if (pUploadBuffer == nullptr) [[unlikely]] {
                Error error(std::format(
                    "shader light array \"{}\" is being destroyed but its GPU resources are already "
                    "destroyed (expected resources to be valid to destroy them here)",
                    sShaderLightResourceName,
                    mtxResources.second.activeSlots.size()));
                error.showError();
                return; // don't throw in destructor
            }
        }
    }

    std::unique_ptr<ShaderLightArray> ShaderLightArray::create(
        Renderer* pRenderer,
        const std::string& sShaderLightResourceName,
        const std::function<void(size_t)>& onSizeChanged) {
        return std::unique_ptr<ShaderLightArray>(
            new ShaderLightArray(pRenderer, sShaderLightResourceName, onSizeChanged));
    }

    std::variant<std::unique_ptr<ShaderLightArraySlot>, Error> ShaderLightArray::reserveNewSlot(
        size_t iDataSizeInBytes,
        const std::function<void*()>& startUpdateCallback,
        const std::function<void()>& finishUpdateCallback) {
        // Pause the rendering and make sure our resources are not used by the GPU
        // (locking both mutexes to avoid a deadlock that might occur below).
        std::scoped_lock drawingGuard(*pRenderer->getRenderResourcesMutex(), mtxResources.first);
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        if (mtxResources.second.activeSlots.empty()) {
            // Save element size.
            iElementSizeInBytes = iDataSizeInBytes;
        }
        // Self check: make sure the specified size equals to the previously specified one.
        else if (iDataSizeInBytes != iElementSizeInBytes) [[unlikely]] {
            return Error(std::format(
                "shader light array \"{}\" was requested to reserve a new slot but the specified "
                "data size {} differs from the data size that currently existing slots use: {}",
                sShaderLightResourceName,
                iDataSizeInBytes,
                iElementSizeInBytes));
        }

        // Create a new slot.
        auto pNewSlot = std::unique_ptr<ShaderLightArraySlot>(new ShaderLightArraySlot(
            this, mtxResources.second.activeSlots.size(), startUpdateCallback, finishUpdateCallback));

        // Add new slot to the array of active slots.
        mtxResources.second.activeSlots.insert(pNewSlot.get());

        // Expand array to include the new slot
        // (new slot's data will be copied inside of this function).
        auto optionalError = recreateArray();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }

        // Notify.
        onSizeChanged(mtxResources.second.activeSlots.size());

        return pNewSlot;
    }

    std::pair<std::recursive_mutex, ShaderLightArray::Resources>* ShaderLightArray::getInternalResources() {
        return &mtxResources;
    }

    void ShaderLightArray::freeSlot(ShaderLightArraySlot* pSlot) {
        // Pause the rendering and make sure our resources are not used by the GPU
        // (locking both mutexes to avoid a deadlock that might occur below).
        std::scoped_lock drawingGuard(*pRenderer->getRenderResourcesMutex(), mtxResources.first);
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Make sure this slot is indeed active.
        const auto slotIt = mtxResources.second.activeSlots.find(pSlot);
        if (slotIt == mtxResources.second.activeSlots.end()) [[unlikely]] {
            Error error(std::format(
                "a slot notified the shader light array \"{}\" that it's being destroyed "
                "but this array can't find this slot in its array of active slots",
                sShaderLightResourceName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Remove this slot.
        mtxResources.second.activeSlots.erase(slotIt);

        // Remove this slot from "to be updated" array (if it exists there).
        for (auto& slots : mtxResources.second.vSlotsToBeUpdated) {
            // Find in array.
            const auto it = slots.find(pSlot);
            if (it == slots.end()) {
                continue;
            }

            // Remove from array.
            slots.erase(it);
        }

        if (mtxResources.second.activeSlots.empty()) {
            // Self check: make sure "to be updated" array is empty.
            for (const auto& slots : mtxResources.second.vSlotsToBeUpdated) {
                if (!slots.empty()) [[unlikely]] {
                    Error error(std::format(
                        "shader light array \"{}\" now has no slots but its \"slots to update\" array still "
                        "has {} slot(s)",
                        sShaderLightResourceName,
                        slots.size()));
                    error.showError();
                    throw std::runtime_error(error.getFullErrorMessage());
                }
            }

            // Don't destroy GPU resources, we need to have a valid resource to avoid hitting
            // `nullptr` or use branching when binding resources, resources will not be used
            // since counter for active light sources will be zero.
        } else {
            // Shrink array.
            auto optionalError = recreateArray();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                optionalError->showError();
                throw std::runtime_error(optionalError->getFullErrorMessage());
            }
        }

        // Notify.
        onSizeChanged(mtxResources.second.activeSlots.size());
    }

    void ShaderLightArray::markSlotAsNeedsUpdate(ShaderLightArraySlot* pSlot) {
        std::scoped_lock guard(mtxResources.first);

        // Self check: make sure this slot exists in the array of active slots.
        const auto slotIt = mtxResources.second.activeSlots.find(pSlot);
        if (slotIt == mtxResources.second.activeSlots.end()) [[unlikely]] {
            Logger::get().error(std::format(
                "a slot notified the shader light array \"{}\" that it needs an update but this slot does "
                "not exist in the array of active slots",
                sShaderLightResourceName));
            return;
        }

        // Add to be updated for each frame resource,
        // even if it's already marked as "needs update" `std::set` guarantees element uniqueness
        // so there's no need to check if the resource already marked as "needs update" or not.
        for (auto& set : mtxResources.second.vSlotsToBeUpdated) {
            set.insert(pSlot);
        }
    }

    std::optional<Error> ShaderLightArray::recreateArray(bool bIsInitialization) {
        // Pause the rendering and make sure our resources are not used by the GPU
        // (locking both mutexes to avoid a deadlock).
        std::scoped_lock drawingGuard(*pRenderer->getRenderResourcesMutex(), mtxResources.first);
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Get resource manager.
        const auto pResourceManager = pRenderer->getResourceManager();

        // Prepare array size.
        const size_t iArraySize = bIsInitialization ? 1 : mtxResources.second.activeSlots.size();
        const size_t iArrayElementSize = bIsInitialization ? 4 : iElementSizeInBytes; // NOLINT: dummy size

        // Self check: make sure new array size is not zero.
        if (iArraySize == 0) [[unlikely]] {
            return Error(std::format(
                "shader light array \"{}\" was requested to be created to change "
                "its size but the new size is zero",
                sShaderLightResourceName));
        }

        // Re-create the resource.
        for (size_t i = 0; i < mtxResources.second.vGpuResources.size(); i++) {
            // Create a new resource with the specified size.
            auto result = pResourceManager->createResourceWithCpuWriteAccess(
                std::format("{} frame #{}", sShaderLightResourceName, i),
                iArrayElementSize,
                iArraySize,
                true);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            mtxResources.second.vGpuResources[i] = std::get<std::unique_ptr<UploadBuffer>>(std::move(result));
        }

#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            // Bind SRV to the created resource.
            for (auto& pUploadBuffer : mtxResources.second.vGpuResources) {
                // Convert to DirectX resource.
                const auto pDirectXResource =
                    dynamic_cast<DirectXResource*>(pUploadBuffer->getInternalResource());
                if (pDirectXResource == nullptr) [[unlikely]] {
                    return Error("expected a DirectX resource");
                }

                // Bind SRV.
                auto optionalError = pDirectXResource->bindDescriptor(DirectXDescriptorType::SRV);
                if (optionalError.has_value()) [[unlikely]] {
                    optionalError->addCurrentLocationToErrorStack();
                    return optionalError;
                }
            }
        }
#endif

        // Clear array of slots to update since they hold indices to old (deleted) array and we will anyway
        // re-copy slot data now.
        for (auto& slots : mtxResources.second.vSlotsToBeUpdated) {
            slots.clear();
        }

        // Copy slots' data into the new GPU resources.
        size_t iCurrentSlotIndex = 0;
        for (auto& pSlot : mtxResources.second.activeSlots) {
            // Update slot's index.
            pSlot->iIndexIntoArray = iCurrentSlotIndex;

            // Get pointer to the data.
            const auto pData = pSlot->startUpdateCallback();

            // Copy slot data into the new GPU resource.
            for (const auto& pUploadBuffer : mtxResources.second.vGpuResources) {
                pUploadBuffer->copyDataToElement(iCurrentSlotIndex, pData, iElementSizeInBytes);
            }

            // Mark updating finished.
            pSlot->finishUpdateCallback();

            // Increment next slot index.
            iCurrentSlotIndex += 1;
        }

        // (Re)bind the (re)created resource to descriptors of all pipelines that use this resource.
        auto optionalError = updateBindingsInAllPipelines();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    void ShaderLightArray::updateSlotsMarkedAsNeedsUpdate(size_t iCurrentFrameResourceIndex) {
        std::scoped_lock shaderRwResourceGuard(mtxResources.first);

        if (mtxResources.second.vSlotsToBeUpdated[iCurrentFrameResourceIndex].empty()) {
            // Nothing to update.
            return;
        }

        // Copy new resource data to the GPU resources of the current frame resource.
        const auto pSlotsToUpdate = &mtxResources.second.vSlotsToBeUpdated[iCurrentFrameResourceIndex];
        for (const auto& pSlot : *pSlotsToUpdate) {
            // Get pointer to the data.
            const auto pData = pSlot->startUpdateCallback();

            // Copy slot data into the GPU resource of the current frame.
            const auto pUploadBuffer = mtxResources.second.vGpuResources[iCurrentFrameResourceIndex].get();
            pUploadBuffer->copyDataToElement(pSlot->iIndexIntoArray, pData, iElementSizeInBytes);

            // Mark updating finished.
            pSlot->finishUpdateCallback();
        }

        // Clear array of resources to be updated for the current frame resource since
        // we updated all resources for the current frame resource.
        mtxResources.second.vSlotsToBeUpdated[iCurrentFrameResourceIndex].clear();
    }

    std::optional<Error> ShaderLightArray::updateBindingsInAllPipelines() {
        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pRenderer);
        if (pVulkanRenderer == nullptr) {
            // Under DirectX we will bind SRV to a specific root signature index inside of the `draw`
            // function.
            return {};
        }

        // Lock resources.
        std::scoped_lock guard(mtxResources.first);

        // Don't check if slots are empty because we need to provide a valid binding anyway
        // and even if there are no active slots a resource is guaranteed to exist (see field docs).

        // Self check: make sure GPU resources are valid.
        for (const auto& pUploadBuffer : mtxResources.second.vGpuResources) {
            if (pUploadBuffer == nullptr) [[unlikely]] {
                return Error(std::format(
                    "shader light array \"{}\" has {} active slot(s) but array's GPU resources are not "
                    "created",
                    sShaderLightResourceName,
                    mtxResources.second.activeSlots.size()));
            }
        }

        // Get internal GPU resources.
        std::array<VkBuffer, FrameResourcesManager::getFrameResourcesCount()> vInternalBuffers;
        for (size_t i = 0; i < vInternalBuffers.size(); i++) {
            // Convert to Vulkan resource.
            const auto pVulkanResource =
                dynamic_cast<VulkanResource*>(mtxResources.second.vGpuResources[i]->getInternalResource());
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
                    auto it =
                        pMtxPipelineInternalResources->second.resourceBindings.find(sShaderLightResourceName);
                    if (it == pMtxPipelineInternalResources->second.resourceBindings.end()) {
                        continue;
                    }

                    // Update one descriptor in set per frame resource.
                    for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
                        // Prepare info to bind storage buffer slot to descriptor.
                        VkDescriptorBufferInfo bufferInfo{};
                        bufferInfo.buffer = vInternalBuffers[i];
                        bufferInfo.offset = 0;
                        bufferInfo.range = mtxResources.second.vGpuResources[i]->getElementCount() *
                                           mtxResources.second.vGpuResources[i]->getElementSizeInBytes();

                        // Bind reserved space to descriptor.
                        VkWriteDescriptorSet descriptorUpdateInfo{};
                        descriptorUpdateInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        descriptorUpdateInfo.dstSet = pMtxPipelineInternalResources->second
                                                          .vDescriptorSets[i]; // descriptor set to update
                        descriptorUpdateInfo.dstBinding = it->second;          // descriptor binding index
                        descriptorUpdateInfo.dstArrayElement = 0; // first descriptor in array to update
                        descriptorUpdateInfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
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

    std::optional<Error> ShaderLightArray::updatePipelineBinding(Pipeline* pPipeline) {
        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pRenderer);
        if (pVulkanRenderer == nullptr) {
            // Under DirectX we will bind SRV to a specific root signature index inside of the `draw`
            // function.
            return {};
        }

        // Lock resources.
        std::scoped_lock guard(mtxResources.first);

        // Don't check if slots are empty because we need to provide a valid binding anyway
        // and even if there are no active slots a resource is guaranteed to exist (see field docs).

        // Self check: make sure GPU resources are valid.
        for (const auto& pUploadBuffer : mtxResources.second.vGpuResources) {
            if (pUploadBuffer == nullptr) [[unlikely]] {
                return Error(std::format(
                    "shader light array \"{}\" has {} active slot(s) but array's GPU resources are not "
                    "created",
                    sShaderLightResourceName,
                    mtxResources.second.activeSlots.size()));
            }
        }

        // Get internal GPU resources.
        std::array<VkBuffer, FrameResourcesManager::getFrameResourcesCount()> vInternalBuffers;
        for (size_t i = 0; i < vInternalBuffers.size(); i++) {
            // Convert to Vulkan resource.
            const auto pVulkanResource =
                dynamic_cast<VulkanResource*>(mtxResources.second.vGpuResources[i]->getInternalResource());
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
        auto it = pMtxPipelineInternalResources->second.resourceBindings.find(sShaderLightResourceName);
        if (it == pMtxPipelineInternalResources->second.resourceBindings.end()) {
            return {};
        }

        // Update one descriptor in set per frame resource.
        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
            // Prepare info to bind storage buffer slot to descriptor.
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = vInternalBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = mtxResources.second.vGpuResources[i]->getElementCount() *
                               mtxResources.second.vGpuResources[i]->getElementSizeInBytes();

            // Bind reserved space to descriptor.
            VkWriteDescriptorSet descriptorUpdateInfo{};
            descriptorUpdateInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorUpdateInfo.dstSet =
                pMtxPipelineInternalResources->second.vDescriptorSets[i]; // descriptor set to update
            descriptorUpdateInfo.dstBinding = it->second;                 // descriptor binding index
            descriptorUpdateInfo.dstArrayElement = 0; // first descriptor in array to update
            descriptorUpdateInfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorUpdateInfo.descriptorCount = 1;       // how much descriptors in array to update
            descriptorUpdateInfo.pBufferInfo = &bufferInfo; // descriptor refers to buffer data

            // Update descriptor.
            vkUpdateDescriptorSets(pLogicalDevice, 1, &descriptorUpdateInfo, 0, nullptr);
        }

        return {};
    }

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
            sizeof(LightingShaderResourceManager) == 240, "consider notifying new arrays here"); // NOLINT
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
            sizeof(LightingShaderResourceManager) == 240, "consider notifying new arrays here"); // NOLINT
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

    void LightingShaderResourceManager::updateResources(size_t iCurrentFrameResourceIndex) {
        // Notify light arrays.
        pPointLightDataArray->updateSlotsMarkedAsNeedsUpdate(iCurrentFrameResourceIndex);
        pDirectionalLightDataArray->updateSlotsMarkedAsNeedsUpdate(iCurrentFrameResourceIndex);
        pSpotlightDataArray->updateSlotsMarkedAsNeedsUpdate(iCurrentFrameResourceIndex);

#if defined(DEBUG) && defined(WIN32)
        static_assert(
            sizeof(LightingShaderResourceManager) == 240, "consider notifying new arrays here"); // NOLINT
#elif defined(DEBUG)
        static_assert(
            sizeof(LightingShaderResourceManager) == 144, "consider notifying new arrays here"); // NOLINT
#endif

        // Copy general lighting info (maybe changed, since that data is very small it should be OK to
        // update it every frame).
        copyDataToGpu(iCurrentFrameResourceIndex);

        // Queue light culling shader (should be called every frame).
        auto optionalError =
            lightCullingComputeShaderData.queueExecutionForNextFrame(pRenderer, frustumGridComputeShaderData);
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
            sizeof(LightingShaderResourceManager) == 240, "consider creating new arrays here"); // NOLINT
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

        // Get tile size.
        size_t iTileSizeInPixels = 0;
        try {
            iTileSizeInPixels = std::stoull(
                EngineShaderConstantMacros::ForwardPlus::FrustumGridThreadsInGroupXyMacro::sValue);
        } catch (const std::exception& exception) {
            return Error(std::format(
                "failed to convert frustum grid tile size to an integer, error: {}", exception.what()));
        };

        // Calculate tile count.
        const auto iTileCountX = static_cast<unsigned int>(renderResolution.first / iTileSizeInPixels);
        const auto iTileCountY = static_cast<unsigned int>(renderResolution.second / iTileSizeInPixels);

        // Calculate frustum count.
        const size_t iFrustumCount = iTileCountX * iTileCountY;

        // Calculate thread group count.
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
        Renderer* pRenderer) {
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

        // Done.
        bIsInitialized = true;

        return {};
    }

    std::optional<Error> LightingShaderResourceManager::ComputeShaderData::LightCullingComputeShader::
        ComputeShader::queueExecutionForNextFrame(
            Renderer* pRenderer, const FrustumGridComputeShader::ComputeShader& frustumGridShader) {
        // Make sure frustum grid shader was initialized.
        if (!frustumGridShader.bIsInitialized) [[unlikely]] {
            return Error("expected frustum grid shader to be initialized");
        }

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

        // Resource that stores calculated grid of frustums is binded inside of the update function
        // for shader that calculates that grid.

        // Queue shader execution.
        pComputeInterface->submitForExecution(16, 16, 1); // TODO

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
            optionalError = lightCullingComputeShaderData.initialize(pRenderer);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        // Update shader data.
        auto optionalError = frustumGridComputeShaderData.updateData(
            pRenderer, renderResolution, inverseProjectionMatrix, true);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Rebind grid of frustums resource to light culling shader because it was re-created.
        optionalError = lightCullingComputeShaderData.pComputeInterface->bindResource(
            frustumGridComputeShaderData.resources.pCalculatedFrustums.get(),
            ComputeShaderData::FrustumGridComputeShader::ComputeShader::sCalculatedFrustumsShaderResourceName,
            ComputeResourceUsage::READ_ONLY_ARRAY_BUFFER);
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
            sizeof(LightingShaderResourceManager) == 240, "consider resetting new arrays here"); // NOLINT
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
