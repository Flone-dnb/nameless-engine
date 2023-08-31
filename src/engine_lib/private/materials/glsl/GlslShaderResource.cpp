#include "GlslShaderResource.h"

// Standard.
#include <format>

// Custom.
#include "render/general/resources/GpuResourceManager.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"
#include "render/vulkan/resources/VulkanResourceManager.h"
#include "render/vulkan/resources/VulkanStorageResourceArrayManager.h"
#include "render/vulkan/VulkanRenderer.h"

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

        // Get resource manager.
        const auto pResourceManager = pVulkanPipeline->getRenderer()->getResourceManager();
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
            sShaderResourceName,
            iResourceSizeInBytes,
            onStartedUpdatingResource,
            onFinishedUpdatingResource));

        // Find a resource with the specified name in the descriptor set layout and update our index.
        auto optionalError = pShaderResource->updatePushConstantIndex(pVulkanPipeline);
        if (optionalError.has_value()) {
            auto error = std::move(optionalError.value());
            error.addCurrentLocationToErrorStack();
            return error;
        }

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

        return pShaderResource;
    }

    std::optional<Error> GlslShaderCpuWriteResource::updateBindingInfo(Pipeline* pNewPipeline) {
        // Convert pipeline.
        const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pNewPipeline);
        if (pVulkanPipeline == nullptr) [[unlikely]] {
            return Error("expected a Vulkan pipeline");
        }

        // Find a resource with the specified name in the descriptor set layout and update our index.
        auto optionalError = updatePushConstantIndex(pVulkanPipeline);
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    GlslShaderCpuWriteResource::GlslShaderCpuWriteResource(
        const std::string& sResourceName,
        size_t iOriginalResourceSizeInBytes,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource)
        : ShaderCpuWriteResource(
              sResourceName,
              iOriginalResourceSizeInBytes,
              onStartedUpdatingResource,
              onFinishedUpdatingResource) {}

    [[nodiscard]] std::optional<Error>
    GlslShaderCpuWriteResource::updatePushConstantIndex(VulkanPipeline* pVulkanPipeline) {
        // Get pipeline's internal resources.
        const auto pMtxPipelineResources = pVulkanPipeline->getInternalResources();
        std::scoped_lock guard(pMtxPipelineResources->first);

        const auto sShaderResourceName = getResourceName();

        // Find a shader resource binding using our name.
        const auto bindingIt = pMtxPipelineResources->second.resourceBindings.find(sShaderResourceName);
        if (bindingIt == pMtxPipelineResources->second.resourceBindings.end()) [[unlikely]] {
            return Error(std::format(
                "unable to find a shader resource by the specified name \"{}\", make sure the resource name "
                "is correct and that this resource is actually being used inside of your shader (otherwise "
                "the shader resource might be optimized out and the engine will not be able to see it)",
                sShaderResourceName));
        }

        // For now (maybe will be changed later), make sure our resource name is referenced in push constants
        // since we will expect it during the rendering.
        if (!pMtxPipelineResources->second.pushConstantUintFieldNames.has_value()) [[unlikely]] {
            return Error("expected push constants to be used");
        }
        const auto referencedNameIt =
            pMtxPipelineResources->second.pushConstantUintFieldNames->find(sShaderResourceName);
        if (referencedNameIt == pMtxPipelineResources->second.pushConstantUintFieldNames->end())
            [[unlikely]] {
            return Error(std::format(
                "expected to find a shader resource \"{}\" to be referenced in push constants",
                sShaderResourceName));
        }

        // Make sure smallest binding index referenced by push constants is set.
        if (!pMtxPipelineResources->second.smallestBindingIndexReferencedByPushConstants.has_value())
            [[unlikely]] {
            return Error("expected smallest binding index referenced by push constants to be set");
        }

        // Get values to calculate push constant index.
        const auto iBindingIndex = bindingIt->second;
        const auto iSmallestBindingIndexReferencedByPushConstants =
            pMtxPipelineResources->second.smallestBindingIndexReferencedByPushConstants.value();

        // Make sure we won't go below zero.
        if (iBindingIndex < iSmallestBindingIndexReferencedByPushConstants) [[unlikely]] {
            return Error(std::format(
                "failed to calculate push constant index because resulting index will be smaller than zero"));
        }

        // Now calculate push constant index that we will update.
        iPushConstantIndex = iBindingIndex - iSmallestBindingIndexReferencedByPushConstants;

        return {};
    }

} // namespace ne
