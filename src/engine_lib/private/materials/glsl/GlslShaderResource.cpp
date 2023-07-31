#include "GlslShaderResource.h"

// Custom.
#include "render/Renderer.h"
#include "render/general/resources/GpuResourceManager.h"

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
        throw std::runtime_error("not implemented");
        //        // Find a resource with the specified name in the root signature.
        //        auto result = getRootParameterIndexFromPso(pUsedPso, sShaderResourceName);
        //        if (std::holds_alternative<Error>(result)) {
        //            auto error = std::get<Error>(std::move(result));
        //            error.addCurrentLocationToErrorStack();
        //            return error;
        //        }
        //        const auto iRootParameterIndex = std::get<UINT>(result);

        //               // Create upload buffer per frame resource.
        //        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
        //            vResourceData;
        //        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
        //            auto result =
        //            pUsedPso->getRenderer()->getResourceManager()->createResourceWithCpuWriteAccess(
        //                fmt::format(
        //                    "{} shader ({}/{}) CPU write resource \"{}\" frame #{}",
        //                    sResourceAdditionalInfo,
        //                    pUsedPso->getVertexShaderName(),
        //                    pUsedPso->getPixelShaderName(),
        //                    sShaderResourceName,
        //                    i),
        //                iResourceSizeInBytes,
        //                1,
        //                CpuVisibleShaderResourceUsageDetails(false));
        //            if (std::holds_alternative<Error>(result)) [[unlikely]] {
        //                auto error = std::get<Error>(std::move(result));
        //                error.addCurrentLocationToErrorStack();
        //                return error;
        //            }
        //            auto pUploadBuffer = std::get<std::unique_ptr<UploadBuffer>>(std::move(result));

        //                   // Bind a CBV.
        //            auto optionalError =
        //                pUploadBuffer->getInternalResource()->bindDescriptor(GpuResource::DescriptorType::CBV);
        //            if (optionalError.has_value()) [[unlikely]] {
        //                auto error = optionalError.value();
        //                error.addCurrentLocationToErrorStack();
        //                return error;
        //            }

        //            vResourceData[i] = std::move(pUploadBuffer);
        //        }

        //               // Create shader resource and return it.
        //        return std::unique_ptr<HlslShaderCpuWriteResource>(new HlslShaderCpuWriteResource(
        //            sShaderResourceName,
        //            iResourceSizeInBytes,
        //            std::move(vResourceData),
        //            onStartedUpdatingResource,
        //            onFinishedUpdatingResource,
        //            iRootParameterIndex));
    }

    std::optional<Error> GlslShaderCpuWriteResource::updateBindingInfo(Pipeline* pNewPipeline) {
        throw std::runtime_error("not implemented");
    }

    GlslShaderCpuWriteResource::GlslShaderCpuWriteResource(
        const std::string& sResourceName,
        size_t iOriginalResourceSizeInBytes,
        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
            vResourceData,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource)
        : ShaderCpuWriteResource(
              sResourceName,
              iOriginalResourceSizeInBytes,
              std::move(vResourceData),
              onStartedUpdatingResource,
              onFinishedUpdatingResource) {}

} // namespace ne
