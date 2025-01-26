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
#include "shader/hlsl/TextureSamplerDescriptions.hpp"

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

        // Check if a descriptor table (range) for our shader resource is already created in the pipeline.
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

        // Additionally, check if array of texture samplers is already bound to the pipeline.
        const auto samplerArrayIndexIt = pMtxPipelineResources->second.rootParameterIndices.find(
            getTextureSamplersShaderArrayName().data());
        if (samplerArrayIndexIt != pMtxPipelineResources->second.rootParameterIndices.end()) {
            const auto iSamplerHeapRootIndex = samplerArrayIndexIt->second;

            // Make sure sampler heap is already bound to this pipeline.
            const auto samplerHeapRangeIt =
                pMtxPipelineResources->second.descriptorRangesToBind.find(iSamplerHeapRootIndex);
            if (samplerHeapRangeIt == pMtxPipelineResources->second.descriptorRangesToBind.end()) {
                // Bind sampler heap to this pipeline.
                pMtxPipelineResources->second.samplerHeapRootIndexToBind = iSamplerHeapRootIndex;
            } // else: already bound, nothing to do
        }

        return pSrvDescriptorRange;
    }

    unsigned int HlslShaderTextureResourceBinding::getTextureSamplerOffset(
        DirectXResource* pTexture, Renderer* pRenderer) {
        const auto textureFilteringPreference = pTexture->getTextureFilteringPreference();

        if (textureFilteringPreference == TextureFilteringPreference::FROM_RENDER_SETTINGS) {
            const auto mtxRenderSettings = pRenderer->getRenderSettings();
            std::scoped_lock guard(*mtxRenderSettings.first);

            switch (mtxRenderSettings.second->getTextureFilteringQuality()) {
            case (TextureFilteringQuality::LOW): {
                return static_cast<unsigned int>(TextureFilteringPreference::POINT_FILTERING);
                break;
            }
            case (TextureFilteringQuality::MEDIUM): {
                return static_cast<unsigned int>(TextureFilteringPreference::LINEAR_FILTERING);
                break;
            }
            case (TextureFilteringQuality::HIGH): {
                return static_cast<unsigned int>(TextureFilteringPreference::ANISOTROPIC_FILTERING);
                break;
            }
            default:
                Error error("unhandled case");
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
        } else {
            return static_cast<unsigned int>(textureFilteringPreference);
        }
    }

    std::variant<std::unique_ptr<ShaderTextureResourceBinding>, Error>
    HlslShaderTextureResourceBinding::create(
        const std::string& sShaderResourceName,
        const std::unordered_set<Pipeline*>& pipelinesToUse,
        std::unique_ptr<TextureHandle> pTexture) {
        // Make sure at least one pipeline is specified.
        if (pipelinesToUse.empty()) [[unlikely]] {
            return Error("expected at least one pipeline to be specified");
        }
        const auto pRenderer = (*pipelinesToUse.begin())->getRenderer();

        // Cast type.
        const auto pDirectXTexture = dynamic_cast<DirectXResource*>(pTexture->getResource());
        if (pDirectXTexture == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }

        // Make sure no pipeline will re-create its internal resources because we will create raw pointers
        // to pipelines' internal resources.
        // After we create a new shader resource binding object we can release the mutex
        // since shader resource bindings are notified after pipelines re-create their internal resources.
        const auto pMtxGraphicsPipelines = pRenderer->getPipelineManager()->getGraphicsPipelines();
        std::scoped_lock pipelinesGuard(pMtxGraphicsPipelines->first);

        std::unordered_map<DirectXPso*, ShaderConstantOffsets> shaderConstantOffsetPerPipeline;

        for (const auto& pPipeline : pipelinesToUse) {
            // Convert type.
            const auto pDirectXPso = dynamic_cast<DirectXPso*>(pPipeline);
            if (pDirectXPso == nullptr) [[unlikely]] {
                return Error("expected a DirectX PSO");
            }

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
            auto optionalError = pDirectXTexture->bindDescriptor(
                DirectXDescriptorType::SRV, pSrvDescriptorRange, bBindSrvToCubemapFaces);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }

            // Get shader constant offset.
            auto offsetResult = pPipeline->getUintConstantOffset(sShaderResourceName);
            if (std::holds_alternative<Error>(offsetResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(offsetResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            const auto iTextureShaderConstantOffset = std::get<size_t>(offsetResult);

            // Also get offset for texture samplers array.
            offsetResult =
                pPipeline->getUintConstantOffset(sIndexIntoTextureSamplersArrayShaderConstantName.data());
            if (std::holds_alternative<Error>(offsetResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(offsetResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            const auto iSamplersArrayShaderConstantOffset = std::get<size_t>(offsetResult);

            // Save.
            shaderConstantOffsetPerPipeline[pDirectXPso] = {
                .iTextureShaderConstantIndex = iTextureShaderConstantOffset,
                .iSamplerArrayOffsetConstantIndex = iSamplersArrayShaderConstantOffset};
        }

        // Get sampler offset.
        const auto iTextureSamplerOffset = getTextureSamplerOffset(pDirectXTexture, pRenderer);

        // Pass data to the binding.
        auto pResourceBinding =
            std::unique_ptr<HlslShaderTextureResourceBinding>(new HlslShaderTextureResourceBinding(
                sShaderResourceName,
                TextureInfo{.pTexture = std::move(pTexture), .iTextureSamplerOffset = iTextureSamplerOffset},
                std::move(shaderConstantOffsetPerPipeline),
                pRenderer));

        // At this point we can release the pipelines mutex.

        return pResourceBinding;
    }

    HlslShaderTextureResourceBinding::HlslShaderTextureResourceBinding(
        const std::string& sResourceName,
        TextureInfo textureInfo,
        std::unordered_map<DirectXPso*, ShaderConstantOffsets>&& shaderConstantOffsetPerPipeline,
        Renderer* pRenderer)
        : ShaderTextureResourceBinding(sResourceName), pRenderer(pRenderer) {
        // Save parameters.
        mtxUsedTexture.second = std::move(textureInfo);
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
                dynamic_cast<DirectXResource*>(mtxUsedTexture.second.pTexture->getResource());
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

        // Replace old texture.
        mtxUsedTexture.second.pTexture = std::move(pTextureToUse);

        // Cast type.
        const auto pDirectXTexture =
            dynamic_cast<DirectXResource*>(mtxUsedTexture.second.pTexture->getResource());
        if (pDirectXTexture == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }

        // Bind an SRV to the new texture.
        auto optionalError = pDirectXTexture->bindDescriptor(
            DirectXDescriptorType::SRV, pUsedSrvRange, bBindSrvToCubemapFaces);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        // Save sampler offset.
        mtxUsedTexture.second.iTextureSamplerOffset = getTextureSamplerOffset(pDirectXTexture, pRenderer);

        return {};
    }

    std::optional<Error> HlslShaderTextureResourceBinding::changeUsedPipelines(
        const std::unordered_set<Pipeline*>& pipelinesToUse) {
        std::scoped_lock guard(mtxUsedTexture.first, mtxShaderConstantOffsetPerPipeline.first);

        // Make sure at least one pipeline is specified.
        if (pipelinesToUse.empty()) [[unlikely]] {
            return Error("expected at least one pipeline to be specified");
        }

        // Cast type.
        const auto pDirectXTexture =
            dynamic_cast<DirectXResource*>(mtxUsedTexture.second.pTexture->getResource());
        if (pDirectXTexture == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }

        // Re-save sampler offset because in case the texture filtering preference is set to use render
        // settings and texture filtering quality was changed (in this case we will get in this function).
        mtxUsedTexture.second.iTextureSamplerOffset = getTextureSamplerOffset(pDirectXTexture, pRenderer);

        // Get SRV.
        const auto pSrvDescriptor = pDirectXTexture->getDescriptor(DirectXDescriptorType::SRV);
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
            const auto iTextureShaderConstantOffset = std::get<size_t>(result);

            // Also get offset for texture samplers array.
            result =
                pPipeline->getUintConstantOffset(sIndexIntoTextureSamplersArrayShaderConstantName.data());
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            const auto iSamplersArrayShaderConstantOffset = std::get<size_t>(result);

            // Save.
            mtxShaderConstantOffsetPerPipeline.second[pDirectXPso] = {
                .iTextureShaderConstantIndex = iTextureShaderConstantOffset,
                .iSamplerArrayOffsetConstantIndex = iSamplersArrayShaderConstantOffset};
        }

        return {};
    }

} // namespace ne
