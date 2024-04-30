#pragma once

// Standard.
#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>
#include <variant>

// Custom.
#include "misc/Error.h"
#include "render/general/resources/GpuResource.h"

namespace ne {

    class TextureManager;
    class GpuResourceManager;

    /**
     * RAII-style object that tells the manager to not release the texture from the memory while it's
     * being used. A texture resource will be released from the memory when no texture handle that
     * references the same resource path will exist.
     */
    class TextureHandle {
        // We expect that only texture manager will create texture handles.
        friend class TextureManager;

    public:
        TextureHandle() = delete;

        TextureHandle(const TextureHandle&) = delete;
        TextureHandle& operator=(const TextureHandle&) = delete;

        TextureHandle(TextureHandle&&) = delete;
        TextureHandle& operator=(TextureHandle&&) = delete;

        /**
         * Returns the underlying GPU resource.
         *
         * @return GPU resource.
         */
        GpuResource* getResource();

        /**
         * Path to texture (file/directory) relative to `res` directory.
         *
         * @return Path relative `res` directory.
         */
        std::string getPathToResourceRelativeRes();

        /** Notifies manager about handle no longer referencing the texture. */
        ~TextureHandle();

    private:
        /**
         * Creates a new texture handle that references a specific texture resource.
         *
         * @param pTextureManager            Texture manager that created this handle. It will be notified
         * when the texture handle is being destroyed.
         * @param sPathToResourceRelativeRes Path to the texture (file/directory) relative to `res` directory
         * that this texture handle references.
         * @param pTexture                   Texture resource that this handle references.
         */
        TextureHandle(
            TextureManager* pTextureManager,
            const std::string& sPathToResourceRelativeRes,
            GpuResource* pTexture);

        /**
         * Path to texture (file/directory) relative to `res` directory.
         *
         * @remark Used in texture manager to determine which texture resource this handle references.
         */
        std::string sPathToResourceRelativeRes;

        /** Do not delete (free) this pointer. Texture manager that created this object. */
        TextureManager* pTextureManager = nullptr;

        /** Do not delete (free) this pointer. Texture resource that this handle references. */
        GpuResource* pTexture = nullptr;
    };

    /** Controls texture loading and owns all textures. */
    class TextureManager {
        // Texture handles will notify texture manager in destructor to mark referenced texture resource
        // as not used so that texture manager can release the texture resource from memory
        // if no texture handle is referencing it.
        friend class TextureHandle;

    public:
        TextureManager(const TextureManager&) = delete;
        TextureManager& operator=(const TextureManager&) = delete;

        TextureManager() = delete;

        /**
         * Creates a new texture manager.
         *
         * @param pResourceManager Resource manager that owns this object.
         */
        TextureManager(GpuResourceManager* pResourceManager);

        /** Makes sure that no resource is loaded in the memory. */
        ~TextureManager();

        /**
         * Returns the current number of textures loaded in the memory.
         *
         * @return Textures in memory.
         */
        size_t getTextureInMemoryCount();

        /**
         * Looks if the specified texture is loaded in the GPU memory or not and if not loads it
         * in the GPU memory and returns a new handle that references this texture (if the texture is
         * already loaded just returns a new handle).
         *
         * @param sPathToResourceRelativeRes Path to texture (file/directory) relative to `res` directory.
         *
         * @return Error if something went wrong, otherwise RAII-style object that tells the manager to not
         * release the texture from the memory while it's being used. A texture resource will be
         * released from the memory when no texture handle that references this path will exist.
         * Returning `std::unique_ptr` so that the handle can be "moved" and "reset" and we don't
         * need to care about implementing this functionality for the handle class.
         */
        std::variant<std::unique_ptr<TextureHandle>, Error>
        getTexture(const std::string& sPathToResourceRelativeRes);

    private:
        /** Groups information about a GPU resource that stores a texture. */
        struct TextureResource {
            /** Texture stored in GPU memory. */
            std::unique_ptr<GpuResource> pTexture;

            /** Describes how much active texture handles there are that point to @ref pTexture. */
            size_t iActiveTextureHandleCount = 0;
        };

        /**
         * Called by texture handles in their destructor to notify the manager about a texture
         * handle no longer referencing a texture resource so that the manager can release
         * the texture resource if no other texture handle is referencing it.
         *
         * @param sPathToResourceRelativeRes Path to texture (file/directory) relative to `res` directory.
         */
        void releaseTextureResourceIfNotUsed(const std::string& sPathToResourceRelativeRes);

        /** Initializes @ref sTextureFormatExtension depending on the current renderer. */
        void determineTextureFormatExtension();

        /**
         * Creates a new texture handle for the specified path by using @ref mtxTextureResources.
         *
         * @remark Increment handle counter.
         *
         * @param sPathToResourceRelativeRes Path to texture (file/directory) relative to `res` directory.
         *
         * @return New texture handle.
         */
        std::unique_ptr<TextureHandle> createNewTextureHandle(const std::string& sPathToResourceRelativeRes);

        /**
         * Loads the texture from the specified path and creates a new handle using
         * @ref createNewTextureHandle.
         *
         * @param sPathToResourceRelativeRes Path to texture (file/directory) relative to `res` directory.
         *
         * @return Error if something went wrong, otherwise created texture handle.
         */
        std::variant<std::unique_ptr<TextureHandle>, Error>
        loadTextureAndCreateNewTextureHandle(const std::string& sPathToResourceRelativeRes);

        /**
         * Stores pairs of "path to texture (file/directory) relative to `res` directory" - "loaded
         * texture resource".
         */
        std::pair<std::recursive_mutex, std::unordered_map<std::string, TextureResource>> mtxTextureResources;

        /** Either ".dds" or ".ktx" depending on the current renderer. */
        std::string sTextureFormatExtension;

        /** Do not delete (free) this pointer. Resource manager that owns this object. */
        GpuResourceManager* const pResourceManager = nullptr;
    };
}
