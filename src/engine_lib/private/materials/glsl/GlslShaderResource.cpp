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
        // That's pretty much it. We just need to check that new pipeline has resources with our name.
        // Storage array will handle descriptor updates.

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

        // Get pipeline's internal resources.
        const auto pMtxPipelineResources = pVulkanPipeline->getInternalResources();
        std::scoped_lock guard(pMtxPipelineResources->first);

        // Find a resource with the specified name in the descriptor set layout.
        auto result = getResourceBindingIndex(pVulkanPipeline, getResourceName());
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // That's pretty much it. We just need to check that new pipeline has resources with our name.
        // Storage array will handle descriptor updates.

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

} // namespace ne
