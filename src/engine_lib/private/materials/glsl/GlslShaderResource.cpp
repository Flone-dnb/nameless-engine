#include "GlslShaderResource.h"

// Custom.
#include "render/general/resources/GpuResourceManager.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"
#include "render/vulkan/resources/VulkanResourceManager.h"
#include "render/vulkan/resources/VulkanStorageResourceArrayManager.h"
#include "render/vulkan/VulkanRenderer.h"

// External.
#include "fmt/core.h"

namespace ne {

    GlslShaderCpuWriteResource::~GlslShaderCpuWriteResource() {}

    std::variant<std::unique_ptr<ShaderCpuWriteResource>, Error> GlslShaderCpuWriteResource::create(
        const std::string& sShaderResourceName,
        const std::string& sResourceAdditionalInfo,
        size_t iResourceSizeInBytes,
        Pipeline* pUsedPipeline,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource) {
        // Convert pipeline.
        const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pUsedPipeline);
        if (pVulkanPipeline == nullptr) [[unlikely]] {
            return Error("expected a Vulkan pipeline");
        }

        // Get pipeline's internal resources.
        const auto pMtxPipelineResources = pVulkanPipeline->getInternalResources();
        std::scoped_lock guard(pMtxPipelineResources->first);

        // Find a resource with the specified name in the descriptor set layout.
        auto result = getResourceBindingIndex(pVulkanPipeline, sShaderResourceName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto iBindingIndex = std::get<uint32_t>(result);

        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pVulkanPipeline->getRenderer());
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Get resource manager.
        const auto pResourceManager = pVulkanRenderer->getResourceManager();
        if (pResourceManager == nullptr) [[unlikely]] {
            return Error("resource manager is `nullptr`");
        }

        // Convert manager.
        const auto pVulkanResourceManager = dynamic_cast<VulkanResourceManager*>(pResourceManager);
        if (pVulkanResourceManager == nullptr) [[unlikely]] {
            return Error("expected a Vulkan resource manager");
        }

        // Get storage resource array manager.
        const auto pStorageResourceArrayManager = pVulkanResourceManager->getStorageResourceArrayManager();
        if (pStorageResourceArrayManager == nullptr) [[unlikely]] {
            return Error("storage resource array manager is `nullptr`");
        }

