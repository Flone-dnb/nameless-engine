#include "HlslGlobalShaderResourceBinding.h"

// Custom.
#include "render/general/resources/GpuResourceManager.h"
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

        // Make sure these resources have an SRV binded (because we only bind SRVs for now).
        for (const auto& pResource : vDirectXResourcesToBind) {
            if (pResource->getDescriptor(DirectXDescriptorType::SRV) == nullptr) [[unlikely]] {
                return Error(std::format(
                    "expected the resource \"{}\" to have a binded SRV because we only bind SRVs for now",
                    pResource->getResourceName()));
            }
        }

        // Prepare lambda to add binding to pipeline.
        const auto bindToPipeline = [&](Pipeline* pPipeline) -> std::optional<Error> {
            // Convert type.
            const auto pDirectXPso = dynamic_cast<DirectXPso*>(pPipeline);
            if (pDirectXPso == nullptr) [[unlikely]] {
                return Error("expected a DirectX PSO");
            }

            // Get pipeline resources.
            const auto pMtxPsoResources = pDirectXPso->getInternalResources();
            std::scoped_lock guard(pMtxPsoResources->first);

            // Find root parameter index.
            const auto rootIndexIt =
                pMtxPsoResources->second.rootParameterIndices.find(getShaderResourceName());
            if (rootIndexIt == pMtxPsoResources->second.rootParameterIndices.end()) { // DON'T make unlikely
                // OK, not used.
                return {};
            }

            // Add as SRV.
            pMtxPsoResources->second.globalShaderResourceSrvs[rootIndexIt->second] = vDirectXResourcesToBind;

            return {};
        };

        if (pSpecificPipeline != nullptr) {
            return bindToPipeline(pSpecificPipeline);
        }

        // Get pipeline manager.
        const auto pPipelineManager =
            vResourcesToBind[0]->getResourceManager()->getRenderer()->getPipelineManager();
        if (pPipelineManager == nullptr) [[unlikely]] {
            return Error("pipeline manager is `nullptr`");
        }

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
                    auto optionalError = bindToPipeline(pPipeline.get());
                    if (optionalError.has_value()) [[unlikely]] {
                        optionalError->addCurrentLocationToErrorStack();
                        return optionalError;
                    }
                }
            }
        }

        return {};
    }

}
