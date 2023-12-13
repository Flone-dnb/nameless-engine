#include "ShadowMapManager.h"

// Standard.
#include <format>

// Custom.
#include "io/Logger.h"
#include "render/general/resources/GpuResource.h"

namespace ne {

    ShadowMapManager::ShadowMapManager(GpuResourceManager* pResourceManager) {
        this->pResourceManager = pResourceManager;
    }

    void ShadowMapManager::destroyResource(GpuResource* pResourceToDestroy) {
        std::scoped_lock guard(mtxShadowMaps.first);

        // Find this resource.
        const auto it = mtxShadowMaps.second.find(pResourceToDestroy);
        if (it == mtxShadowMaps.second.end()) [[unlikely]] {
            // Maybe the specified resource pointer is invalid.
            Logger::get().error("failed to find the specified shader texture resource to be destroyed");
            return;
        }

        // Destroy resource.
        mtxShadowMaps.second.erase(it);
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
