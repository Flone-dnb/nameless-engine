#include "UploadBuffer.h"

// Standard.
#include <stdexcept>

// Custom.
#include "render/resources/GpuResource.h"
#if defined(WIN32)
#include "render/directx/resources/DirectXResource.h"
#endif

namespace ne {

    UploadBuffer::UploadBuffer(
        std::unique_ptr<GpuResource> pGpuResourceToUse, size_t iElementSizeInBytes, size_t iElementCount) {
        pGpuResource = std::move(pGpuResourceToUse);
        this->iElementSizeInBytes = iElementSizeInBytes;
        this->iElementCount = iElementCount;

#if defined(WIN32)
        if (auto pDirectXResource = dynamic_cast<DirectXResource*>(pGpuResource.get())) {
            // Map the resource.
            auto hResult = pDirectXResource->getInternalResource()->Map(
                0, nullptr, reinterpret_cast<void**>(&pMappedResourceData));
            if (FAILED(hResult)) {
                Error error(hResult);
                error.addEntry();
                error.showError();
                throw std::runtime_error(error.getError());
            }
        }
#endif
    }

    void UploadBuffer::copyDataToElement(size_t iElementIndex, void* pData, size_t iDataSize) {
        std::memcpy(&pMappedResourceData[iElementIndex * iElementSizeInBytes], pData, iDataSize);
    }

    size_t UploadBuffer::getElementCount() const { return iElementCount; }

    size_t UploadBuffer::getElementSizeInBytes() const { return iElementSizeInBytes; }

    UploadBuffer::~UploadBuffer() {
#if defined(WIN32)
        if (auto pDirectXResource = dynamic_cast<DirectXResource*>(pGpuResource.get())) {
            // Unmap the resource.
            pDirectXResource->getInternalResource()->Unmap(0, nullptr);
            pMappedResourceData = nullptr;
        }
#endif
    }

    GpuResource* UploadBuffer::getInternalResource() const { return pGpuResource.get(); }

} // namespace ne
