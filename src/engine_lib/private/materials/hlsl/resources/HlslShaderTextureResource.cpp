#include "HlslShaderTextureResource.h"

// Standard.
#include <format>

// Custom.
#include "render/Renderer.h"
#include "materials/hlsl/resources/HlslShaderResourceHelpers.h"
#include "render/directx/resources/DirectXResource.h"
#include "render/general/pipeline/Pipeline.h"
#include "render/directx/resources/DirectXResourceManager.h"

namespace ne {

    std::variant<std::unique_ptr<ShaderTextureResource>, Error> HlslShaderTextureResource::create(
        const std::string& sShaderResourceName,
        Pipeline* pUsedPipeline,
        std::unique_ptr<TextureHandle> pTextureToUse) {
        // Find a resource with the specified name in the root signature.
        auto result =
            HlslShaderResourceHelpers::getRootParameterIndexFromPipeline(pUsedPipeline, sShaderResourceName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto iRootParameterIndex = std::get<UINT>(result);

        // Convert to DirectX resource.
        const auto pDirectXResource = dynamic_cast<DirectXResource*>(pTextureToUse->getResource());
        if (pDirectXResource == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }

        // Bind a SRV.
        auto optionalError = pDirectXResource->bindDescriptor(DirectXDescriptorType::SRV);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Create shader resource and return it.
        return std::unique_ptr<HlslShaderTextureResource>(new HlslShaderTextureResource(
            sShaderResourceName, pUsedPipeline, std::move(pTextureToUse), iRootParameterIndex));
    }

    HlslShaderTextureResource::HlslShaderTextureResource(
        const std::string& sResourceName,
        Pipeline* pUsedPipeline,
        std::unique_ptr<TextureHandle> pTextureToUse,
        UINT iRootParameterIndex)
        : ShaderTextureResource(sResourceName, pUsedPipeline) {
        // Save parameters.
        mtxUsedTexture.second = std::move(pTextureToUse);
        this->iRootParameterIndex = iRootParameterIndex;

        // Get resource manager
        const auto pResourceManager =
            dynamic_cast<DirectXResourceManager*>(pUsedPipeline->getRenderer()->getResourceManager());
        if (pResourceManager == nullptr) [[unlikely]] {
            Error error("expected a DirectX resource manager");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get SRV heap.
        const auto pSrvHeap = pResourceManager->getCbvSrvUavHeap();

        // Save SRV heap start.
        iSrvHeapStart = pSrvHeap->getInternalHeap()->GetGPUDescriptorHandleForHeapStart().ptr;

        // Save SRV descriptor size.
        iSrvDescriptorSize = pSrvHeap->getDescriptorSize();

        // Convert to DirectX resource.
        const auto pDirectXResource = dynamic_cast<DirectXResource*>(mtxUsedTexture.second->getResource());
        if (pDirectXResource == nullptr) [[unlikely]] {
            Error error("expected a DirectX resource");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get descriptor binded to this texture.
        pTextureSrv = pDirectXResource->getDescriptor(DirectXDescriptorType::SRV);
        if (pTextureSrv == nullptr) [[unlikely]] {
            Error error("expected the texture to have binded SRV");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    std::optional<Error> HlslShaderTextureResource::bindToNewPipeline(Pipeline* pNewPipeline) {
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

    std::optional<Error> HlslShaderTextureResource::updateTextureDescriptor(
        std::unique_ptr<TextureHandle> pTextureToUse, Pipeline* pUsedPipeline) {
        std::scoped_lock guard(mtxUsedTexture.first);

        // Note: don't unbind SRV from old resource (it can be used by someone else).

        // Replace used texture.
        mtxUsedTexture.second = std::move(pTextureToUse);

        // Convert to DirectX resource.
        const auto pDirectXResource = dynamic_cast<DirectXResource*>(mtxUsedTexture.second->getResource());
        if (pDirectXResource == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }

        // Bind a SRV.
        auto optionalError = pDirectXResource->bindDescriptor(DirectXDescriptorType::SRV);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Get descriptor binded to this texture.
        pTextureSrv = pDirectXResource->getDescriptor(DirectXDescriptorType::SRV);
        if (pTextureSrv == nullptr) [[unlikely]] {
            Error error("expected the texture to have binded SRV");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        return {};
    }

} // namespace ne
