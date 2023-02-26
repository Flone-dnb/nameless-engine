#include "DirectXDescriptor.h"

// Custom.
#include "render/directx/descriptors/DirectXDescriptorHeap.h"
#include "render/directx/resources/DirectXResource.h"

namespace ne {
    DirectXDescriptor::~DirectXDescriptor() {
        if (iDescriptorOffsetInDescriptors.has_value()) {
            pHeap->markDescriptorAsNoLongerBeingUsed(pResource);
        }
    }

    DirectXDescriptor::DirectXDescriptor(DirectXDescriptor&& other) noexcept { *this = std::move(other); }

    DirectXDescriptor& DirectXDescriptor::operator=(DirectXDescriptor&& other) noexcept {
        static_assert(
            sizeof(DirectXDescriptor) == 32, // NOLINT: current size
            "add new/edited fields to move operator");

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

    std::optional<int> DirectXDescriptor::getDescriptorOffsetInDescriptors() const {
        return iDescriptorOffsetInDescriptors;
    }

    DirectXDescriptorHeap* DirectXDescriptor::getDescriptorHeap() const { return pHeap; }

    DirectXDescriptor::DirectXDescriptor(
        DirectXDescriptorHeap* pHeap,
        GpuResource::DescriptorType descriptorType,
        DirectXResource* pResource,
        int iDescriptorOffsetInDescriptors) {
        this->iDescriptorOffsetInDescriptors = iDescriptorOffsetInDescriptors;
        this->pResource = pResource;
        this->descriptorType = descriptorType;
        this->pHeap = pHeap;
    }

    DirectXResource* DirectXDescriptor::getOwnerResource() const { return pResource; }
} // namespace ne
