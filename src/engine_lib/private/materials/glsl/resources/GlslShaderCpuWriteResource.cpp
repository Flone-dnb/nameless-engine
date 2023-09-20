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

    std::variant<std::unique_ptr<ShaderCpuWriteResource>, Error> GlslShaderCpuWriteResource::create(
        const std::string& sShaderResourceName,
        const std::string& sResourceAdditionalInfo,
        size_t iResourceSizeInBytes,
        const std::unordered_set<Pipeline*>& pipelinesToUse,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource) {
        // Make sure at least one pipeline is specified.
        if (pipelinesToUse.empty()) [[unlikely]] {
            return Error("expected at least one pipeline to be specified");
        }

        // Convert pipeline.
        const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(*pipelinesToUse.begin());
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

        // Find push constant indices to use.
        std::unordered_map<VulkanPipeline*, size_t> pushConstantIndices;
        for (const auto& pPipeline : pipelinesToUse) {
            // Convert pipeline.
            const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pPipeline);
            if (pVulkanPipeline == nullptr) [[unlikely]] {
                return Error("expected a Vulkan pipeline");
            }

            // Find a resource with the specified name in the descriptor set layout and update save index.
            auto pushConstantResult =
                GlslShaderResourceHelpers::getPushConstantIndex(pVulkanPipeline, sShaderResourceName);
            if (std::holds_alternative<Error>(pushConstantResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(pushConstantResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            // Save a pair of "pipeline" - "index of push constant".
            pushConstantIndices[pVulkanPipeline] = std::get<size_t>(pushConstantResult);
        }

        // Create shader resource.
        auto pShaderResource = std::unique_ptr<GlslShaderCpuWriteResource>(new GlslShaderCpuWriteResource(
            sShaderResourceName,
            iResourceSizeInBytes,
            onStartedUpdatingResource,
            onFinishedUpdatingResource,
            pushConstantIndices));

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
        const std::unordered_map<VulkanPipeline*, size_t>& pushConstantIndices)
        : ShaderCpuWriteResource(
              sResourceName,
              iOriginalResourceSizeInBytes,
              onStartedUpdatingResource,
              onFinishedUpdatingResource) {
        // Save push constant indices.
        mtxPushConstantIndices.second = pushConstantIndices;
    }

    std::optional<Error>
    GlslShaderCpuWriteResource::changeUsedPipelines(const std::unordered_set<Pipeline*>& pipelinesToUse) {
        std::scoped_lock guard(mtxPushConstantIndices.first);

        // Make sure at least one pipeline is specified.
        if (pipelinesToUse.empty()) [[unlikely]] {
            return Error("expected at least one pipeline to be specified");
        }

        // Clear currently used pipelines.
        mtxPushConstantIndices.second.clear();

        // Find push constant indices to use.
        for (const auto& pPipeline : pipelinesToUse) {
            // Convert pipeline.
            const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pPipeline);
            if (pVulkanPipeline == nullptr) [[unlikely]] {
                return Error("expected a Vulkan pipeline");
            }

            // Find a resource with our name in the descriptor set layout.
            auto pushConstantResult =
                GlslShaderResourceHelpers::getPushConstantIndex(pVulkanPipeline, getResourceName());
            if (std::holds_alternative<Error>(pushConstantResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(pushConstantResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            // Save a pair of "pipeline" - "index of push constant".
            mtxPushConstantIndices.second[pVulkanPipeline] = std::get<size_t>(pushConstantResult);
        }

        return {};
    }

    std::optional<Error> GlslShaderCpuWriteResource::onAfterAllPipelinesRefreshedResources() {
        std::scoped_lock guard(mtxPushConstantIndices.first);

        // Update push constant indices of all used pipelines.
        for (auto& [pPipeline, iPushConstantIndex] : mtxPushConstantIndices.second) {
            // Convert pipeline.
            const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pPipeline);
            if (pVulkanPipeline == nullptr) [[unlikely]] {
                return Error("expected a Vulkan pipeline");
            }

            // Find a resource with the our name in the descriptor set layout and update our index.
            auto pushConstantResult =
                GlslShaderResourceHelpers::getPushConstantIndex(pVulkanPipeline, getResourceName());
            if (std::holds_alternative<Error>(pushConstantResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(pushConstantResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            // Save new index.
            iPushConstantIndex = std::get<size_t>(pushConstantResult);
        }

        return {};
    }

} // namespace ne
