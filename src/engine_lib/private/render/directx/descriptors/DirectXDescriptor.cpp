#include "DirectXDescriptor.h"

// Custom.
#include "render/directx/descriptors/DirectXDescriptorHeap.h"
#include "render/directx/resource/DirectXResource.h"

namespace ne {
    DirectXDescriptor::~DirectXDescriptor() {
        // Set `nullptr` to resource because most likely its destructor was already finished
        // and now resource fields are being destructed.
        // If the heap will try to use this resource pointer it will hit `nullptr` and we will instantly find
        // the error.
        pResource = nullptr;

        // Notify heap.
        pHeap->onDescriptorBeingDestroyed(this, pRange.get());
    }

    DirectXDescriptor::DirectXDescriptor(
        DirectXDescriptorHeap* pHeap,
        DirectXDescriptorType descriptorType,
        DirectXResource* pResource,
        int iDescriptorOffsetInDescriptors,
        std::optional<size_t> referencedCubemapFaceIndex,
        const std::shared_ptr<ContinuousDirectXDescriptorRange>& pRange)
        : pResource(pResource), pHeap(pHeap), pRange(pRange),
          referencedCubemapFaceIndex(referencedCubemapFaceIndex), descriptorType(descriptorType) {
        // Save index.
        this->iDescriptorOffsetInDescriptors = iDescriptorOffsetInDescriptors;
    }

    std::variant<unsigned int, Error> DirectXDescriptor::getDescriptorOffsetFromRangeStart() {
        // Make sure this descriptor was allocated from a range.
        if (pRange == nullptr) [[unlikely]] {
            return Error("expected the descriptor to be allocated from a range");
        }

        const int iDescriptorOffsetFromHeapStart = iDescriptorOffsetInDescriptors;
        const int iRangeOffsetFromHeapStart = pRange->getRangeStartInHeap();

        // Make sure range offset is valid.
        if (iRangeOffsetFromHeapStart < 0) [[unlikely]] {
            return Error(std::format(
                "descriptor range \"{}\" has a negative start index in the heap", pRange->getRangeName()));
        }

        // Calculate offset from range start.
        const int iDescriptorOffsetFromRangeStart =
            iDescriptorOffsetFromHeapStart - iRangeOffsetFromHeapStart;

        // Self check: make sure resulting value is non-negative.
        if (iDescriptorOffsetFromRangeStart < 0) [[unlikely]] {
            return Error(std::format(
                "failed to calculate descriptor offset from descriptor range start (range: \"{}\") for "
                "resource \"{}\" (descriptor offset from heap: {}, range offset from heap: {}) because "
                "resulting descriptor offset from range start is negative: {})",
                pRange->getRangeName(),
                pResource->getResourceName(),
                iDescriptorOffsetFromHeapStart,
                iRangeOffsetFromHeapStart,
                iDescriptorOffsetFromRangeStart));
        }

        return static_cast<unsigned int>(iDescriptorOffsetFromRangeStart);
    }

    DirectXResource* DirectXDescriptor::getOwnerResource() const { return pResource; }
} // namespace ne
