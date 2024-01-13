#include "DirectXDescriptor.h"

// Custom.
#include "render/directx/descriptors/DirectXDescriptorHeap.h"
#include "render/directx/resources/DirectXResource.h"

namespace ne {
    DirectXDescriptor::~DirectXDescriptor() {
        // Set `nullptr` to resource because most likely its destructor was already finished
        // and now resource fields are being destructed.
        // If the heap will try to use this resource pointer it will hit `nullptr` and we will instantly find
        // the error.
        pResource = nullptr;

        // Notify heap.
        pHeap->onDescriptorBeingDestroyed(this, pRange);
    }

    DirectXDescriptor::DirectXDescriptor(
        DirectXDescriptorHeap* pHeap,
        DirectXDescriptorType descriptorType,
        DirectXResource* pResource,
        int iDescriptorOffsetInDescriptors,
        std::optional<size_t> referencedCubemapFaceIndex,
        ContinuousDirectXDescriptorRange* pRange)
        : pResource(pResource), pHeap(pHeap), pRange(pRange),
          referencedCubemapFaceIndex(referencedCubemapFaceIndex), descriptorType(descriptorType) {
        // Save index.
        this->iDescriptorOffsetInDescriptors = iDescriptorOffsetInDescriptors;
    }

    DirectXResource* DirectXDescriptor::getOwnerResource() const { return pResource; }
} // namespace ne
