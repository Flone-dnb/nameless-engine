#include "HlslShaderResource.h"

// Custom.
#include "render/directx/pso/DirectXPso.h"
#include "render/Renderer.h"
#include "render/general/resources/GpuResourceManager.h"

// External.
#include "fmt/core.h"

namespace ne {

    HlslShaderCpuReadWriteResource::~HlslShaderCpuReadWriteResource() {}

    std::variant<std::unique_ptr<ShaderCpuReadWriteResource>, Error> HlslShaderCpuReadWriteResource::create(
        const std::string& sShaderResourceName,
        const std::string& sResourceAdditionalInfo,
        size_t iResourceSizeInBytes,
        Pso* pUsedPso,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource) {
        // Make sure PSO has correct type.
        const auto pDirectXPso = dynamic_cast<DirectXPso*>(pUsedPso);
        if (pDirectXPso == nullptr) [[unlikely]] {
            return Error("expected DirectX PSO");
        }

        // Get PSO internal resources.
        auto pMtxInternalPsoResources = pDirectXPso->getInternalResources();
        std::scoped_lock psoGuard(pMtxInternalPsoResources->first);

        // Find this resource by name.
        auto it = pMtxInternalPsoResources->second.rootParameterIndices.find(sShaderResourceName);
        if (it == pMtxInternalPsoResources->second.rootParameterIndices.end()) [[unlikely]] {
            return Error(fmt::format(
                "unable to find a shader resource by the specified name \"{}\", make sure the resource name "
                "is correct and that this resource is actually being used inside of your shader (otherwise "
                "the shader resource might be optimized out and the engine will not be able to see it)",
                sShaderResourceName));
        }

        // Save root parameter index.
        const auto iRootParameterIndex = it->second;

        // Create upload buffer per frame resource.
        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
            vResourceData;
        // Consider all read/write resources as cbuffers for now.
        const bool bIsShaderConstantBuffer = true;
        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
            auto result = pUsedPso->getRenderer()->getResourceManager()->createResourceWithCpuAccess(
                fmt::format(
                    "{} shader ({}/{}) CPU read/write resource \"{}\" #{}",
                    sResourceAdditionalInfo,
                    pUsedPso->getVertexShaderName(),
                    pUsedPso->getPixelShaderName(),
                    sShaderResourceName,
                    i),
                iResourceSizeInBytes,
                1,
                bIsShaderConstantBuffer);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addEntry();
                return error;
            }
            auto pUploadBuffer = std::get<std::unique_ptr<UploadBuffer>>(std::move(result));

            // Bind a CBV if this is a cbuffer.
            if (bIsShaderConstantBuffer) {
                auto optionalError =
                    pUploadBuffer->getInternalResource()->bindDescriptor(GpuResource::DescriptorType::CBV);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = optionalError.value();
                    error.addEntry();
                    return error;
                }
            }

            vResourceData[i] = std::move(pUploadBuffer);
        }

        // Create shader resource and return it.
        return std::unique_ptr<HlslShaderCpuReadWriteResource>(new HlslShaderCpuReadWriteResource(
            sShaderResourceName,
            iResourceSizeInBytes,
            std::move(vResourceData),
            onStartedUpdatingResource,
            onFinishedUpdatingResource,
            iRootParameterIndex));
    }

    HlslShaderCpuReadWriteResource::HlslShaderCpuReadWriteResource(
        const std::string& sResourceName,
        size_t iOriginalResourceSizeInBytes,
        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
            vResourceData,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource,
        UINT iRootParameterIndex)
        : ShaderCpuReadWriteResource(
              sResourceName,
              iOriginalResourceSizeInBytes,
              std::move(vResourceData),
              onStartedUpdatingResource,
              onFinishedUpdatingResource) {
        this->iRootParameterIndex = iRootParameterIndex;
    }

} // namespace ne
