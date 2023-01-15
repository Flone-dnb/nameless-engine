#pragma once

// Standard.
#include <variant>
#include <memory>

// Custom.
#include "misc/Error.h"

namespace ne {
    class Renderer;
    class GpuResource;
    class UploadBuffer;

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
         * Creates a new constant buffer resource with available CPU access, typically used
         * for resources that needs to be frequently updated from the CPU side.
         *
         * @remark When used with DirectX renderer additionally binds a constant buffer view descriptor to
         * created buffer.
         *
         * @remark Due to hardware requirements resulting element size might be bigger than you've expected
         * due to padding if not multiple of 256.
         *
         * Example:
         * @code
         * struct ObjectData{
         *     glm::mat4x4 world;
         * };
         *
         * auto result = pResourceManager->createCbvResourceWithCpuAccess(
         *     "object constant data",
         *     sizeof(ObjectData),
         *     1);
         * @endcode
         *
         * @param sResourceName        Resource name, used for logging.
         * @param iElementSizeInBytes  Size of one buffer element in bytes.
         * @param iElementCount        Amount of elements in the resulting buffer.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        virtual std::variant<std::unique_ptr<UploadBuffer>, Error> createCbvResourceWithCpuAccess(
            const std::string& sResourceName, size_t iElementSizeInBytes, size_t iElementCount) const = 0;

    protected:
        GpuResourceManager() = default;
    };
} // namespace ne
