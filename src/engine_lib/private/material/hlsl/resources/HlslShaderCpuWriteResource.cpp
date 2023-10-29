#include "HlslShaderCpuWriteResource.h"

// Standard.
#include <format>

// Custom.
#include "render/directx/pipeline/DirectXPso.h"
#include "material/hlsl/resources/HlslShaderResourceHelpers.h"
#include "render/Renderer.h"
#include "render/general/resources/GpuResourceManager.h"

namespace ne {

    std::variant<std::unique_ptr<ShaderCpuWriteResource>, Error> HlslShaderCpuWriteResource::create(
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

        // Get resource manager.
        const auto pResourceManager = (*pipelinesToUse.begin())->getRenderer()->getResourceManager();
        if (pResourceManager == nullptr) [[unlikely]] {
            return Error("resource manager is `nullptr`");
        }

        // Find a root parameter index for each pipeline.
        std::unordered_map<DirectXPso*, UINT> rootParameterIndices;
        for (const auto& pPipeline : pipelinesToUse) {
            // Convert pipeline.
            const auto pDirectXPso = dynamic_cast<DirectXPso*>(pPipeline);
            if (pDirectXPso == nullptr) [[unlikely]] {
                return Error("expected a DirectX PSO");
            }

            // Find a resource with the specified name in the root signature.
            auto result =
                HlslShaderResourceHelpers::getRootParameterIndexFromPipeline(pPipeline, sShaderResourceName);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            // Save pair.
            rootParameterIndices[pDirectXPso] = std::get<UINT>(result);
        }

        // Create upload buffer per frame resource.
        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
            vResourceData;
        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
            // Create upload buffer.
            auto result = pResourceManager->createResourceWithCpuWriteAccess(
                std::format(
                    "{} shader CPU write resource \"{}\" frame #{}",
                    sResourceAdditionalInfo,
                    sShaderResourceName,
                    i),
                iResourceSizeInBytes,
                1,
                false);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            auto pUploadBuffer = std::get<std::unique_ptr<UploadBuffer>>(std::move(result));

            // Convert to DirectX resource.
            const auto pDirectXResource =
                dynamic_cast<DirectXResource*>(pUploadBuffer->getInternalResource());
            if (pDirectXResource == nullptr) [[unlikely]] {
                return Error("expected a DirectX resource");
            }

            // Bind a CBV.
            auto optionalError = pDirectXResource->bindDescriptor(DirectXDescriptorType::CBV);
            if (optionalError.has_value()) [[unlikely]] {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                return error;
            }

            vResourceData[i] = std::move(pUploadBuffer);
        }

        // Create shader resource and return it.
        return std::unique_ptr<HlslShaderCpuWriteResource>(new HlslShaderCpuWriteResource(
            sShaderResourceName,
            iResourceSizeInBytes,
            std::move(vResourceData),
            onStartedUpdatingResource,
            onFinishedUpdatingResource,
            rootParameterIndices));
    }

    HlslShaderCpuWriteResource::HlslShaderCpuWriteResource(
        const std::string& sResourceName,
        size_t iOriginalResourceSizeInBytes,
        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
            vResourceData,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource,
        const std::unordered_map<DirectXPso*, UINT>& rootParameterIndices)
        : ShaderCpuWriteResource(
              sResourceName,
              iOriginalResourceSizeInBytes,
              onStartedUpdatingResource,
              onFinishedUpdatingResource) {
        // Save data.
        this->vResourceData = std::move(vResourceData);

        // Save indices.
        mtxRootParameterIndices.second = rootParameterIndices;
    }

    std::optional<Error>
    HlslShaderCpuWriteResource::changeUsedPipelines(const std::unordered_set<Pipeline*>& pipelinesToUse) {
        std::scoped_lock guard(mtxRootParameterIndices.first);

        // Make sure at least one pipeline is specified.
        if (pipelinesToUse.empty()) [[unlikely]] {
            return Error("expected at least one pipeline to be specified");
        }

        // Clear currently used pipelines.
        mtxRootParameterIndices.second.clear();

        for (const auto& pPipeline : pipelinesToUse) {
            // Convert pipeline.
            const auto pDirectXPso = dynamic_cast<DirectXPso*>(pPipeline);
            if (pDirectXPso == nullptr) [[unlikely]] {
                return Error("expected a DirectX PSO");
            }

            // Find a resource with our name in the root signature.
            auto result =
                HlslShaderResourceHelpers::getRootParameterIndexFromPipeline(pPipeline, getResourceName());
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            // Save pair.
            mtxRootParameterIndices.second[pDirectXPso] = std::get<UINT>(result);
        }

        return {};
    }

    std::optional<Error> HlslShaderCpuWriteResource::onAfterAllPipelinesRefreshedResources() {
        std::scoped_lock guard(mtxRootParameterIndices.first);

        // Update root parameter indices of all used pipelines.
        for (auto& [pPipeline, iRootParameterIndex] : mtxRootParameterIndices.second) {
            // Find a resource with our name in the root signature.
            auto result =
                HlslShaderResourceHelpers::getRootParameterIndexFromPipeline(pPipeline, getResourceName());
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            // Save new index.
            iRootParameterIndex = std::get<UINT>(result);
        }

        return {};
    }

} // namespace ne
