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

    /** Base class for render-specific GPU resource managers. */
    class GpuResourceManager {
    public:
        virtual ~GpuResourceManager() = default;

        /**
         * Creates a new platform-specific resource manager.
         *
         * @param pRenderer Used renderer.
         *
         * @return Error if something went wrong, otherwise created resource manager.
         */
        static std::variant<std::unique_ptr<GpuResourceManager>, Error> create(Renderer* pRenderer);

        /**
         * Returns total video memory size (VRAM) in megabytes.
         *
         * @return Total video memory size in megabytes.
         */
        virtual size_t getTotalVideoMemoryInMb() const = 0;

        /**
         * Returns used video memory size (VRAM) in megabytes.
         *
         * @return Used video memory size in megabytes.
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
         * @param sResourceName           Resource name, used for logging.
         * @param iElementSizeInBytes     Size of one buffer element in bytes.
         * @param iElementCount           Amount of elements in the resulting buffer.
         * @param bIsShaderConstantBuffer Determines whether this resource will be used as
         * a constant buffer in shaders or not. Specify `true` if you plan to use this resource
         * as a constant buffer in shaders, this will result in element size being padded to
         * be a multiple of 256 because of the hardware requirement for shader constant buffers.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        virtual std::variant<std::unique_ptr<UploadBuffer>, Error> createResourceWithCpuAccess(
            const std::string& sResourceName,
            size_t iElementSizeInBytes,
            size_t iElementCount,
            bool bIsShaderConstantBuffer) const = 0;

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
         * @param sResourceName         Resource name, used for logging.
         * @param pBufferData           Pointer to the data that the new resource will contain.
         * @param iDataSizeInBytes      Size in bytes of the data (resource size).
         * @param bAllowUnorderedAccess Whether the new resource allows unordered access or not.
         *
         * @return Error if something went wrong, otherwise created resource with filled data.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> createResourceWithData(
            const std::string& sResourceName,
            const void* pBufferData,
            size_t iDataSizeInBytes,
            bool bAllowUnorderedAccess) const = 0;

    protected:
        GpuResourceManager() = default;
    };
} // namespace ne
