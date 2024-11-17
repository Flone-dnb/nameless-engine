#include "HlslGlobalShaderResourceBinding.h"

// Custom.
#include "render/general/resource/GpuResourceManager.h"
#include "render/general/pipeline/PipelineManager.h"
#include "misc/Profiler.hpp"
#include "render/Renderer.h"
#include "render/directx/pipeline/DirectXPso.h"

namespace ne {

    HlslGlobalShaderResourceBinding::HlslGlobalShaderResourceBinding(
        GlobalShaderResourceBindingManager* pManager,
        const std::string& sShaderResourceName,
        const std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()>& vResourcesToBind)
        : GlobalShaderResourceBinding(pManager, sShaderResourceName, vResourcesToBind) {}

    HlslGlobalShaderResourceBinding::~HlslGlobalShaderResourceBinding() { unregisterBinding(); }

    std::optional<Error> HlslGlobalShaderResourceBinding::bindToPipelines(Pipeline* pSpecificPipeline) {
        PROFILE_FUNC;

        // Convert resource types.
        const auto vResourcesToBind = getBindedResources();
        std::array<DirectXResource*, FrameResourceManager::getFrameResourceCount()> vDirectXResourcesToBind;
        for (size_t i = 0; i < vDirectXResourcesToBind.size(); i++) {
            const auto pDirectXResource = dynamic_cast<DirectXResource*>(vResourcesToBind[i]);
            if (pDirectXResource == nullptr) [[unlikely]] {
                return Error("expected a DirectX resource");
            }
            vDirectXResourcesToBind[i] = pDirectXResource;
        }

        // Prepare a lambda to bind the resources.
        std::function<std::optional<Error>(Pipeline * pPipeline)> onBind =
            [](Pipeline* pPipeline) -> std::optional<Error> { return Error("not implemented"); };

        // Check descriptor type.
        if (vDirectXResourcesToBind[0]->getDescriptor(DirectXDescriptorType::SRV) != nullptr) {
            // Make sure all have an SRV bound.
            for (const auto& pResource : vDirectXResourcesToBind) {
                if (pResource->getDescriptor(DirectXDescriptorType::SRV) == nullptr) [[unlikely]] {
                    return Error(std::format(
                        "expected all specified resource to have an SRV bound because the first resource has "
                        "an SRV",
                        pResource->getResourceName()));
                }
            }

            // Set binding lambda.
            onBind = [&](Pipeline* pPipeline) -> std::optional<Error> {
                return bindResourcesToPipeline(
                    vDirectXResourcesToBind, DirectXDescriptorType::SRV, pPipeline, getShaderResourceName());
            };
        } else if (vDirectXResourcesToBind[0]->getDescriptor(DirectXDescriptorType::CBV) != nullptr) {
            // Make sure all have a CBV bound.
            for (const auto& pResource : vDirectXResourcesToBind) {
                if (pResource->getDescriptor(DirectXDescriptorType::CBV) == nullptr) [[unlikely]] {
                    return Error(std::format(
                        "expected all specified resource to have a CBV bound because the first resource has "
                        "a CBV",
                        pResource->getResourceName()));
                }
            }

            // Set binding lambda.
            onBind = [&](Pipeline* pPipeline) -> std::optional<Error> {
                return bindResourcesToPipeline(
                    vDirectXResourcesToBind, DirectXDescriptorType::CBV, pPipeline, getShaderResourceName());
            };
        } else {
            return Error("supported descriptor type was not found (either you forgot to bind a descriptor to "
                         "your global resource or it's not supported");
        }

        if (pSpecificPipeline != nullptr) {
            // Bind to the specified pipeline.
            return onBind(pSpecificPipeline);
        } else {
            // Get pipeline manager.
            const auto pPipelineManager =
                vResourcesToBind[0]->getResourceManager()->getRenderer()->getPipelineManager();
            if (pPipelineManager == nullptr) [[unlikely]] {
                return Error("pipeline manager is `nullptr`");
            }

            // Bind to all graphics pipelines.
            auto optionalError = bindResourceToGraphicsPipelines(pPipelineManager, onBind);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        return {};
    }

    std::optional<Error> HlslGlobalShaderResourceBinding::bindResourceToGraphicsPipelines(
        PipelineManager* pPipelineManager, const std::function<std::optional<Error>(Pipeline*)>& onBind) {
        // Get all pipelines.
        const auto pMtxPipelines = pPipelineManager->getGraphicsPipelines();
        std::scoped_lock pipelinesGuard(pMtxPipelines->first);

        // Iterate over graphics pipelines of all types.
        for (auto& pipelinesOfSpecificType : pMtxPipelines->second.vPipelineTypes) {

            // Iterate over all active shader combinations.
            for (const auto& [sShaderNames, pipelines] : pipelinesOfSpecificType) {

                // Iterate over all active unique material macros combinations.
                for (const auto& [materialMacros, pPipeline] : pipelines.shaderPipelines) {
                    // Bind.
                    auto optionalError = onBind(pPipeline.get());
                    if (optionalError.has_value()) [[unlikely]] {
                        optionalError->addCurrentLocationToErrorStack();
                        return optionalError;
                    }
                }
            }
        }

        return {};
    }

    std::optional<Error> HlslGlobalShaderResourceBinding::bindResourcesToPipeline(
        const std::array<DirectXResource*, FrameResourceManager::getFrameResourceCount()>& vResourcesToBind,
        DirectXDescriptorType bindingType,
        Pipeline* pPipeline,
        const std::string& sShaderResourceName) {
        // Convert type.
        const auto pDirectXPso = dynamic_cast<DirectXPso*>(pPipeline);
        if (pDirectXPso == nullptr) [[unlikely]] {
            return Error("expected a DirectX PSO");
        }

        // Get pipeline resources.
        const auto pMtxPsoResources = pDirectXPso->getInternalResources();
        std::scoped_lock guard(pMtxPsoResources->first);

        // Find root parameter index.
        const auto rootIndexIt = pMtxPsoResources->second.rootParameterIndices.find(sShaderResourceName);
        if (rootIndexIt == pMtxPsoResources->second.rootParameterIndices.end()) { // DON'T make unlikely
            // OK, not used.
            return {};
        }

        if (bindingType == DirectXDescriptorType::CBV) {
            pMtxPsoResources->second.globalShaderResourceCbvs[rootIndexIt->second] = vResourcesToBind;
        } else if (bindingType == DirectXDescriptorType::SRV) {
            pMtxPsoResources->second.globalShaderResourceSrvs[rootIndexIt->second] = vResourcesToBind;
        } else [[unlikely]] {
            return Error("unsupported binding type");
        }

        return {};
    }

}
