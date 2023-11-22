#include "ShaderLightArray.h"

// Standard.
#include <format>

// Custom.
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif
#include "render/vulkan/VulkanRenderer.h"
#include "io/Logger.h"
#include "render/general/pipeline/PipelineManager.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"

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
}
