#include "ShadowMapManager.h"

// Standard.
#include <format>

// Custom.
#include "io/Logger.h"
#include "render/general/resources/GpuResource.h"
#include "render/general/resources/GpuResourceManager.h"
#include "render/Renderer.h"
#include "game/GameManager.h"
#include "render/general/resources/shadow/ShadowMapHandle.h"

namespace ne {

    ShadowMapManager::ShadowMapManager(
        GpuResourceManager* pResourceManager,
        std::array<std::unique_ptr<ShadowMapArrayIndexManager>, static_cast<size_t>(ShadowMapType::SIZE)>
            vShadowMapArrayIndexManagers) {
        this->pResourceManager = pResourceManager;
        mtxInternalResources.second.vShadowMapArrayIndexManagers = std::move(vShadowMapArrayIndexManagers);
    }

    std::optional<Error> ShadowMapManager::bindShadowMapsToPipeline(Pipeline* pPipeline) {
        std::scoped_lock guard(mtxInternalResources.first);

        // Notify managers.
        for (auto& pManager : mtxInternalResources.second.vShadowMapArrayIndexManagers) {
            auto optionalError = pManager->bindShadowMapsToPipeline(pPipeline);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        return {};
    }

    std::optional<Error> ShadowMapManager::bindShadowMapsToAllPipelines() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Notify managers.
        for (auto& pManager : mtxInternalResources.second.vShadowMapArrayIndexManagers) {
            auto optionalError = pManager->bindShadowMapsToAllPipelines();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        return {};
    }

    std::variant<std::unique_ptr<ShadowMapHandle>, Error> ShadowMapManager::createShadowMap(
        const std::string& sResourceName,
        ShadowMapType type,
        const std::function<void(unsigned int)>& onArrayIndexChanged) {
        // Get render settings.
        const auto pRenderer = pResourceManager->getRenderer();
        const auto pMtxRenderSettings = pRenderer->getRenderSettings();

        // Lock render settings and internal resources.
        std::scoped_lock guardSettings(pMtxRenderSettings->first, mtxInternalResources.first);

        // Get shadow map resolution from render settings.
        unsigned int iShadowMapSize =
            static_cast<unsigned int>(pMtxRenderSettings->second->getShadowQuality());

        // Correct for the specified shadow map type.
        iShadowMapSize = correctShadowMapResolutionForType(iShadowMapSize, type);

        // Create shadow map.
        auto result = pResourceManager->createShadowMapTexture(
            sResourceName, iShadowMapSize, type == ShadowMapType::POINT);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto pShadowMapResource = std::get<std::unique_ptr<GpuResource>>(std::move(result));

        // Create handle.
        auto pShadowMapHandle = std::unique_ptr<ShadowMapHandle>(
            new ShadowMapHandle(this, pShadowMapResource.get(), type, iShadowMapSize, onArrayIndexChanged));

        // Get array index manager.
        const auto pShadowMapArrayIndexManager = getArrayIndexManagerBasedOnShadowMapType(type);
        if (pShadowMapArrayIndexManager == nullptr) [[unlikely]] {
            return Error("unsupported shadow map type");
        }

        // Assign an index for this new resource.
        auto optionalError = pShadowMapArrayIndexManager->registerShadowMapResource(pShadowMapHandle.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        // Add to the map of allocated shadow maps.
        mtxInternalResources.second.shadowMaps[pShadowMapHandle.get()] = std::move(pShadowMapResource);

        return pShadowMapHandle;
    }

    ShadowMapArrayIndexManager*
    ShadowMapManager::getArrayIndexManagerBasedOnShadowMapType(ShadowMapType type) {
        // Convert type to index.
        const auto iIndex = static_cast<size_t>(type);

        // Make sure we won't access out of bounds.
        if (iIndex >= mtxInternalResources.second.vShadowMapArrayIndexManagers.size()) [[unlikely]] {
            Error error("invalid type");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        return mtxInternalResources.second.vShadowMapArrayIndexManagers[iIndex].get();
    }

    void ShadowMapManager::onShadowMapHandleBeingDestroyed(ShadowMapHandle* pHandleToResourceDestroy) {
        std::scoped_lock guard(mtxInternalResources.first);

        // Find this resource.
        const auto it = mtxInternalResources.second.shadowMaps.find(pHandleToResourceDestroy);
        if (it == mtxInternalResources.second.shadowMaps.end()) [[unlikely]] {
            // Maybe the specified resource pointer is invalid.
            Error error(std::format(
                "failed to find the specified {} shadow map handle to destroy",
                shadowMapTypeToString(pHandleToResourceDestroy->shadowMapType)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get array index manager.
        const auto pShadowMapArrayIndexManager =
            getArrayIndexManagerBasedOnShadowMapType(pHandleToResourceDestroy->shadowMapType);
        if (pShadowMapArrayIndexManager == nullptr) [[unlikely]] {
            Error error("unsupported shadow map type");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Unregister shadow map resource.
        auto optionalError =
            pShadowMapArrayIndexManager->unregisterShadowMapResource(pHandleToResourceDestroy);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }

        // Destroy resource.
        mtxInternalResources.second.shadowMaps.erase(it);
    }

    std::optional<Error> ShadowMapManager::recreateShadowMaps() {
        // Get render settings.
        const auto pRenderer = pResourceManager->getRenderer();
        const auto pMtxRenderSettings = pRenderer->getRenderSettings();

        // Lock render settings, internal resources and rendering.
        std::scoped_lock guardSettings(
            pMtxRenderSettings->first, mtxInternalResources.first, *pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Get shadow map resolution from render settings.
        unsigned int iRenderSettingsShadowMapSize =
            static_cast<unsigned int>(pMtxRenderSettings->second->getShadowQuality());

        for (auto& [pShadowMapHandle, pShadowMap] : mtxInternalResources.second.shadowMaps) {
            // Get shadow map type.
            const auto shadowMapType = pShadowMapHandle->getShadowMapType();

            // Get resource name.
            const auto sResourceName = pShadowMap->getResourceName();

            // Correct for the specified shadow map type.
            const auto iShadowMapSize = correctShadowMapResolutionForType(
                iRenderSettingsShadowMapSize, pShadowMapHandle->shadowMapType);

            // Get array index manager.
            const auto pShadowMapArrayIndexManager =
                getArrayIndexManagerBasedOnShadowMapType(pShadowMapHandle->shadowMapType);
            if (pShadowMapArrayIndexManager == nullptr) [[unlikely]] {
                return Error("unsupported shadow map type");
            }

            std::scoped_lock handleResourceGuard(pShadowMapHandle->mtxResource.first);

            // Unregister this handle because the resource will now be deleted.
            auto optionalError = pShadowMapArrayIndexManager->unregisterShadowMapResource(pShadowMapHandle);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }

            // Destroy old resource.
            pShadowMap = nullptr;

            // Re-create the shadow map.
            auto result = pResourceManager->createShadowMapTexture(
                sResourceName, iShadowMapSize, shadowMapType == ShadowMapType::POINT);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            pShadowMap = std::get<std::unique_ptr<GpuResource>>(std::move(result));

            // Update handle.
            pShadowMapHandle->mtxResource.second = pShadowMap.get();
            pShadowMapHandle->iShadowMapSize = iShadowMapSize;

            // Register newly created resource to bind the new GPU resource to descriptor.
            optionalError = pShadowMapArrayIndexManager->registerShadowMapResource(pShadowMapHandle);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        return {};
    }

    unsigned int ShadowMapManager::correctShadowMapResolutionForType(
        unsigned int iRenderSettingsShadowMapSize, ShadowMapType type) {
        if (type == ShadowMapType::DIRECTIONAL) {
            // Overwrite shadow map size with world size.
            const auto iWorldSize = pResourceManager->getRenderer()->getGameManager()->getWorldSize();

            // Before casting to `unsigned int` make a check.
            if (iWorldSize > std::numeric_limits<unsigned int>::max()) [[unlikely]] {
                Error error(
                    std::format("world size ({}) exceeds type limit for shadow map size", iWorldSize));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            iRenderSettingsShadowMapSize =
                static_cast<unsigned int>(iWorldSize) *
                32; // NOLINT: due to no cascading shadow maps we have to use huge directional shadow maps
        }

        return iRenderSettingsShadowMapSize;
    }

    ShadowMapManager::~ShadowMapManager() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Make sure there are no CPU write resources.
        if (!mtxInternalResources.second.shadowMaps.empty()) [[unlikely]] {
            // Prepare their names and count.
            std::unordered_map<std::string, size_t> leftResources;
            for (const auto& [pRawResource, pResource] : mtxInternalResources.second.shadowMaps) {
                const auto it = leftResources.find(pResource->getResourceName());
                if (it == leftResources.end()) {
                    leftResources[pResource->getResourceName()] = 1;
                } else {
                    it->second += 1;
                }
            }

            // Prepare output message.
            std::string sLeftResources;
            for (const auto& [sResourceName, iLeftCount] : leftResources) {
                sLeftResources += std::format("- {}, left: {}", sResourceName, iLeftCount);
            }

            // Show error.
            Error error(std::format(
                "shadow map manager is being destroyed but there are still {} shadow map(s) "
                "alive:\n{}",
                mtxInternalResources.second.shadowMaps.size(),
                sLeftResources));
            error.showError();
            return; // don't throw in destructor, just quit
        }
    }

    std::variant<std::unique_ptr<ShadowMapManager>, Error>
    ShadowMapManager::create(GpuResourceManager* pResourceManager) {
        // Prepare final array of managers.
        std::array<std::unique_ptr<ShadowMapArrayIndexManager>, static_cast<size_t>(ShadowMapType::SIZE)>
            vShadowMapArrayIndexManagers;

        const auto pRenderer = pResourceManager->getRenderer();

        // Create directional manager.
        auto result = ShadowMapArrayIndexManager::create(
            pRenderer, pResourceManager, pDirectionalShadowMapsShaderResourceName);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        vShadowMapArrayIndexManagers[static_cast<size_t>(ShadowMapType::DIRECTIONAL)] =
            std::get<std::unique_ptr<ShadowMapArrayIndexManager>>(std::move(result));

        // Create spot manager.
        result = ShadowMapArrayIndexManager::create(
            pRenderer, pResourceManager, pSpotShadowMapsShaderResourceName);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        vShadowMapArrayIndexManagers[static_cast<size_t>(ShadowMapType::SPOT)] =
            std::get<std::unique_ptr<ShadowMapArrayIndexManager>>(std::move(result));

        // Create point manager.
        result = ShadowMapArrayIndexManager::create(
            pRenderer, pResourceManager, pPointShadowMapsShaderResourceName);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        vShadowMapArrayIndexManagers[static_cast<size_t>(ShadowMapType::POINT)] =
            std::get<std::unique_ptr<ShadowMapArrayIndexManager>>(std::move(result));

        return std::unique_ptr<ShadowMapManager>(
            new ShadowMapManager(pResourceManager, std::move(vShadowMapArrayIndexManagers)));
    }
}
