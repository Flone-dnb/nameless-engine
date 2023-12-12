#pragma once

// Standard.
#include <memory>
#include <variant>

// Custom.
#include "render/general/resources/GpuResource.h"
#include "misc/Error.h"

namespace ne {
    class GpuResourceManager;

    /** Wraps a GPU resource that stores a shadow map. */
    class ShadowMap {
        // Only manager is expected to create shadow maps.
        friend class ShadowMapManager;

    public:
        ShadowMap() = delete;
        ShadowMap(const ShadowMap&) = delete;
        ShadowMap& operator=(const ShadowMap&) = delete;

        /**
         * Returns underlying GPU resource that stores shadow map.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Shadow map resource.
         */
        inline GpuResource* getInternalResource() const { return pShadowMapResource.get(); }

    private:
        /**
         * Creates a new shadow map.
         *
         * @param pResourceManager Resource manager that will be used to create GPU resource.
         * @param sShadowMapName   Name of the GPU resource, used for logging.
         * @param iTextureSize     Size of one dimension of the texture in pixels.
         * Must be power of 2 (128, 256, 512, 1024, 2048, etc.).
         *
         * @return Error if something went wrong, otherwise created texture.
         */
        static std::variant<std::unique_ptr<ShadowMap>, Error> create(
            GpuResourceManager* pResourceManager,
            const std::string& sShadowMapName,
            unsigned int iTextureSize = 512);

        /**
         * Initializes shadow map.
         *
         * @param pShadowMapResource Resource to use.
         */
        ShadowMap(std::unique_ptr<GpuResource> pShadowMapResource);

        /** Underlying GPU resource that stores shadow map. */
        std::unique_ptr<GpuResource> pShadowMapResource;
    };
}
