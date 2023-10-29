#include "HlslShaderTextureResource.h"

// Standard.
#include <format>

// Custom.
#include "render/Renderer.h"
#include "material/hlsl/resources/HlslShaderResourceHelpers.h"
#include "render/directx/resources/DirectXResource.h"
#include "render/general/pipeline/Pipeline.h"
#include "render/directx/pipeline/DirectXPso.h"
#include "render/directx/resources/DirectXResourceManager.h"

namespace ne {

    std::variant<std::unique_ptr<ShaderTextureResource>, Error> HlslShaderTextureResource::create(
        const std::string& sShaderResourceName,
        const std::unordered_set<Pipeline*>& pipelinesToUse,
        std::unique_ptr<TextureHandle> pTextureToUse) {
        // Make sure at least one pipeline is specified.
        if (pipelinesToUse.empty()) [[unlikely]] {
            return Error("expected at least one pipeline to be specified");
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
            auto result = HlslShaderResourceHelpers::getRootParameterIndexFromPipeline(
                pDirectXPso, sShaderResourceName);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            // Save pair.
            rootParameterIndices[pDirectXPso] = std::get<UINT>(result);
        }

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
            sShaderResourceName, std::move(pTextureToUse), rootParameterIndices));
    }

    HlslShaderTextureResource::HlslShaderTextureResource(
        const std::string& sResourceName,
        std::unique_ptr<TextureHandle> pTextureToUse,
        const std::unordered_map<DirectXPso*, UINT>& rootParameterIndices)
        : ShaderTextureResource(sResourceName) {
        // Save parameters.
        mtxUsedTexture.second = std::move(pTextureToUse);
        mtxRootParameterIndices.second = rootParameterIndices;

        // Make sure there is at least one pipeline.
        if (mtxRootParameterIndices.second.empty()) [[unlikely]] {
            Error error("expected at least one pipeline to exist");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get resource manager.
        const auto pResourceManager = dynamic_cast<DirectXResourceManager*>(
            mtxRootParameterIndices.second.begin()->first->getRenderer()->getResourceManager());
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

    std::optional<Error> HlslShaderTextureResource::onAfterAllPipelinesRefreshedResources() {
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

    std::optional<Error>
    HlslShaderTextureResource::useNewTexture(std::unique_ptr<TextureHandle> pTextureToUse) {
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

    std::optional<Error>
    HlslShaderTextureResource::changeUsedPipelines(const std::unordered_set<Pipeline*>& pipelinesToUse) {
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

} // namespace ne
