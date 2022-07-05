#include "DirectXDescriptor.h"

// Custom.
#include "render/directx/descriptors/DirectXDescriptorHeapManager.h"

namespace ne {
    DirectXDescriptor::~DirectXDescriptor() {
        if (iDescriptorOffsetInDescriptors.has_value()) {
            pHeap->markDescriptorAsNoLongerBeingUsed(pResource);
        }
    }

    DirectXDescriptor::DirectXDescriptor(DirectXDescriptor&& other) noexcept { *this = std::move(other); }

    DirectXDescriptor& DirectXDescriptor::operator=(DirectXDescriptor&& other) noexcept {
        if (this != &other) {
            if (other.iDescriptorOffsetInDescriptors.has_value()) {
                iDescriptorOffsetInDescriptors = other.iDescriptorOffsetInDescriptors.value();
                other.iDescriptorOffsetInDescriptors = {};
            }

            pHeap = other.pHeap;
            pResource = other.pResource;
        }

        return *this;
    }

    DirectXDescriptor::DirectXDescriptor(
        DirectXDescriptorHeapManager* pHeap, DirectXResource* pResource, int iDescriptorOffsetInDescriptors) {
        this->iDescriptorOffsetInDescriptors = iDescriptorOffsetInDescriptors;
        this->pHeap = pHeap;
        this->pResource = pResource;
    }

    DirectXResource* DirectXDescriptor::getOwnerResource() const { return pResource; }
} // namespace ne
