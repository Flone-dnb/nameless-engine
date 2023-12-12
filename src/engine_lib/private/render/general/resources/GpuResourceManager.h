#pragma once

// Standard.
#include <variant>
#include <memory>
#include <filesystem>
#include <optional>

// Custom.
#include "misc/Error.h"
#include "render/general/resources/UploadBuffer.h"
#include "material/TextureManager.h"
#include "render/general/resources/shadow/ShadowMapManager.h"

namespace ne {
    class Renderer;
    class GpuResource;

    /** Describes how a resource will be used. */
    enum class ResourceUsageType {
        VERTEX_BUFFER, ///< Vertex buffer.
        INDEX_BUFFER,  ///< Index buffer.
        ARRAY_BUFFER,  ///< `(RW)StructuredBuffer` or storage buffer (`(readonly) buffer`).
        OTHER
    };

    /** Format of a texture resource to be used in shaders as a read/write resource. */
    enum class ShaderReadWriteTextureResourceFormat {
        R32G32_UINT,
        // ONLY THE FOLLOWING FORMATS CAN BE ADDED HERE:
        // 1. Formats that are supported as Vulkan storage images on most of the GPUs. Please, make sure you
        // don't add new formats without checking Vulkan Hardware Database (take Intel(R) UHD
        // Graphics 600 on Linux (!) for example, if it supports the format as storage image then it's
        // OK to add it here).
        // 2. Formats that have the same type in both Vulkan and DirectX.

        SIZE, //< marks the size of this enum
    };

    /** Allows creating GPU resources. */
    class GpuResourceManager {
        // Only renderer should be allowed to create resource manager.
        friend class Renderer;

    public:
        GpuResourceManager() = delete;

        virtual ~GpuResourceManager() = default;

        /**
         * Returns total video memory size (VRAM) in megabytes.
         *
         * @return Total video memory size in megabytes.
         */
        virtual size_t getTotalVideoMemoryInMb() const = 0;

        /**
         * Returns the amount of video memory (VRAM) occupied by all currently allocated resources.
         *
         * @return Size of the video memory used by allocated resources.
         */
        virtual size_t getUsedVideoMemoryInMb() const = 0;

