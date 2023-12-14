#pragma once

// Standard.
#include <memory>
#include <variant>
#include <optional>
#include <mutex>
#include <functional>
#include <unordered_map>

// Custom.
#include "misc/Error.h"
#include "ShadowMapType.hpp"

namespace ne {
    class GpuResource;
    class GpuResourceManager;
    class ShadowMapHandle;

    /**
     * Stores all shadow maps.
     *
     * @remark Although shadow maps are textures they are not managed by the texture manager because shadow
     * maps are special (specific) textures and they need special treatment/management logic that texture
     * manager should not care about.
     */
    class ShadowMapManager {
        // Unique pointers will notify the manager before destruction.
        friend class ShadowMapHandle;

        // Renderer will notify us when shadow quality setting is changed.
        friend class Renderer;

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

        /**
         * Creates a shadow map.
         *
         * @param sResourceName Name of the GPU resource that will be created, used for logging.
         * @param type          Type of a shadow map to create depending on the light source type.
         *
         * @return Error if something went wrong, otherwise created shadow map. Returning unique ptr
         * although shadow map handle already behaves like unique ptr in order for the manager to be
         * able to store raw pointers to handles without fearing that a raw pointer will point to invalid
         * handle due to `move`ing the handle.
         */
        std::variant<std::unique_ptr<ShadowMapHandle>, Error>
        createShadowMap(const std::string& sResourceName, ShadowMapType type);

    private:
        /**
         * Called by shadow map unique pointers to notify the manager that a resource is no longer used.
         *
         * @param pHandleToResourceDestroy Handle to resource to destroy.
         */
        void destroyResource(ShadowMapHandle* pHandleToResourceDestroy);

        /**
         * Called by the renderer to notify the manager that shadow quality setting was changed and all
         * shadow maps should now be re-created using the new shadow map resolution.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> recreateShadowMaps();

        /**
         * Returns shadow map texture size (in pixels) that should be used for the specified shadow map type.
         * (the specified value might be corrected for the specified shadow map type).
         *
         * @param iRenderSettingsShadowMapSize Shadow map size from render settings.
         * @param type                         Shadow map size.
         *
         * @return Shadow map size to use for this shadow map type.
         */
        unsigned int
        correctShadowMapResolutionForType(unsigned int iRenderSettingsShadowMapSize, ShadowMapType type);

        /**
         * Allocated shadow maps.
         *
         * @remark Storing pairs of "raw pointer" - "unique pointer" to quickly find needed resources
         * when need to destroy some resource given a raw pointer.
         *
         * @remark Storing raw pointers here is safe because shadow map handle will notify us
         * before destroying the resource so we will remove the raw pointer and shadow map handle
         */
        std::pair<std::recursive_mutex, std::unordered_map<ShadowMapHandle*, std::unique_ptr<GpuResource>>>
            mtxShadowMaps;

        /** Do not delete (free) this pointer. GPU resource manager that owns this object. */
        GpuResourceManager* pResourceManager = nullptr;
    };
}
