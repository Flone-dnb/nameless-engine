#include "HlslShaderTextureResourceBinding.h"

// Standard.
#include <format>

// Custom.
#include "render/Renderer.h"
#include "render/general/pipeline/Pipeline.h"
#include "render/directx/pipeline/DirectXPso.h"
#include "render/directx/resource/DirectXResourceManager.h"
#include "render/general/pipeline/PipelineManager.h"
#include "material/TextureHandle.h"

namespace ne {

    std::variant<std::shared_ptr<ContinuousDirectXDescriptorRange>, Error>
    HlslShaderTextureResourceBinding::getSrvDescriptorRange(
        DirectXPso* pPipeline, const std::string& sShaderResourceName) {
        // Get resource manager.
        const auto pResourceManager =
            dynamic_cast<DirectXResourceManager*>(pPipeline->getRenderer()->getResourceManager());
        if (pResourceManager == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource manager");
        }

        // Get SRV heap for later usage.
        const auto pSrvHeap = pResourceManager->getCbvSrvUavHeap();

        // Lock PSO resources.
        const auto pMtxPipelineResources = pPipeline->getInternalResources();
        const auto pMtxShaderConstants = pPipeline->getShaderConstants();
        std::scoped_lock guard(pMtxPipelineResources->first, pMtxShaderConstants->first);

        // Find a resource with the specified name in the root signature.
        auto result = pPipeline->getRootParameterIndex(sShaderResourceName);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto iRootParameterIndex = std::get<unsigned int>(result);

        std::shared_ptr<ContinuousDirectXDescriptorRange> pSrvDescriptorRange = nullptr;

        // Check if a descriptor table for our shader resource is already created in the pipeline.
        auto& descriptorRanges = pMtxPipelineResources->second.descriptorRangesToBind;
        const auto descriptorRangeIt = descriptorRanges.find(iRootParameterIndex);
        if (descriptorRangeIt == descriptorRanges.end()) {
            // It's OK, we might be the first one to bind a resource to it.

            // Create a new SRV range.
            auto rangeResult = pSrvHeap->allocateContinuousDescriptorRange(
                std::format("texture array for shader resource \"{}\"", sShaderResourceName), []() {
                    // Nothing to notify here because we don't store descriptor offsets and instead query
                    // the offsets during the `draw` for simplicity.
                });
            if (std::holds_alternative<Error>(rangeResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(rangeResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            pSrvDescriptorRange =
                std::get<std::shared_ptr<ContinuousDirectXDescriptorRange>>(std::move(rangeResult));

            // Save in the pipeline.
            pMtxPipelineResources->second.descriptorRangesToBind[iRootParameterIndex] = pSrvDescriptorRange;
        } else {
            // Save the pointer.
            pSrvDescriptorRange = descriptorRangeIt->second;
        }

        return pSrvDescriptorRange;
    }

    std::variant<std::unique_ptr<ShaderTextureResourceBinding>, Error>
    HlslShaderTextureResourceBinding::create(
        const std::string& sShaderResourceName,
        const std::unordered_set<Pipeline*>& pipelinesToUse,
        std::unique_ptr<TextureHandle> pTextureToUse) {
        // Make sure at least one pipeline is specified.
        if (pipelinesToUse.empty()) [[unlikely]] {
            return Error("expected at least one pipeline to be specified");
        }
        const auto pRenderer = (*pipelinesToUse.begin())->getRenderer();

        // Convert to DirectX resource.
        const auto pDirectXResource = dynamic_cast<DirectXResource*>(pTextureToUse->getResource());
        if (pDirectXResource == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }

        // Make sure no pipeline will re-create its internal resources because we will create raw pointers
        // to pipelines' internal resources.
        // After we create a new shader resource binding object we can release the mutex
        // since shader resource bindings are notified after pipelines re-create their internal resources.
        const auto pMtxGraphicsPipelines = pRenderer->getPipelineManager()->getGraphicsPipelines();
        std::scoped_lock pipelinesGuard(pMtxGraphicsPipelines->first);

        std::unordered_map<DirectXPso*, size_t> shaderConstantOffsetPerPipeline;

        for (const auto& pPipeline : pipelinesToUse) {
            // Convert type.
            const auto pDirectXPso = dynamic_cast<DirectXPso*>(pPipeline);
            if (pDirectXPso == nullptr) [[unlikely]] {
                return Error("expected a DirectX PSO");
            }

            // Get shader constant offset.
            auto offsetResult = pPipeline->getUintConstantOffset(sShaderResourceName);
            if (std::holds_alternative<Error>(offsetResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(offsetResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            const auto iShaderConstantOffset = std::get<size_t>(offsetResult);

            // Get SRV descriptor range.
            auto rangeResult = getSrvDescriptorRange(pDirectXPso, sShaderResourceName);
            if (std::holds_alternative<Error>(rangeResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(rangeResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            const auto pSrvDescriptorRange =
                std::get<std::shared_ptr<ContinuousDirectXDescriptorRange>>(rangeResult);

            // Bind SRV from the range to our texture.
            auto optionalError = pDirectXResource->bindDescriptor(
                DirectXDescriptorType::SRV, pSrvDescriptorRange, bBindSrvToCubemapFaces);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }

            // Save range.
            shaderConstantOffsetPerPipeline[pDirectXPso] = iShaderConstantOffset;
        }

        // Pass data to the binding.
        auto pResourceBinding =
            std::unique_ptr<HlslShaderTextureResourceBinding>(new HlslShaderTextureResourceBinding(
                sShaderResourceName, std::move(pTextureToUse), std::move(shaderConstantOffsetPerPipeline)));

        // At this point we can release the pipelines mutex.

        return pResourceBinding;
    }

    HlslShaderTextureResourceBinding::HlslShaderTextureResourceBinding(
        const std::string& sResourceName,
        std::unique_ptr<TextureHandle> pTextureToUse,
        std::unordered_map<DirectXPso*, size_t>&& shaderConstantOffsetPerPipeline)
        : ShaderTextureResourceBinding(sResourceName) {
        // Save parameters.
        mtxUsedTexture.second = std::move(pTextureToUse);
        mtxShaderConstantOffsetPerPipeline.second = std::move(shaderConstantOffsetPerPipeline);

        // Self check: make sure there is at least one pipeline.
        if (mtxShaderConstantOffsetPerPipeline.second.empty()) [[unlikely]] {
            Error error("expected at least one pipeline to exist");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    std::optional<Error> HlslShaderTextureResourceBinding::onAfterAllPipelinesRefreshedResources() {
        std::scoped_lock guard(mtxShaderConstantOffsetPerPipeline.first);

        // Collect used pipelines into a set.
        std::unordered_set<Pipeline*> pipelinesToUse;
        for (const auto& [pPipeline, iShaderConstantOffset] : mtxShaderConstantOffsetPerPipeline.second) {
            pipelinesToUse.insert(pPipeline);
        }

        // Rebind descriptor for each pipeline.
        auto optionalError = changeUsedPipelines(pipelinesToUse);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error>
    HlslShaderTextureResourceBinding::useNewTexture(std::unique_ptr<TextureHandle> pTextureToUse) {
        std::scoped_lock guard(mtxUsedTexture.first, mtxShaderConstantOffsetPerPipeline.first);

        // Note: don't unbind SRV from the old texture because that texture can be used by someone else.
        // (when the old texture will be destroyed it will automatically free its used descriptors).

        // Get SRV range that old texture used.
        std::shared_ptr<ContinuousDirectXDescriptorRange> pUsedSrvRange = nullptr;
        {
            const auto pOldDirectXTexture =
                dynamic_cast<DirectXResource*>(mtxUsedTexture.second->getResource());
            if (pOldDirectXTexture == nullptr) [[unlikely]] {
                return Error("expected a DirectX resource");
            }

            const auto pOldSrvDescriptor = pOldDirectXTexture->getDescriptor(DirectXDescriptorType::SRV);
            if (pOldSrvDescriptor == nullptr) [[unlikely]] {
                return Error("expected an SRV to be bound");
            }

            pUsedSrvRange = pOldSrvDescriptor->getDescriptorRange();
            if (pUsedSrvRange == nullptr) [[unlikely]] {
                return Error("expected descriptor range to be valid");
            }
        }

        // Replace used texture.
        mtxUsedTexture.second = std::move(pTextureToUse);

        // Convert to DirectX resource.
        const auto pDirectXResource = dynamic_cast<DirectXResource*>(mtxUsedTexture.second->getResource());
        if (pDirectXResource == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }

        // Bind an SRV to the new texture.
        auto optionalError = pDirectXResource->bindDescriptor(
            DirectXDescriptorType::SRV, pUsedSrvRange, bBindSrvToCubemapFaces);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return {};
    }

    std::optional<Error> HlslShaderTextureResourceBinding::changeUsedPipelines(
        const std::unordered_set<Pipeline*>& pipelinesToUse) {
        std::scoped_lock guard(mtxUsedTexture.first, mtxShaderConstantOffsetPerPipeline.first);

        // Make sure at least one pipeline is specified.
        if (pipelinesToUse.empty()) [[unlikely]] {
            return Error("expected at least one pipeline to be specified");
        }

        // Convert to DirectX resource.
        const auto pDirectXResource = dynamic_cast<DirectXResource*>(mtxUsedTexture.second->getResource());
        if (pDirectXResource == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }

        // Get SRV.
        const auto pSrvDescriptor = pDirectXResource->getDescriptor(DirectXDescriptorType::SRV);
        if (pSrvDescriptor == nullptr) [[unlikely]] {
            return Error("expected an SRV to be bound");
        }

        // Get SRV range.
        const auto pSrvRange = pSrvDescriptor->getDescriptorRange();
        if (pSrvRange == nullptr) [[unlikely]] {
            return Error("expected an SRV range to be valid");
        }

        // Clear currently used pipelines.
        mtxShaderConstantOffsetPerPipeline.second.clear();

        for (const auto& pPipeline : pipelinesToUse) {
            // Cast pipeline.
            const auto pDirectXPso = dynamic_cast<DirectXPso*>(pPipeline);
            if (pDirectXPso == nullptr) [[unlikely]] {
                return Error("expected a DirectX PSO");
            }

            // Add SRV range of our texture (because we already created it previously).
            const auto pMtxInternalResources = pDirectXPso->getInternalResources();
            std::scoped_lock guard(pMtxInternalResources->first);

            // Get root parameter index for our resource.
            auto rootParameterResult = pDirectXPso->getRootParameterIndex(getShaderResourceName());
            if (std::holds_alternative<Error>(rootParameterResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(rootParameterResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            const auto iRootParameterIndex = std::get<unsigned int>(rootParameterResult);

            // See if some binding already added our descriptor range.
            const auto rangeIt =
                pMtxInternalResources->second.descriptorRangesToBind.find(iRootParameterIndex);
            if (rangeIt == pMtxInternalResources->second.descriptorRangesToBind.end()) {
                // Add range to pipeline.
                pMtxInternalResources->second.descriptorRangesToBind[iRootParameterIndex] = pSrvRange;
            }

            // Get shader constant offset.
            auto result = pPipeline->getUintConstantOffset(getShaderResourceName());
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            const auto iShaderConstantOffset = std::get<size_t>(result);

            // Save.
            mtxShaderConstantOffsetPerPipeline.second[pDirectXPso] = iShaderConstantOffset;
        }

        return {};
    }

} // namespace ne
