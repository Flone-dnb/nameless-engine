#pragma once

// Standard.
#include <variant>
#include <memory>
#include <filesystem>
#include <optional>

// Custom.
#include "misc/Error.h"
#include "render/general/resources/UploadBuffer.h"

namespace ne {
    class Renderer;
    class GpuResource;

    /** Describes how a resource will be used. */
    enum class ResourceUsageType { VERTEX_BUFFER, INDEX_BUFFER, OTHER };

    /** Determines the usage of a resources used by shaders. */
    struct CpuVisibleShaderResourceUsageDetails {
        CpuVisibleShaderResourceUsageDetails() = delete;

        /**
         * Constructs a CPU write shader resource "usage" details.
         *
         * @param bForceFastResourceType Forces strict underlying resource type to guarantee
         * that the shader code will always treat the resource type correctly.
         * Specify `true` if the shader resource will only be used
         * to ALWAYS store small GPU resources (resource size is typically smaller than 64 KB),
         * for example if you plan to store an array of objects in the resource that have fixed
         * amount and their total size will most likely always be smaller than 64 KB specify `true`.
         * Otherwise if you know that at some point you will need to re-create the resource to expand
         * it or if you don't know the final resource size (maybe it's determined at runtime) and
         * there is a good chance that resource size will exceed the 64 KB limit specify `false`,
         * for example if you plan to store an array of objects in the resource the number of which
         * may vary at runtime specify `false`. When you specify `true` some renderers will stick
         * to a specific small but fast resource type and if you re-create the resource to maybe
         * expand it a little bit and specify `true` again the same small but fast resource type
         * will be picked, if you go out of size limit around 64 KB resource creation will fail.
         * When you specify `false` some renderers will stick to a specific unlimited in size
         * resource type and if you re-create the resource to maybe expand it a little bit
         * and specify `false` again the same unlimited in size but not the most fast resource
         * type will be picked. Some renderers have different types of resources depending
         * on their size and usage so when you plan to re-create the resource to expand/shrink it
         * it's important to make sure that it's underlying type will not change otherwise
         * the shader code will need to be changed to threat the resource in other way,
         * for example in some cases the resource type in GLSL shaders may be marked as
         * `uniform` but in some cases it may not fit in the uniform buffer size limit which
         * means that another type of buffer will be required.
         */
        explicit CpuVisibleShaderResourceUsageDetails(bool bForceFastResourceType) {
            this->bForceFastResourceType = bForceFastResourceType;
        }

        /** `true` for small and fast resources, `false` for unlimited in size resources. */
        bool bForceFastResourceType;
    };

    /** Allows creating GPU resources. */
    class GpuResourceManager {
        // Only renderer should be allowed to create resource manager.
        friend class Renderer;

    public:
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
         * Creates a new GPU resource with available CPU write access (only write not read),
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
         *     CpuVisibleShaderResourceUsageDetails(true));
         * @endcode
         *
         * @param sResourceName                 Resource name, used for logging.
         * @param iElementSizeInBytes           Size of one buffer element in bytes.
         * @param iElementCount                 Amount of elements in the resulting buffer.
         * @param isUsedInShadersAsReadOnlyData Determines whether this resource will be used to store
         * shader read-only data (cannon be modified from shaders) or not going to be used in shaders
         * at all (specify empty). You need to specify this because in some internal implementations
         * might pad the specified element size to be a multiple of 256 because of some
         * hardware requirements. Otherwise if you don't plan to use this buffer
         * in shaders (for ex. you can use it as a staging/upload buffer) specify empty.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        virtual std::variant<std::unique_ptr<UploadBuffer>, Error> createResourceWithCpuWriteAccess(
            const std::string& sResourceName,
            size_t iElementSizeInBytes,
            size_t iElementCount,
            std::optional<CpuVisibleShaderResourceUsageDetails> isUsedInShadersAsReadOnlyData) = 0;

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
         *     vVertices.size() * sizeof(glm::vec3),
         *     true);
         * @endcode
         *
         * @param sResourceName              Resource name, used for logging.
         * @param pBufferData                Pointer to the data that the new resource will contain.
         * @param iDataSizeInBytes           Size in bytes of the data (resource size).
         * @param usage                      Describes how you plan to use this resource.
         * @param bIsShaderReadWriteResource Specify `true` if you plan to modify the resource
         * from shaders, otherwise `false`.
         *
         * @return Error if something went wrong, otherwise created resource with filled data.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> createResourceWithData(
            const std::string& sResourceName,
            const void* pBufferData,
            size_t iDataSizeInBytes,
            ResourceUsageType usage,
            bool bIsShaderReadWriteResource) = 0;

        /**
         * Returns renderer that owns this resource manager.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @return Renderer that owns this manager.
         */
        Renderer* getRenderer() const;

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

    private:
        /** Do not delete (free) this pointer. Renderer that owns this manager. */
        Renderer* pRenderer = nullptr;
    };
} // namespace ne
