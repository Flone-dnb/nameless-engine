#pragma once

// Standard.
#include <memory>

namespace ne {
    class GpuResource;

    /**
     * Wrapper class with handy functions that takes ownership of a GPU resource with available CPU
     * access, typically used when a buffer needs to be frequently updated from the CPU side.
     */
    class UploadBuffer {
    public:
        /**
         * Initializes the wrapper with data to use.
         *
         * @param pGpuResourceToUse   GPU resource with available CPU access.
         * @param iElementSizeInBytes Size of one buffer element in bytes (already padded if needed).
         * @param iElementCount       Amount of elements in the resource.
         */
        UploadBuffer(
            std::unique_ptr<GpuResource> pGpuResourceToUse, size_t iElementSizeInBytes, size_t iElementCount);

        ~UploadBuffer();

        UploadBuffer() = delete;
        UploadBuffer(UploadBuffer&) = delete;
        UploadBuffer& operator=(UploadBuffer&) = delete;

        /**
         * Copies the specified data to the resource.
         *
         * @param iElementIndex Index of the element to copy the data to (see @ref getElementCount).
         * @param pData         Data to copy.
         * @param iDataSize     Size in bytes of data to copy.
         */
        void copyDataToElement(size_t iElementIndex, const void* pData, size_t iDataSize);

        /**
         * Returns the number of elements stored in the buffer.
         *
         * @return Number of elements stored in the buffer.
         */
        size_t getElementCount() const;

        /**
         * Returns the size of one element (includes padding if it was needed) stored in the buffer.
         *
         * @return Element size in bytes.
         */
        size_t getElementSizeInBytes() const;

        /**
         * Returns GPU resource that this object wraps.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return GPU resource.
         */
        GpuResource* getInternalResource() const;

    private:
        /**
         * GPU resource with available CPU access.
         *
         * @warning In DirectX, before releasing this resource we should `Unmap` it.
         */
        std::unique_ptr<GpuResource> pGpuResource;

        /**
         * CPU pointer to the data located in @ref pGpuResource.
         *
         * @remark CPU reads should be avoided, they will work, but are prohibitively slow
         * on many common GPU architectures.
         */
        unsigned char* pMappedResourceData = nullptr;

        /** Size of one buffer element in bytes (see @ref iElementCount). */
        size_t iElementSizeInBytes = 0;

        /** Amount of elements in @ref pGpuResource. */
        size_t iElementCount = 0;
    };
} // namespace ne
