#pragma once

// Standard.
#include <memory>
#include <mutex>
#include <unordered_map>

namespace ne {
    class ShadowMap;
    class GpuResourceManager;

    /**
     * Stores all shadow maps.
     *
     * @remark Although shadow maps are textures they are not managed by the texture manager because shadow
     * maps are special (specific) textures and they need special treatment/management logic that texture
     * manager should not care about.
     */
    class ShadowMapManager {
        // Unique pointers will notify the manager before destruction.
        friend class ShadowMapUniquePtr;

    public:
        ShadowMapManager() = delete;
        ShadowMapManager(const ShadowMapManager&) = delete;
        ShadowMapManager& operator=(const ShadowMapManager&) = delete;

        /** Makes sure that no resource exists. */
        ~ShadowMapManager();

        /**
         * Creates a new shadow map manager.
         *
         * @param pResourceManager Resource manager that owns this object.
         */
        ShadowMapManager(GpuResourceManager* pResourceManager);

    private:
        /**
         * Called by shadow map unique pointers to notify the manager that a resource is no longer used.
         *
         * @param pResourceToDestroy Resource to destroy.
         */
        void destroyResource(ShadowMap* pResourceToDestroy);

        /**
         * Allocated shadow maps.
         *
         * @remark Storing pairs of "raw pointer" - "unique pointer" to quickly find needed resources
         * when need to destroy some resource given a raw pointer.
         *
         * @remark Storing raw pointers here is safe because `ShadowMapUniquePtr` will notify us
         * before destroying the resource so we will remove the raw pointer.
         */
        std::pair<std::recursive_mutex, std::unordered_map<ShadowMap*, std::unique_ptr<ShadowMap>>>
            mtxShadowMaps;

        /** Do not delete (free) this pointer. GPU resource manager that owns this object. */
        GpuResourceManager* pResourceManager = nullptr;
    };
}
