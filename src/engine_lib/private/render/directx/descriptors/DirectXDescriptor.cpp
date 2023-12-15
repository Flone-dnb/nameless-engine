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
        pHeap->onDescriptorBeingDestroyed(this);
    }

    DirectXDescriptor::DirectXDescriptor(
        DirectXDescriptorHeap* pHeap,
        DirectXDescriptorType descriptorType,
        DirectXResource* pResource,
        int iDescriptorOffsetInDescriptors) {
        this->iDescriptorOffsetInDescriptors = iDescriptorOffsetInDescriptors;
        this->pResource = pResource;
        this->descriptorType = descriptorType;
        this->pHeap = pHeap;
    }

    DirectXResource* DirectXDescriptor::getOwnerResource() const { return pResource; }
} // namespace ne
