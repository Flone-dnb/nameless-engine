#pragma once

// Standard.
#include <functional>
#include <mutex>

// Custom.
#include "ShadowMapType.hpp"

// External.
#include "vulkan/vulkan_core.h"

namespace ne {
    class ShadowMapManager;
    class GpuResource;

    /**
     * Small raw pointer wrapper that acts kind of like `std::unique_ptr`
     * for shadow maps to do some extra work when started/stopped referencing a shadow map.
     *
     * When deleted causes the resource to be also deleted.
     */
    class ShadowMapHandle {
        // Only manager can create and update objects of this class.
        friend class ShadowMapManager;

        // Index manager updates array index.
        friend class ShadowMapArrayIndexManager;

    public:
        /** Groups GPU resources that a shadow map handle references. */
        struct InternalResources {
            InternalResources() = default;

            /** Depth image. */
            GpuResource* pDepthTexture = nullptr;

            /**
             * Optional (may be `nullptr`) "color" target, used for point lights to store additional
             * information.
             */
            GpuResource* pColorTexture = nullptr;

            /**
             * Framebuffers that reference @ref pDepthTexture and @ref pColorTexture (if it's valid),
             * these framebuffers are used by the Vulkan renderer during the shadow pass.
             *
             * @remark Stores only 1 framebuffer if @ref pColorTexture is `nullptr` otherwise 6 framebuffers
             * (because @ref pColorTexture is a cubemap for point lights).
             */
            std::vector<VkFramebuffer> vShadowMappingFramebuffers;
        };

        ShadowMapHandle() = delete;

        ShadowMapHandle(const ShadowMapHandle&) = delete;
        ShadowMapHandle& operator=(const ShadowMapHandle&) = delete;

        // Intentionally disallow `move` because manager wraps handles to `std::unique_ptr`
        // to store raw pointer to shadow map handle and we need to make sure nobody will
        // accidentally `move` the handle which will cause the manager's raw pointer to be invalid.
        ShadowMapHandle(ShadowMapHandle&& other) noexcept = delete;
        ShadowMapHandle& operator=(ShadowMapHandle&& other) noexcept = delete;

        ~ShadowMapHandle();

        /**
         * Returns the underlying resource.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @remark Use returned resource only when mutex is locked.
         *
         * @return Valid pointer to resource.
         */
        inline std::pair<std::recursive_mutex, InternalResources>* getResources() { return &mtxResources; }

        /**
         * Returns type of a shadow map that this handle references.
         *
         * @return Shadow map type.
         */
        inline ShadowMapType getShadowMapType() const { return shadowMapType; }

        /**
         * Returns the current size of the shadow map resource.
         *
         * @return Size in pixels.
         */
        inline size_t getShadowMapSize() const { return iShadowMapSize; }

    private:
        /**
         * Constructs a new handle.
         *
         * @param pManager                    Manager that owns the resource.
         * @param pDepthTexture               Resource to point to.
         * @param type                        Type of the shadow map this handle references.
         * @param iTextureSize                Size (in pixels) of the shadow map.
         * @param onArrayIndexChanged Called after the index of the shadow map into the
         * descriptor array of shadow maps was initialized/changed.
         * @param pColorTexture               Optional "color" target, used for point
         * lights to store additional information.
         */
        ShadowMapHandle(
            ShadowMapManager* pManager,
            GpuResource* pDepthTexture,
            ShadowMapType type,
            size_t iTextureSize,
            const std::function<void(unsigned int)>& onArrayIndexChanged,
            GpuResource* pColorTexture = nullptr);

        /**
         * Called by array index manager to notify the shadow map user about array index changed.
         *
         * @param iNewArrayIndex New array index.
         */
        void changeArrayIndex(unsigned int iNewArrayIndex);

        /**
         * Called by shadow map manager after GPU resources were re-created (due to some render settings
         * changed for example) to assign new resources.
         *
         * @param pDepthTexture  Resource to point to.
         * @param iShadowMapSize Size (in pixels) of the shadow map.
         * @param pColorTexture  Optional "color" target, used for point lights to store additional
         * information.
         */
        void setUpdatedResources(
            GpuResource* pDepthTexture, size_t iShadowMapSize, GpuResource* pColorTexture = nullptr);

        /** (Re)creates framebuffers from @ref mtxResources if running Vulkan renderer. */
        void recreateFramebuffers();

        /** Manager that owns the resource we are pointing to. */
        ShadowMapManager* pManager = nullptr;

        /** Resource that this handle references. */
        std::pair<std::recursive_mutex, InternalResources> mtxResources;

        /** Size (in pixels) of the @ref mtxResources texture, used for fast access. */
        size_t iShadowMapSize = 0;

        /**
         * Called after the index of the shadow map into the
         * descriptor array of shadow maps was initialized/changed.
         */
        const std::function<void(unsigned int)> onArrayIndexChanged;

        /** Type of the shadow map that this handle references. */
        const ShadowMapType shadowMapType = ShadowMapType::DIRECTIONAL;
    };
}
