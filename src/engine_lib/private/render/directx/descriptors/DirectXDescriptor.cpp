#include "DirectXDescriptor.h"

// Custom.
#include "render/directx/descriptors/DirectXDescriptorHeapManager.h"
#include "render/directx/resources/DirectXResource.h"

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

            pResource = other.pResource;
            pHeap = other.pHeap;
            descriptorType = other.descriptorType;
        }

        return *this;
    }

    DirectXDescriptor::DirectXDescriptor(
        DirectXDescriptorHeapManager* pHeap,
        DescriptorType descriptorType,
        DirectXResource* pResource,
        int iDescriptorOffsetInDescriptors) {
        this->iDescriptorOffsetInDescriptors = iDescriptorOffsetInDescriptors;
        this->pResource = pResource;
        this->descriptorType = descriptorType;
        this->pHeap = pHeap;
    }

    DirectXResource* DirectXDescriptor::getOwnerResource() const { return pResource; }
} // namespace ne
