#pragma once

// Standard.
#include <variant>
#include <memory>

// Custom.
#include "misc/Error.h"
#include "render/general/resources/UploadBuffer.h"

namespace ne {
    class Renderer;
    class GpuResource;

    /** Describes how a resource will be used. */
    enum class ResourceUsageType { VERTEX_BUFFER, INDEX_BUFFER, OTHER };

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
         * Creates a new GPU resource with available CPU access, typically used
         * for resources that needs to be frequently updated from the CPU side.
         *
         * Example:
         * @code
         * struct ObjectData{
         *     glm::mat4x4 world;
         * };
         *
         * auto result = pResourceManager->createResourceWithCpuAccess(
         *     "object constant data",
         *     sizeof(ObjectData),
         *     1,
         *     true);
         * @endcode
         *
         * @param sResourceName                  Resource name, used for logging.
         * @param iElementSizeInBytes            Size of one buffer element in bytes.
         * @param iElementCount                  Amount of elements in the resulting buffer.
         * @param bIsUsedInShadersAsReadOnlyData Determines whether this resource will be used to store
         * shader read-only data (cannon be modified from shaders) or not going to be used in shaders
         * at all. You need to specify this because in some internal implementations (like DirectX)
         * it will result in element size being padded to be a multiple of 256 because of the hardware
         * requirement for shader constant buffers. Otherwise if you don't plan to use this buffer
         * in shaders (for ex. you can use it as a staging/upload buffer) specify `false`.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        virtual std::variant<std::unique_ptr<UploadBuffer>, Error> createResourceWithCpuAccess(
            const std::string& sResourceName,
            size_t iElementSizeInBytes,
            size_t iElementCount,
            bool bIsUsedInShadersAsReadOnlyData) = 0;

        /**
         * Creates a new GPU resource and fills it with the specified data.
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

    protected:
        /**
         * Creates a new platform-specific resource manager.
         *
         * @param pRenderer Used renderer.
         *
         * @return Error if something went wrong, otherwise created resource manager.
         */
        static std::variant<std::unique_ptr<GpuResourceManager>, Error> create(Renderer* pRenderer);

        /** Creates uninitialized manager, used internally. */
        GpuResourceManager() = default;
    };
} // namespace ne
