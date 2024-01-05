#pragma once

// Standard.
#include <functional>
#include <mutex>

// Custom.
#include "ShadowMapType.hpp"

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
        inline std::pair<std::recursive_mutex, GpuResource*>* getResource() { return &mtxResource; }

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
         * @param pResource                   Resource to point to.
         * @param type                        Type of the shadow map this handle references.
         * @param iTextureSize                Size (in pixels) of the shadow map.
         * @param onArrayIndexChanged Called after the index of the shadow map into the
         * descriptor array of shadow maps was initialized/changed.
         */
        ShadowMapHandle(
            ShadowMapManager* pManager,
            GpuResource* pResource,
            ShadowMapType type,
            size_t iTextureSize,
            const std::function<void(unsigned int)>& onArrayIndexChanged);

        /**
         * Called by array index manager to notify the shadow map user about array index changed.
         *
         * @param iNewArrayIndex New array index.
         */
        void changeArrayIndex(unsigned int iNewArrayIndex);

        /** Manager that owns the resource we are pointing to. */
        ShadowMapManager* pManager = nullptr;

        /** Resource that this handle references. */
        std::pair<std::recursive_mutex, GpuResource*> mtxResource;

        /** Size (in pixels) of the @ref mtxResource texture, used for fast access. */
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
