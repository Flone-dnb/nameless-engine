#include "PsoManager.h"

// Custom.
#include "io/Logger.h"
#include "render/Renderer.h"

namespace ne {
    PsoManager::PsoManager(Renderer* pRenderer) { this->pRenderer = pRenderer; }

    DelayedPsoResourcesCreation PsoManager::clearGraphicsPsosInternalResourcesAndDelayRestoring() {
        return DelayedPsoResourcesCreation(this);
    }

    std::variant<PsoSharedPtr, Error> PsoManager::getGraphicsPsoForMaterial(
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUsePixelBlending,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        const std::set<ShaderMacro>& additionalPixelShaderMacros,
        Material* pMaterial) {
        const size_t iIndex = bUsePixelBlending ? static_cast<size_t>(PsoType::PT_TRANSPARENT)
                                                : static_cast<size_t>(PsoType::PT_OPAQUE);

        std::scoped_lock guard(vGraphicsPsos[iIndex].first);

        const auto it = vGraphicsPsos[iIndex].second.find(
            Pso::constructUniquePsoIdentifier(sVertexShaderName, sPixelShaderName, bUsePixelBlending));

        if (it == vGraphicsPsos[iIndex].second.end()) {
            return createGraphicsPsoForMaterial(
                sVertexShaderName,
                sPixelShaderName,
                bUsePixelBlending,
                additionalVertexShaderMacros,
                additionalPixelShaderMacros,
                pMaterial);
        }

        return PsoSharedPtr(it->second, pMaterial);
    }

    std::array<
        std::pair<std::recursive_mutex, std::unordered_map<std::string, std::shared_ptr<Pso>>>,
        static_cast<size_t>(PsoType::SIZE)>*
    PsoManager::getGraphicsPsos() {
        return &vGraphicsPsos;
    }

    PsoManager::~PsoManager() {
        // Make sure all graphics PSOs were destroyed.
        const auto iCreatedGraphicsPsoCount = getCreatedGraphicsPsoCount();
        if (iCreatedGraphicsPsoCount != 0) [[unlikely]] {
            Logger::get().error(fmt::format(
                "PSO manager is being destroyed but there are still {} graphics PSO(s) exist",
                iCreatedGraphicsPsoCount));
        }

        // Make sure all compute PSOs were destroyed.
        const auto iCreatedComputePsoCount = getCreatedComputePsoCount();
        if (iCreatedComputePsoCount != 0) [[unlikely]] {
            Logger::get().error(fmt::format(
                "PSO manager is being destroyed but there are still {} compute PSO(s) exist",
                iCreatedComputePsoCount));
        }
    }

    std::variant<PsoSharedPtr, Error> PsoManager::createGraphicsPsoForMaterial(
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUsePixelBlending,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        const std::set<ShaderMacro>& additionalPixelShaderMacros,
        Material* pMaterial) {
        // Create PSO.
        auto result = Pso::createGraphicsPso(
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

        auto pPso = std::get<std::shared_ptr<Pso>>(std::move(result));

        // Add to array of created PSOs.
        const size_t iIndex = bUsePixelBlending ? static_cast<size_t>(PsoType::PT_TRANSPARENT)
                                                : static_cast<size_t>(PsoType::PT_OPAQUE);

        const auto sUniquePsoIdentifier = pPso->getUniquePsoIdentifier();
        std::scoped_lock guard(vGraphicsPsos[iIndex].first);

        const auto it = vGraphicsPsos[iIndex].second.find(sUniquePsoIdentifier);
        if (it != vGraphicsPsos[iIndex].second.end()) [[unlikely]] {
            Logger::get().error(fmt::format(
                "created a PSO with combined shader name \"{}\" but another PSO already existed with "
                "this combined shader name in the array of created PSO",
                sUniquePsoIdentifier));
        }

        vGraphicsPsos[iIndex].second[sUniquePsoIdentifier] = pPso;

        return PsoSharedPtr(pPso, pMaterial);
    }

    size_t PsoManager::getCreatedGraphicsPsoCount() {
        size_t iTotalCount = 0;

        for (auto& [mutex, map] : vGraphicsPsos) {
            std::scoped_lock guard(mutex);

            iTotalCount += map.size();
        }

        return iTotalCount;
    }

    size_t PsoManager::getCreatedComputePsoCount() {
        std::scoped_lock guard(mtxComputePsos.first);
        return mtxComputePsos.second.size();
    }

    Renderer* PsoManager::getRenderer() const { return pRenderer; }

    std::optional<Error> PsoManager::releaseInternalGraphicsPsosResources() {
        for (auto& [mutex, map] : vGraphicsPsos) {
            mutex.lock(); // lock until resources where not restored

            for (const auto& [sUniqueId, pGraphicsPso] : map) {
                auto optionalError = pGraphicsPso->releaseInternalResources();
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
            }
        }

        return {};
    }

    std::optional<Error> PsoManager::restoreInternalGraphicsPsosResources() {
        for (auto& [mutex, map] : vGraphicsPsos) {
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

    void PsoManager::onPsoNoLongerUsedByMaterial(const std::string& sUniquePsoIdentifier) {
        // Find this PSO.
        for (auto& [mutex, map] : vGraphicsPsos) {
            std::scoped_lock guard(mutex);

            const auto it = map.find(sUniquePsoIdentifier);
            if (it == map.end()) {
                continue;
            }

            if (it->second.use_count() > 1) {
                // Still used by somebody else.
                return;
            }

            map.erase(it);
            break;
        }
    }

    void DelayedPsoResourcesCreation::initialize() {
        const auto pRenderer = pPsoManager->getRenderer();

        // Make sure no drawing is happening and the GPU is not referencing any resources.
        std::scoped_lock guard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Release resources.
        auto optionalError = pPsoManager->releaseInternalGraphicsPsosResources();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    void DelayedPsoResourcesCreation::destroy() {
        if (!bIsValid) {
            return;
        }

        // Restore resources.
        auto optionalError = pPsoManager->restoreInternalGraphicsPsosResources();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

} // namespace ne