        /**
         * Loads a texture from the specified path in the GPU memory.
         *
         * @param sResourceName     Resource name, used for logging.
         * @param pathToTextureFile Path to the image file that stores texture data.
         *
         * @return Error if something went wrong, otherwise created GPU resource that stores texture data.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> loadTextureFromDisk(
            const std::string& sResourceName, const std::filesystem::path& pathToTextureFile) = 0;

        /**
         * Creates a new GPU resource with available CPU write access (only CPU write not read),
         * typically used for resources that needs to be frequently updated from the CPU side.
         *
         * Example:
         * @code
         * struct ObjectData{
         *     glm::mat4x4 world;
         * };
         *
         * auto result = pResourceManager->createResourceWithCpuWriteAccess(
         *     "object constant data",
         *     sizeof(ObjectData),
         *     1,
         *     false);
         * @endcode
         *
         * @param sResourceName                  Resource name, used for logging.
         * @param iElementSizeInBytes            Size of one buffer element in bytes.
         * @param iElementCount                  Number of elements in the resulting buffer.
         * @param isUsedInShadersAsArrayResource Specify `empty` if this resource is not going to be
         * used in shaders, `false` if this resource will be used in shaders as a single constant (cbuffer in
         * HLSL, uniform in GLSL), might cause padding to 256 bytes and size limitation up to 64 KB, specify
         * `true` if this resource will be used in shaders as an array resource (StructuredBuffer in HLSL,
         * storage buffer in GLSL - not an array but it will be a storage buffer).
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        virtual std::variant<std::unique_ptr<UploadBuffer>, Error> createResourceWithCpuWriteAccess(
            const std::string& sResourceName,
            size_t iElementSizeInBytes,
            size_t iElementCount,
            std::optional<bool> isUsedInShadersAsArrayResource) = 0;

        /**
         * Creates a new GPU resource (buffer, not a texture) and fills it with the specified data.
         *
         * Example:
         * @code
         * std::vector<glm::vec3> vVertices;
         *
         * auto result = pResourceManager->createResourceWithData(
         *     "mesh vertex buffer",
         *     vVertices.data(),
         *     sizeof(glm::vec3),
         *     vVertices.size(),
         *     true);
         * @endcode
         *
         * @param sResourceName              Resource name, used for logging.
         * @param pBufferData                Pointer to the data that the new resource will contain.
         * @param iElementSizeInBytes        Size of one buffer element in bytes.
         * @param iElementCount              Number of elements in the resulting buffer.
         * @param usage                      Describes how you plan to use this resource.
         * @param bIsShaderReadWriteResource Specify `true` if you plan to modify the resource
         * from shaders, otherwise `false`.
         *
         * @return Error if something went wrong, otherwise created resource with filled data.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> createResourceWithData(
            const std::string& sResourceName,
            const void* pBufferData,
            size_t iElementSizeInBytes,
            size_t iElementCount,
            ResourceUsageType usage,
            bool bIsShaderReadWriteResource) = 0;

        /**
         * Creates a new GPU resource (buffer, not a texture) without any initial data.
         *
         * @remark This function can be useful if you plan to create a resource to be filled
         * from a (compute) shader and then use this data in some other shader.
         *
         * @param sResourceName              Resource name, used for logging.
         * @param iElementSizeInBytes        Size of one buffer element in bytes.
         * @param iElementCount              Number of elements in the resulting buffer.
         * @param usage                      Describes how you plan to use this resource.
         * @param bIsShaderReadWriteResource Specify `true` if you plan to modify the resource
         * from shaders, otherwise `false`.
         *
         * @return Error if something went wrong, otherwise created resource with filled data.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> createResource(
            const std::string& sResourceName,
            size_t iElementSizeInBytes,
            size_t iElementCount,
            ResourceUsageType usage,
            bool bIsShaderReadWriteResource) = 0;

        /**
         * Creates a texture resource that is available as a read/write resource in shaders.
         *
         * @param sResourceName Resource name, used for logging.
         * @param iWidth        Width of the texture in pixels.
         * @param iHeight       Height of the texture in pixels.
         * @param format        Format of the texture.
         *
         * @return Error if something went wrong, otherwise created texture resource.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> createShaderReadWriteTextureResource(
            const std::string& sResourceName,
            unsigned int iWidth,
            unsigned int iHeight,
            ShaderReadWriteTextureResourceFormat format) = 0;

        /**
         * Dumps internal state of the resource manager in JSON format.
         *
         * @return JSON string.
         */
        virtual std::string getCurrentStateInfo() = 0;

        /**
         * Returns renderer that owns this resource manager.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @return Renderer that owns this manager.
         */
        Renderer* getRenderer() const;

        /**
         * Returns texture manager.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @return Texture manager.
         */
        TextureManager* getTextureManager() const;

        /**
         * Returns shadow map manager.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @return Shadow map manager.
         */
        ShadowMapManager* getShadowMapManager() const;

    protected:
        /**
         * Creates a new platform-specific resource manager.
         *
         * @param pRenderer Used renderer.
         *
         * @return Error if something went wrong, otherwise created resource manager.
         */
        static std::variant<std::unique_ptr<GpuResourceManager>, Error> create(Renderer* pRenderer);

        /**
         * Creates uninitialized manager, used internally.
         *
         * @param pRenderer Renderer that owns this manager.
         */
        GpuResourceManager(Renderer* pRenderer);

        /**
         * Sets `nullptr` to texture manager's unique ptr to force destroy it (if exists).
         *
         * @warning Avoid using this function. Only use it if you need a special destruction order
         * in your object.
         */
        void resetTextureManager();

        /**
         * Sets `nullptr` to shadow map manager's unique ptr to force destroy it (if exists).
         *
         * @warning Avoid using this function. Only use it if you need a special destruction order
         * in your object.
         */
        void resetShadowMapManager();

    private:
        /** Stores all texture GPU resources. */
        std::unique_ptr<TextureManager> pTextureManager;

        /** Stores all shadow maps. */
        std::unique_ptr<ShadowMapManager> pShadowMapManager;

        /** Do not delete (free) this pointer. Renderer that owns this manager. */
        Renderer* pRenderer = nullptr;
    };
} // namespace ne
