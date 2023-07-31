#include "PipelineManager.h"

// Custom.
#include "io/Logger.h"
#include "render/Renderer.h"

namespace ne {
    PipelineManager::PipelineManager(Renderer* pRenderer) { this->pRenderer = pRenderer; }

    DelayedPipelineResourcesCreation
    PipelineManager::clearGraphicsPipelinesInternalResourcesAndDelayRestoring() {
        return DelayedPipelineResourcesCreation(this);
    }

    std::variant<PipelineSharedPtr, Error> PipelineManager::getGraphicsPipelineForMaterial(
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUsePixelBlending,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        const std::set<ShaderMacro>& additionalPixelShaderMacros,
        Material* pMaterial) {
        const size_t iIndex = bUsePixelBlending ? static_cast<size_t>(PipelineType::PT_TRANSPARENT)
                                                : static_cast<size_t>(PipelineType::PT_OPAQUE);

        std::scoped_lock guard(vGraphicsPipelines[iIndex].first);

        const auto it = vGraphicsPipelines[iIndex].second.find(Pipeline::constructUniquePipelineIdentifier(
            sVertexShaderName, sPixelShaderName, bUsePixelBlending));

        if (it == vGraphicsPipelines[iIndex].second.end()) {
            return createGraphicsPipelineForMaterial(
                sVertexShaderName,
                sPixelShaderName,
                bUsePixelBlending,
                additionalVertexShaderMacros,
                additionalPixelShaderMacros,
                pMaterial);
        }

        return PipelineSharedPtr(it->second, pMaterial);
    }

    std::array<
        std::pair<std::recursive_mutex, std::unordered_map<std::string, std::shared_ptr<Pipeline>>>,
        static_cast<size_t>(PipelineType::SIZE)>*
    PipelineManager::getGraphicsPipelines() {
        return &vGraphicsPipelines;
    }

    PipelineManager::~PipelineManager() {
        // Make sure all graphics pipelines were destroyed.
        const auto iCreatedGraphicsPipelineCount = getCreatedGraphicsPipelineCount();
        if (iCreatedGraphicsPipelineCount != 0) [[unlikely]] {
            Logger::get().error(fmt::format(
                "Pipeline manager is being destroyed but there are still {} graphics pipeline(s) exist",
                iCreatedGraphicsPipelineCount));
        }

        // Make sure all compute pipelines were destroyed.
        const auto iCreatedComputePipelineCount = getCreatedComputePipelineCount();
        if (iCreatedComputePipelineCount != 0) [[unlikely]] {
            Logger::get().error(fmt::format(
                "Pipeline manager is being destroyed but there are still {} compute pipeline(s) exist",
                iCreatedComputePipelineCount));
        }
    }

    std::variant<PipelineSharedPtr, Error> PipelineManager::createGraphicsPipelineForMaterial(
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUsePixelBlending,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        const std::set<ShaderMacro>& additionalPixelShaderMacros,
        Material* pMaterial) {
        // Create pipeline.
        auto result = Pipeline::createGraphicsPipeline(
            pRenderer,
            this,
            sVertexShaderName,
            sPixelShaderName,
            bUsePixelBlending,
            additionalVertexShaderMacros,
            additionalPixelShaderMacros);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        auto pPipeline = std::get<std::shared_ptr<Pipeline>>(std::move(result));

        // Add to array of created pipelines.
        const size_t iIndex = bUsePixelBlending ? static_cast<size_t>(PipelineType::PT_TRANSPARENT)
                                                : static_cast<size_t>(PipelineType::PT_OPAQUE);

        const auto sUniquePipelineIdentifier = pPipeline->getUniquePipelineIdentifier();
        std::scoped_lock guard(vGraphicsPipelines[iIndex].first);

        const auto it = vGraphicsPipelines[iIndex].second.find(sUniquePipelineIdentifier);
        if (it != vGraphicsPipelines[iIndex].second.end()) [[unlikely]] {
            Logger::get().error(fmt::format(
                "created a pipeline with combined shader name \"{}\" but another pipeline already existed "
                "with this combined shader name in the array of created pipelines",
                sUniquePipelineIdentifier));
        }

        vGraphicsPipelines[iIndex].second[sUniquePipelineIdentifier] = pPipeline;

        return PipelineSharedPtr(pPipeline, pMaterial);
    }

    size_t PipelineManager::getCreatedGraphicsPipelineCount() {
        size_t iTotalCount = 0;

        for (auto& [mutex, map] : vGraphicsPipelines) {
            std::scoped_lock guard(mutex);

            iTotalCount += map.size();
        }

        return iTotalCount;
    }

    size_t PipelineManager::getCreatedComputePipelineCount() {
        std::scoped_lock guard(mtxComputePipelines.first);
        return mtxComputePipelines.second.size();
    }

    Renderer* PipelineManager::getRenderer() const { return pRenderer; }

    std::optional<Error> PipelineManager::releaseInternalGraphicsPipelinesResources() {
        for (auto& [mutex, map] : vGraphicsPipelines) {
            mutex.lock(); // lock until resources where not restored

            for (const auto& [sUniqueId, pGraphicsPipeline] : map) {
                auto optionalError = pGraphicsPipeline->releaseInternalResources();
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
            }
        }

        return {};
    }

    std::optional<Error> PipelineManager::restoreInternalGraphicsPipelinesResources() {
        for (auto& [mutex, map] : vGraphicsPipelines) {
            {
                std::scoped_lock guard(mutex);

                for (const auto& [sUniqueId, pGraphicsPso] : map) {
                    auto optionalError = pGraphicsPso->restoreInternalResources();
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        return error;
                    }
                }
            }

            mutex.unlock(); // unlock because resources were restored
        }

        return {};
    }

    void PipelineManager::onPipelineNoLongerUsedByMaterial(const std::string& sUniquePipelineIdentifier) {
        for (auto& [mutex, map] : vGraphicsPipelines) {
            std::scoped_lock guard(mutex);

            // Find this pipeline.
            const auto it = map.find(sUniquePipelineIdentifier);
            if (it == map.end()) {
                continue;
            }

            // Check if it's used by some ShaderUser.
            if (it->second.use_count() > 1) {
                // Still used by some ShaderUser.
                return;
            }

            map.erase(it);
            return;
        }

        Logger::get().error(
            fmt::format("unable to find the specified pipeline \"{}\"", sUniquePipelineIdentifier));
    }

    void DelayedPipelineResourcesCreation::initialize() {
        const auto pRenderer = pPipelineManager->getRenderer();

        // Make sure no drawing is happening and the GPU is not referencing any resources.
        std::scoped_lock guard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Release resources.
        auto optionalError = pPipelineManager->releaseInternalGraphicsPipelinesResources();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    void DelayedPipelineResourcesCreation::destroy() {
        if (!bIsValid) {
            return;
        }

        // Restore resources.
        auto optionalError = pPipelineManager->restoreInternalGraphicsPipelinesResources();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

} // namespace ne
