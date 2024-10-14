#pragma once

// Standard.
#include <string>

namespace ne {
    class GpuResource;
    class TextureManager;

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
        const std::string sPathToResourceRelativeRes;

        /** Do not delete (free) this pointer. Texture manager that created this object. */
        TextureManager* const pTextureManager = nullptr;

        /** Do not delete (free) this pointer. Texture resource that this handle references. */
        GpuResource* const pTexture = nullptr;
    };
}
