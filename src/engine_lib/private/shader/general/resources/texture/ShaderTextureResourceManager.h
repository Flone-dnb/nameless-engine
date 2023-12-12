#pragma once

// Standard.
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

// Custom.
#include "shader/general/resources/ShaderResource.h"
#include "shader/general/resources/texture/ShaderTextureResourceUniquePtr.h"

namespace ne {
    class Renderer;

    /**
     * Owns all shader resources that references texture resource.
     *
     * @remark This manager does not really do anything but it provides a single and thread-safe
     * way to interact with all shader resources. Initial motivation for this manager was to
     * have a safe way to notify all texture resources using
     * `ShaderResourceBase::onAfterAllPipelinesRefreshedResources` from pipeline manager
     * (to avoid running this function on not fully initialized shader resources or shader resources
     * that are being destroyed because this could happen if instead of this manager we just had
     * some notifications on `ShaderResourceBase` constructor/destructor).
     */
    class ShaderTextureResourceManager {
        // Only renderer should be allowed to create this manager.
        friend class Renderer;

        // Unique pointers will notify the manager before destruction.
        friend class ShaderTextureResourceUniquePtr;

    public:
        ShaderTextureResourceManager() = delete;

        ShaderTextureResourceManager(const ShaderTextureResourceManager&) = delete;
        ShaderTextureResourceManager& operator=(const ShaderTextureResourceManager&) = delete;

        /** Makes sure that no resource exists. */
        ~ShaderTextureResourceManager();

        /**
         * Creates a new render-specific shader resource.
         *
         * @param sShaderResourceName     Name of the resource we are referencing (should be exactly the same
         * as the resource name written in the shader file we are referencing).
         * @param sResourceAdditionalInfo Additional text that we will append to created resource name
         * (used for logging).
         * @param pipelinesToUse          Pipelines that use shader/parameters we are referencing.
         * @param pTextureToUse           Texture that should be binded to a descriptor.
         *
         * @return Error if something went wrong, otherwise created shader resource.
         */
        std::variant<ShaderTextureResourceUniquePtr, Error> createShaderTextureResource(
            const std::string& sShaderResourceName,
            const std::string& sResourceAdditionalInfo,
            const std::unordered_set<Pipeline*>& pipelinesToUse,
            std::unique_ptr<TextureHandle> pTextureToUse);

        /**
         * Returns all shader resources that reference textures.
         *
         * @remark Do not free (delete) or modify unique pointers or returned map.
         *
         * @return Internal resources.
         */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<ShaderTextureResource*, std::unique_ptr<ShaderTextureResource>>>*
        getResources();

    private:
        /**
         * Initializes manager.
         *
         * @param pRenderer
         */
        ShaderTextureResourceManager(Renderer* pRenderer);

        /**
         * Processes resource creation.
         *
         * @param result Result of resource creation function.
         *
         * @return Result of resource creation.
         */
        std::variant<ShaderTextureResourceUniquePtr, Error>
        handleResourceCreation(std::variant<std::unique_ptr<ShaderTextureResource>, Error> result);

        /**
         * Called by shader texture resource unique pointers to destroy the specified resource because it will
         * no longer be used.
         *
         * @param pResourceToDestroy Resource to destroy.
         */
        void destroyResource(ShaderTextureResource* pResourceToDestroy);

        /** Renderer that owns this manager. */
        Renderer* pRenderer = nullptr;

        /**
         * Shader texture resources.
         *
         * @remark Storing pairs of "raw pointer" - "unique pointer" to quickly find needed resources
         * when need to destroy some resource given a raw pointer.
         */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<ShaderTextureResource*, std::unique_ptr<ShaderTextureResource>>>
            mtxShaderTextureResources;
    };
} // namespace ne
