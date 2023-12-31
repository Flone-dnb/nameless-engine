#pragma once

// Standard.
#include <memory>
#include <variant>
#include <optional>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <array>

// Custom.
#include "misc/Error.h"
#include "render/general/resources/shadow/ShadowMapArrayIndexManager.h"
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

        // Pipeline manager notifies us about new pipelines or re-created pipelines to re-bind descriptors.
        friend class PipelineManager;

    public:
        ShadowMapManager() = delete;
        ShadowMapManager(const ShadowMapManager&) = delete;
        ShadowMapManager& operator=(const ShadowMapManager&) = delete;

        /** Makes sure that no resource exists. */
        ~ShadowMapManager();

        /**
         * Constant depth bias (offset) to apply when rendering depth to shadow maps to avoid an
         * effect known as "shadow acne" (stair-stepping).
         *
         * @return Constant depth bias.
         */
        static constexpr int getShadowMappingDepthBias() { return iShadowMappingDepthBias; }

        /**
         * Returns constant used to convert visible (non-clipped) distance to near clip plane for shadow
         * mapping.
         *
         * @return Far to near Z ratio.
         */
        static constexpr float getVisibleDistanceToNearClipPlaneRatio() {
            return visibleDistanceToNearClipPlaneRatio;
        }

        /**
         * Creates a new shadow map manager.
         *
         * @param pResourceManager Resource manager that owns this object.
         *
         * @return Error if something went wrong, otherwise created shadow map manager.
         */
        static std::variant<std::unique_ptr<ShadowMapManager>, Error>
        create(GpuResourceManager* pResourceManager);

        /**
         * Creates a shadow map.
         *
         * @param sResourceName               Name of the GPU resource that will be created, used for logging.
         * @param type                        Type of a shadow map to create depending on the
         * light source type.
         * @param onArrayIndexChanged Called after the index of the returned shadow map into the
         * descriptor array of shadow maps was initialized/changed.
         *
         * @return Error if something went wrong, otherwise created shadow map. Returning unique ptr
         * although shadow map handle already behaves like unique ptr in order for the manager to be
         * able to store raw pointers to handles without fearing that a raw pointer will point to invalid
         * handle due to `move`ing the handle.
         */
        std::variant<std::unique_ptr<ShadowMapHandle>, Error> createShadowMap(
            const std::string& sResourceName,
            ShadowMapType type,
            const std::function<void(unsigned int)>& onArrayIndexChanged);

    private:
        /** Groups mutex guarded data. */
        struct InternalResources {
            /** All allocated shadow maps. */
            std::unordered_map<ShadowMapHandle*, std::unique_ptr<GpuResource>> shadowMaps;

            /**
             * Array index managers for various light source types.
             *
             * @remark Access using enum: `vManagers[ShadowMapType::DIRECTIONAL]`.
             */
            std::array<std::unique_ptr<ShadowMapArrayIndexManager>, static_cast<size_t>(ShadowMapType::SIZE)>
                vShadowMapArrayIndexManagers;
        };

        /**
         * Initializes the manager.
         *
         * @param pResourceManager             Resource manager that owns this object.
         * @param vShadowMapArrayIndexManagers Array index managers for various light source types
         */
        ShadowMapManager(
            GpuResourceManager* pResourceManager,
            std::array<std::unique_ptr<ShadowMapArrayIndexManager>, static_cast<size_t>(ShadowMapType::SIZE)>
                vShadowMapArrayIndexManagers);

        /**
         * Looks if the specified pipeline uses shadow maps and if uses binds shadow maps to the pipeline.
         *
         * @param pPipeline Pipeline to bind shadow maps to.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> bindShadowMapsToPipeline(Pipeline* pPipeline);

        /**
         * Goes through all graphics pipelines ad binds shadow maps to pipelines that use them.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> bindShadowMapsToAllPipelines();

        /**
         * Called by destructor of shadow map handle to notify the manager that a resource is no longer
         * used.
         *
         * @param pHandleToResourceDestroy Handle to resource to destroy.
         */
        void onShadowMapHandleBeingDestroyed(ShadowMapHandle* pHandleToResourceDestroy);

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
         * Returns pointer to one of the array index managers from @ref mtxInternalResources depending
         * on the specified shadow map type.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @remark Expects that @ref mtxInternalResources is locked.
         *
         * @param type Type of the shadow map to determine the manager.
         *
         * @return `nullptr` if specified not supported shadow map type, otherwise valid pointer.
         */
        ShadowMapArrayIndexManager* getArrayIndexManagerBasedOnShadowMapType(ShadowMapType type);

        /**
         * Allocated shadow maps.
         *
         * @remark Storing pairs of "raw pointer" - "unique pointer" to quickly find needed resources
         * when need to destroy some resource given a raw pointer.
         *
         * @remark Storing raw pointers here is safe because shadow map handle will notify us
         * before destroying the resource so we will remove the raw pointer and shadow map handle
         */
        std::pair<std::recursive_mutex, InternalResources> mtxInternalResources;

        /** Do not delete (free) this pointer. GPU resource manager that owns this object. */
        GpuResourceManager* pResourceManager = nullptr;

        /**
         * Constant depth bias (offset) to apply when rendering depth to shadow maps to avoid an
         * effect known as "shadow acne" (stair-stepping).
         */
        static constexpr int iShadowMappingDepthBias = 100000; // NOLINT

        /** Constant used to convert visible (non-clipped) distance to near clip plane for shadow mapping. */
        static constexpr float visibleDistanceToNearClipPlaneRatio = 0.004F; // NOLINT

        /** Name of the shader resource (from shader code) that stores all directional shadow maps. */
        static constexpr auto pDirectionalShadowMapsShaderResourceName = "directionalShadowMaps";

        /** Name of the shader resource (from shader code) that stores all spot shadow maps. */
        static constexpr auto pSpotShadowMapsShaderResourceName = "spotShadowMaps";

        /** Name of the shader resource (from shader code) that stores all point shadow maps. */
        static constexpr auto pPointShadowMapsShaderResourceName = "pointShadowMaps";
    };
}
