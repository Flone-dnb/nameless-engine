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

    std::variant<std::pair<std::shared_ptr<ContinuousDirectXDescriptorRange>, size_t>, Error>
    HlslShaderTextureResourceBinding::getSrvDescriptorRangeAndRootConstantIndex(
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

        // Make sure shader constants are used.
        if (!pMtxShaderConstants->second.has_value()) [[unlikely]] {
            return Error(std::format(
                "expected the pipeline \"{}\" to use shader constants to index into the shader resource "
                "\"{}\"",
                pPipeline->getPipelineIdentifier(),
                sShaderResourceName));
        }

        // Get shader constant index.
        const auto shaderConstantIndexIt =
            pMtxShaderConstants->second->uintConstantsOffsets.find(sShaderResourceName);
        if (shaderConstantIndexIt == pMtxShaderConstants->second->uintConstantsOffsets.end()) [[unlikely]] {
            return Error(std::format(
                "expected the pipeline \"{}\" to have a shader constant named \"{}\"",
                pPipeline->getPipelineIdentifier(),
                sShaderResourceName));
        }
        const auto iShaderConstantIndex = shaderConstantIndexIt->second;

        return std::pair<std::shared_ptr<ContinuousDirectXDescriptorRange>, size_t>{
            pSrvDescriptorRange, iShaderConstantIndex};
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

        std::unordered_map<DirectXPso*, std::pair<std::shared_ptr<ContinuousDirectXDescriptorRange>, size_t>>
            usedDescriptorRanges;

        for (const auto& pPipeline : pipelinesToUse) {
            // Convert type.
            const auto pDirectXPso = dynamic_cast<DirectXPso*>(pPipeline);
            if (pDirectXPso == nullptr) [[unlikely]] {
                return Error("expected a DirectX PSO");
            }

            // Get SRV descriptor range.
            auto result = getSrvDescriptorRangeAndRootConstantIndex(pDirectXPso, sShaderResourceName);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            const auto [pSrvDescriptorRange, iUintShaderConstantIndex] =
                std::get<std::pair<std::shared_ptr<ContinuousDirectXDescriptorRange>, size_t>>(result);

            // Bind SRV from the range to our texture.
            auto optionalError = pDirectXResource->bindDescriptor(
                DirectXDescriptorType::SRV, pSrvDescriptorRange, bBindSrvToCubemapFaces);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }

            // Save range.
            usedDescriptorRanges[pDirectXPso] =
                std::pair<std::shared_ptr<ContinuousDirectXDescriptorRange>, size_t>(
                    pSrvDescriptorRange, iUintShaderConstantIndex);
        }

        // Pass data to the binding.
        auto pResourceBinding =
            std::unique_ptr<HlslShaderTextureResourceBinding>(new HlslShaderTextureResourceBinding(
                sShaderResourceName, std::move(pTextureToUse), std::move(usedDescriptorRanges)));

        // At this point we can release the pipelines mutex.

        return pResourceBinding;
    }

    HlslShaderTextureResourceBinding::HlslShaderTextureResourceBinding(
        const std::string& sResourceName,
        std::unique_ptr<TextureHandle> pTextureToUse,
        std::unordered_map<
            DirectXPso*,
            std::pair<std::shared_ptr<ContinuousDirectXDescriptorRange>, size_t>>&& usedDescriptorRanges)
        : ShaderTextureResourceBinding(sResourceName) {
        // Save parameters.
        mtxUsedTexture.second = std::move(pTextureToUse);
        mtxUsedPipelineDescriptorRanges.second = std::move(usedDescriptorRanges);

        // Self check: make sure there is at least one pipeline.
        if (mtxUsedPipelineDescriptorRanges.second.empty()) [[unlikely]] {
            Error error("expected at least one pipeline to exist");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    std::optional<Error> HlslShaderTextureResourceBinding::onAfterAllPipelinesRefreshedResources() {
        std::scoped_lock guard(mtxUsedPipelineDescriptorRanges.first);

        // Collect used pipelines into a set.
        std::unordered_set<Pipeline*> pipelinesToUse;
        for (const auto& [pPipeline, pRange] : mtxUsedPipelineDescriptorRanges.second) {
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
        std::scoped_lock guard(mtxUsedTexture.first, mtxUsedPipelineDescriptorRanges.first);

        // Note: don't unbind SRV from the old texture because that texture can be used by someone else.
        // (when the old texture will be destroyed it will automatically free its used descriptors).

        // Replace used texture.
        mtxUsedTexture.second = std::move(pTextureToUse);

        // Convert to DirectX resource.
        const auto pDirectXResource = dynamic_cast<DirectXResource*>(mtxUsedTexture.second->getResource());
        if (pDirectXResource == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }

        for (const auto& [pPipeline, rootIndexAndRange] : mtxUsedPipelineDescriptorRanges.second) {
            // Bind an SRV to the new texture.
            auto optionalError = pDirectXResource->bindDescriptor(
                DirectXDescriptorType::SRV, rootIndexAndRange.first, bBindSrvToCubemapFaces);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }
        }

        return {};
    }

    std::optional<Error> HlslShaderTextureResourceBinding::changeUsedPipelines(
        const std::unordered_set<Pipeline*>& pipelinesToUse) {
        std::scoped_lock guard(mtxUsedTexture.first, mtxUsedPipelineDescriptorRanges.first);

        // Make sure at least one pipeline is specified.
        if (pipelinesToUse.empty()) [[unlikely]] {
            return Error("expected at least one pipeline to be specified");
        }

        // Convert to DirectX resource.
        const auto pDirectXResource = dynamic_cast<DirectXResource*>(mtxUsedTexture.second->getResource());
        if (pDirectXResource == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }

        // Clear currently used pipelines.
        mtxUsedPipelineDescriptorRanges.second.clear();

        for (const auto& pPipeline : pipelinesToUse) {
            // Convert type.
            const auto pDirectXPso = dynamic_cast<DirectXPso*>(pPipeline);
            if (pDirectXPso == nullptr) [[unlikely]] {
                return Error("expected a DirectX PSO");
            }

            // Get SRV descriptor range.
            auto result = getSrvDescriptorRangeAndRootConstantIndex(pDirectXPso, getShaderResourceName());
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            const auto [pSrvDescriptorRange, iUintShaderConstantIndex] =
                std::get<std::pair<std::shared_ptr<ContinuousDirectXDescriptorRange>, size_t>>(result);

            // Bind SRV from the range to our texture.
            auto optionalError = pDirectXResource->bindDescriptor(
                DirectXDescriptorType::SRV, pSrvDescriptorRange, bBindSrvToCubemapFaces);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }

            // Save range.
            mtxUsedPipelineDescriptorRanges.second[pDirectXPso] =
                std::pair<std::shared_ptr<ContinuousDirectXDescriptorRange>, size_t>(
                    pSrvDescriptorRange, iUintShaderConstantIndex);
        }

        return {};
    }

} // namespace ne