        // Create shader resource.
        auto pShaderResource = std::unique_ptr<GlslShaderCpuWriteResource>(new GlslShaderCpuWriteResource(
            pVulkanPipeline,
            iBindingIndex,
            sShaderResourceName,
            iResourceSizeInBytes,
            onStartedUpdatingResource,
            onFinishedUpdatingResource));

        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
            // Reserve a space for this shader resource's data in a storage buffer per frame resource.
            auto result = pStorageResourceArrayManager->reserveSlotsInArray(pShaderResource.get());
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            pShaderResource->vResourceData[i] =
                std::get<std::unique_ptr<VulkanStorageResourceArraySlot>>(std::move(result));
        }

        // Bind data to new descriptors.
        auto optionalError =
            pShaderResource->updateDescriptors(pVulkanPipeline, pVulkanRenderer, iBindingIndex);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return pShaderResource;
    }

    std::optional<Error> GlslShaderCpuWriteResource::updateBindingInfo(Pipeline* pNewPipeline) {
        // Convert pipeline.
        const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pNewPipeline);
        if (pVulkanPipeline == nullptr) [[unlikely]] {
            return Error("expected a Vulkan pipeline");
        }

        // Get pipeline's internal resources.
        const auto pMtxPipelineResources = pVulkanPipeline->getInternalResources();
        std::scoped_lock guard(pMtxPipelineResources->first, mtxUsedPipelineInfo.first);

        // Find a resource with the specified name in the descriptor set layout.
        auto result = getResourceBindingIndex(pVulkanPipeline, getResourceName());
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto iBindingIndex = std::get<uint32_t>(result);

        // Save new pipeline info.
        mtxUsedPipelineInfo.second.pPipeline = pVulkanPipeline;
        mtxUsedPipelineInfo.second.iBindingIndex = iBindingIndex;

        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pVulkanPipeline->getRenderer());
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Bind data to new descriptors.
        auto optionalError = updateDescriptors(pVulkanPipeline, pVulkanRenderer, iBindingIndex);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error>
    GlslShaderCpuWriteResource::rebindBufferToDescriptor(VulkanRenderer* pVulkanRenderer) {
        std::scoped_lock guard(mtxUsedPipelineInfo.first);

        // Bind new buffer to descriptors.
        auto optionalError = updateDescriptors(
            mtxUsedPipelineInfo.second.pPipeline, pVulkanRenderer, mtxUsedPipelineInfo.second.iBindingIndex);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // ! don't use `updateResource` here !

        return {};
    }

    GlslShaderCpuWriteResource::GlslShaderCpuWriteResource(
        VulkanPipeline* pUsedPipeline,
        uint32_t iBindingIndex,
        const std::string& sResourceName,
        size_t iOriginalResourceSizeInBytes,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource)
        : ShaderCpuWriteResource(
              sResourceName,
              iOriginalResourceSizeInBytes,
              onStartedUpdatingResource,
              onFinishedUpdatingResource) {
        // Save used pipeline info;
        std::scoped_lock guard(mtxUsedPipelineInfo.first);
        mtxUsedPipelineInfo.second.pPipeline = pUsedPipeline;
        mtxUsedPipelineInfo.second.iBindingIndex = iBindingIndex;
    }

    std::variant<uint32_t, Error> GlslShaderCpuWriteResource::getResourceBindingIndex(
        VulkanPipeline* pVulkanPipeline, const std::string& sShaderResourceName) {
        // Get pipeline's internal resources.
        const auto pMtxPipelineResources = pVulkanPipeline->getInternalResources();
        std::scoped_lock guard(pMtxPipelineResources->first);

        // Find a shader resource binding using our name.
        const auto it = pMtxPipelineResources->second.resourceBindings.find(sShaderResourceName);
        if (it == pMtxPipelineResources->second.resourceBindings.end()) [[unlikely]] {
            return Error(fmt::format(
                "unable to find a shader resource by the specified name \"{}\", make sure the resource name "
                "is correct and that this resource is actually being used inside of your shader (otherwise "
                "the shader resource might be optimized out and the engine will not be able to see it)",
                sShaderResourceName));
        }

        return it->second;
    }

    std::optional<Error> GlslShaderCpuWriteResource::updateDescriptors(
        VulkanPipeline* pVulkanPipeline, VulkanRenderer* pVulkanRenderer, uint32_t iBindingIndex) {
        // Get pipeline's internal resources.
        const auto pMtxPipelineResources = pVulkanPipeline->getInternalResources();
        std::scoped_lock guard(pMtxPipelineResources->first);

        // Get logical device.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("logical device is `nullptr`");
        }

        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
            // Prepare info to bind storage buffer slot to descriptor.
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = vResourceData[i]->getBuffer();
            bufferInfo.offset = vResourceData[i]->getOffsetFromArrayStart();
            bufferInfo.range = getOriginalResourceSizeInBytes();

            // Bind reserved space to descriptor.
            VkWriteDescriptorSet descriptorUpdateInfo{};
            descriptorUpdateInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorUpdateInfo.dstSet =
                pMtxPipelineResources->second.vDescriptorSets[i]; // descriptor to update
            descriptorUpdateInfo.dstBinding = iBindingIndex;      // descriptor binding
            descriptorUpdateInfo.dstArrayElement = 0;             // first descriptor in array to update
            descriptorUpdateInfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // type of descriptor
            descriptorUpdateInfo.descriptorCount = 1;       // how much descriptors in array to update
            descriptorUpdateInfo.pBufferInfo = &bufferInfo; // descriptor refers to buffer data

            // Update descriptor.
            vkUpdateDescriptorSets(pLogicalDevice, 1, &descriptorUpdateInfo, 0, nullptr);
        }

        return {};
    }

} // namespace ne
