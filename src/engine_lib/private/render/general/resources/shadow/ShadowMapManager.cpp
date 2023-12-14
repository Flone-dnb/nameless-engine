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

    ShadowMapManager::ShadowMapManager(GpuResourceManager* pResourceManager) {
        this->pResourceManager = pResourceManager;
    }

    std::variant<std::unique_ptr<ShadowMapHandle>, Error>
    ShadowMapManager::createShadowMap(const std::string& sResourceName, ShadowMapType type) {
        // Get render settings.
        const auto pRenderer = pResourceManager->getRenderer();
        const auto pMtxRenderSettings = pRenderer->getRenderSettings();

        // Lock render settings and internal resources.
        std::scoped_lock guardSettings(pMtxRenderSettings->first, mtxShadowMaps.first);

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
        auto pShadowMapHandle =
            std::unique_ptr<ShadowMapHandle>(new ShadowMapHandle(this, pShadowMapResource.get(), type));

        // Add to the map of allocated shadow maps.
        mtxShadowMaps.second[pShadowMapHandle.get()] = std::move(pShadowMapResource);

        return pShadowMapHandle;
    }

    void ShadowMapManager::destroyResource(ShadowMapHandle* pHandleToResourceDestroy) {
        std::scoped_lock guard(mtxShadowMaps.first);

        // Find this resource.
        const auto it = mtxShadowMaps.second.find(pHandleToResourceDestroy);
        if (it == mtxShadowMaps.second.end()) [[unlikely]] {
            // Maybe the specified resource pointer is invalid.
            Logger::get().error("failed to find the specified shadow map resource to be destroyed");
            return;
        }

        // Destroy resource.
        mtxShadowMaps.second.erase(it);
    }

    std::optional<Error> ShadowMapManager::recreateShadowMaps() {
        // Get render settings.
        const auto pRenderer = pResourceManager->getRenderer();
        const auto pMtxRenderSettings = pRenderer->getRenderSettings();

        // Lock render settings and internal resources.
        std::scoped_lock guardSettings(pMtxRenderSettings->first, mtxShadowMaps.first);

        // Get shadow map resolution from render settings.
        unsigned int iRenderSettingsShadowMapSize =
            static_cast<unsigned int>(pMtxRenderSettings->second->getShadowQuality());

        for (auto& [pShadowMapHandle, pShadowMap] : mtxShadowMaps.second) {
            // Get shadow map type.
            const auto shadowMapType = pShadowMapHandle->getShadowMapType();

            // Get resource name.
            const auto sResourceName = pShadowMap->getResourceName();

            // Correct for the specified shadow map type.
            const auto iShadowMapSize = correctShadowMapResolutionForType(
                iRenderSettingsShadowMapSize, pShadowMapHandle->shadowMapType);

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

            // Update raw pointer in the handle.
            pShadowMapHandle->pResource = pShadowMap.get();
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

            iRenderSettingsShadowMapSize = static_cast<unsigned int>(iWorldSize);
        }

        return iRenderSettingsShadowMapSize;
    }

    ShadowMapManager::~ShadowMapManager() {
        std::scoped_lock guard(mtxShadowMaps.first);

        // Make sure there are no CPU write resources.
        if (!mtxShadowMaps.second.empty()) [[unlikely]] {
            // Prepare their names and count.
            std::unordered_map<std::string, size_t> leftResources;
            for (const auto& [pRawResource, pResource] : mtxShadowMaps.second) {
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
                "shadow map manager is being destroyed but there are still {} shadow map(s) alive:\n{}",
                mtxShadowMaps.second.size(),
                sLeftResources));
            error.showError();
            return; // don't throw in destructor, just quit
        }
    }

}
