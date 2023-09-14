#include "HlslShaderCpuWriteResource.h"

// Standard.
#include <format>

// Custom.
#include "render/directx/pipeline/DirectXPso.h"
#include "materials/hlsl/resources/HlslShaderResourceHelpers.h"
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
        auto result =
            HlslShaderResourceHelpers::getRootParameterIndexFromPipeline(pUsedPipeline, sShaderResourceName);
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
        auto result =
            HlslShaderResourceHelpers::getRootParameterIndexFromPipeline(pNewPipeline, getResourceName());
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Save found resource index.
        iRootParameterIndex = std::get<UINT>(result);

        return {};
    }

} // namespace ne
