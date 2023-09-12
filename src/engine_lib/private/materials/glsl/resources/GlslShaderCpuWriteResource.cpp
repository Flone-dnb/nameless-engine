#include "GlslShaderCpuWriteResource.h"

// Standard.
#include <format>

// Custom.
#include "render/general/resources/GpuResourceManager.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"
#include "render/vulkan/resources/VulkanResourceManager.h"
#include "render/vulkan/resources/VulkanStorageResourceArrayManager.h"
#include "render/vulkan/VulkanRenderer.h"
#include "materials/glsl/resources/GlslShaderResourceHelpers.h"

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
            onFinishedUpdatingResource,
            pVulkanPipeline));

        // Find a resource with the specified name in the descriptor set layout and update our index.
        auto pushConstantResult =
            GlslShaderResourceHelpers::getPushConstantIndex(pVulkanPipeline, sShaderResourceName);
        if (std::holds_alternative<Error>(pushConstantResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(pushConstantResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        pShaderResource->iPushConstantIndex = std::get<size_t>(pushConstantResult);

        // Reserve a space for this shader resource's data in a storage buffer per frame resource.
        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
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

    GlslShaderCpuWriteResource::GlslShaderCpuWriteResource(
        const std::string& sResourceName,
        size_t iOriginalResourceSizeInBytes,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource,
        VulkanPipeline* pUsedPipeline)
        : ShaderCpuWriteResource(
              sResourceName,
              pUsedPipeline,
              iOriginalResourceSizeInBytes,
              onStartedUpdatingResource,
              onFinishedUpdatingResource) {}

    std::optional<Error> GlslShaderCpuWriteResource::bindToNewPipeline(Pipeline* pNewPipeline) {
        // Convert pipeline.
        const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pNewPipeline);
        if (pVulkanPipeline == nullptr) [[unlikely]] {
            return Error("expected a Vulkan pipeline");
        }

        // Find a resource with the specified name in the descriptor set layout and update our index.
        auto pushConstantResult =
            GlslShaderResourceHelpers::getPushConstantIndex(pVulkanPipeline, getResourceName());
        if (std::holds_alternative<Error>(pushConstantResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(pushConstantResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        iPushConstantIndex = std::get<size_t>(pushConstantResult);

        return {};
    }

} // namespace ne
