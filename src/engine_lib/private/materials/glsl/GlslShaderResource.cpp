#include "GlslShaderResource.h"

// Custom.
#include "render/Renderer.h"
#include "render/general/resources/GpuResourceManager.h"

// External.
#include "fmt/core.h"

namespace ne {

    GlslShaderCpuReadWriteResource::~GlslShaderCpuReadWriteResource() {}

    std::variant<std::unique_ptr<ShaderCpuReadWriteResource>, Error> GlslShaderCpuReadWriteResource::create(
        const std::string& sShaderResourceName,
        const std::string& sResourceAdditionalInfo,
        size_t iResourceSizeInBytes,
        Pso* pUsedPso,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource) {
        throw std::runtime_error("not implemented");
        //        // Find a resource with the specified name in the root signature.
        //        auto result = getRootParameterIndexFromPso(pUsedPso, sShaderResourceName);
        //        if (std::holds_alternative<Error>(result)) {
        //            auto error = std::get<Error>(std::move(result));
        //            error.addEntry();
        //            return error;
        //        }
        //        const auto iRootParameterIndex = std::get<UINT>(result);

        //               // Consider all read/write resources as cbuffers for now.
        //        const bool bIsShaderConstantBuffer = true;

        //               // Create upload buffer per frame resource.
        //        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
        //            vResourceData;
        //        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
        //            auto result =
        //            pUsedPso->getRenderer()->getResourceManager()->createResourceWithCpuAccess(
        //                fmt::format(
        //                    "{} shader ({}/{}) CPU read/write resource \"{}\" #{}",
        //                    sResourceAdditionalInfo,
        //                    pUsedPso->getVertexShaderName(),
        //                    pUsedPso->getPixelShaderName(),
        //                    sShaderResourceName,
        //                    i),
        //                iResourceSizeInBytes,
        //                1,
        //                bIsShaderConstantBuffer);
        //            if (std::holds_alternative<Error>(result)) [[unlikely]] {
        //                auto error = std::get<Error>(std::move(result));
        //                error.addEntry();
        //                return error;
        //            }
        //            auto pUploadBuffer = std::get<std::unique_ptr<UploadBuffer>>(std::move(result));

        //                   // Bind a CBV if this is a cbuffer.
        //            if (bIsShaderConstantBuffer) {
        //                auto optionalError =
        //                    pUploadBuffer->getInternalResource()->bindDescriptor(GpuResource::DescriptorType::CBV);
        //                if (optionalError.has_value()) [[unlikely]] {
        //                    auto error = optionalError.value();
        //                    error.addEntry();
        //                    return error;
        //                }
        //            }

        //            vResourceData[i] = std::move(pUploadBuffer);
        //        }

        //               // Create shader resource and return it.
        //        return std::unique_ptr<HlslShaderCpuReadWriteResource>(new HlslShaderCpuReadWriteResource(
        //            sShaderResourceName,
        //            iResourceSizeInBytes,
        //            std::move(vResourceData),
        //            onStartedUpdatingResource,
        //            onFinishedUpdatingResource,
        //            iRootParameterIndex));
    }

    std::optional<Error> GlslShaderCpuReadWriteResource::updateBindingInfo(Pso* pNewPso) {
        throw std::runtime_error("not implemented");
    }

    GlslShaderCpuReadWriteResource::GlslShaderCpuReadWriteResource(
        const std::string& sResourceName,
        size_t iOriginalResourceSizeInBytes,
        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
            vResourceData,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource)
        : ShaderCpuReadWriteResource(
              sResourceName,
              iOriginalResourceSizeInBytes,
              std::move(vResourceData),
              onStartedUpdatingResource,
              onFinishedUpdatingResource) {}

} // namespace ne
