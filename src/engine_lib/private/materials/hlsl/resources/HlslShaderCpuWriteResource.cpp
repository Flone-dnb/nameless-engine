#include "HlslShaderCpuWriteResource.h"

// Standard.
#include <format>

// Custom.
#include "render/directx/pipeline/DirectXPso.h"
#include "render/Renderer.h"
#include "render/general/resources/GpuResourceManager.h"

namespace ne {

    HlslShaderCpuWriteResource::~HlslShaderCpuWriteResource() {}

    std::variant<std::unique_ptr<ShaderCpuWriteResource>, Error> HlslShaderCpuWriteResource::create(
        const std::string& sShaderResourceName,
        const std::string& sResourceAdditionalInfo,
        size_t iResourceSizeInBytes,
        Pipeline* pUsedPipeline,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource) {
        // Find a resource with the specified name in the root signature.
        auto result = getRootParameterIndexFromPipeline(pUsedPipeline, sShaderResourceName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto iRootParameterIndex = std::get<UINT>(result);

        // Create upload buffer per frame resource.
        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
            vResourceData;
        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
            // Create upload buffer.
            auto result =
                pUsedPipeline->getRenderer()->getResourceManager()->createResourceWithCpuWriteAccess(
                    std::format(
                        "{} shader ({}/{}) CPU write resource \"{}\" frame #{}",
                        sResourceAdditionalInfo,
                        pUsedPipeline->getVertexShaderName(),
                        pUsedPipeline->getPixelShaderName(),
                        sShaderResourceName,
                        i),
                    iResourceSizeInBytes,
                    1,
                    CpuVisibleShaderResourceUsageDetails(false));
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
            pUsedPipeline,
            iResourceSizeInBytes,
            std::move(vResourceData),
            onStartedUpdatingResource,
            onFinishedUpdatingResource,
            iRootParameterIndex));
    }

    HlslShaderCpuWriteResource::HlslShaderCpuWriteResource(
        const std::string& sResourceName,
        Pipeline* pUsedPipeline,
        size_t iOriginalResourceSizeInBytes,
        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
            vResourceData,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource,
        UINT iRootParameterIndex)
        : ShaderCpuWriteResource(
              sResourceName,
              pUsedPipeline,
              iOriginalResourceSizeInBytes,
              onStartedUpdatingResource,
              onFinishedUpdatingResource) {
        this->vResourceData = std::move(vResourceData);
        this->iRootParameterIndex = iRootParameterIndex;
    }

    std::optional<Error> HlslShaderCpuWriteResource::bindToNewPipeline(Pipeline* pNewPipeline) {
        // Find a resource with the specified name in the root signature.
        auto result = getRootParameterIndexFromPipeline(pNewPipeline, getResourceName());
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Save found resource index.
        iRootParameterIndex = std::get<UINT>(result);

        return {};
    }

    std::variant<UINT, Error> HlslShaderCpuWriteResource::getRootParameterIndexFromPipeline(
        Pipeline* pPipeline, const std::string& sShaderResourceName) {
        // Make sure PSO has correct type.
        const auto pDirectXPso = dynamic_cast<DirectXPso*>(pPipeline);
        if (pDirectXPso == nullptr) [[unlikely]] {
            return Error("expected DirectX PSO");
        }

        // Get PSO internal resources.
        auto pMtxInternalPsoResources = pDirectXPso->getInternalResources();
        std::scoped_lock psoGuard(pMtxInternalPsoResources->first);

        // Find this resource by name.
        auto it = pMtxInternalPsoResources->second.rootParameterIndices.find(sShaderResourceName);
        if (it == pMtxInternalPsoResources->second.rootParameterIndices.end()) [[unlikely]] {
            return Error(std::format(
                "unable to find a shader resource by the specified name \"{}\", make sure the resource name "
                "is correct and that this resource is actually being used inside of your shader (otherwise "
                "the shader resource might be optimized out and the engine will not be able to see it)",
                sShaderResourceName));
        }

        return it->second;
    }

} // namespace ne
