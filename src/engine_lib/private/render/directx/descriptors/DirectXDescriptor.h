#pragma once

// Standard.
#include <optional>

// Custom.
#include "DirectXDescriptorType.hpp"

namespace ne {
    class DirectXDescriptorHeap;
    class DirectXResource;

    /**
     * Represents a descriptor (to a resource) that is stored in a descriptor heap.
     * Automatically marked as unused in destructor.
     */
    class DirectXDescriptor {
        // We notify the heap about descriptor being no longer used in destructor.
        friend class DirectXDescriptorHeap;

    public:
        /** Notifies the heap. */
        ~DirectXDescriptor();

        DirectXDescriptor(const DirectXDescriptor& other) = delete;
        DirectXDescriptor& operator=(const DirectXDescriptor& other) = delete;

        // Intentionally disable `move` because heap stores raw pointers to descriptors
        // and wraps descriptors into unique ptr to provide move functionality.
        DirectXDescriptor(DirectXDescriptor&& other) noexcept = delete;
        DirectXDescriptor& operator=(DirectXDescriptor&& other) noexcept = delete;

        /**
         * Returns offset of this descriptor from the heap start
         * (offset is specified in descriptors, not an actual index).
         *
         * @return Descriptor offset.
         */
        inline int getDescriptorOffsetInDescriptors() const { return iDescriptorOffsetInDescriptors; }

        /**
         * Returns heap that this descriptor uses.
         *
         * @return Descriptor heap.
         */
        inline DirectXDescriptorHeap* getDescriptorHeap() const { return pHeap; }

        /**
         * Returns resource that owns this descriptor.
         *
         * @return Owner resource.
         */
        DirectXResource* getOwnerResource() const;

    protected:
        /**
         * Constructor.
         *
         * @param pHeap                          Heap of this descriptor.
         * @param descriptorType                 Type of this descriptor.
         * @param pResource                      Owner resource of this descriptor.
         * @param iDescriptorOffsetInDescriptors Offset of this descriptor from the heap start
         * (offset is specified in descriptors, not an actual index).
         */
        DirectXDescriptor(
            DirectXDescriptorHeap* pHeap,
            DirectXDescriptorType descriptorType,
            DirectXResource* pResource,
            int iDescriptorOffsetInDescriptors);

    private:
        /** Do not delete. Owner resource of this descriptor. */
        DirectXResource* pResource = nullptr;

        /** Do not delete. Heap of this descriptor. */
        DirectXDescriptorHeap* pHeap = nullptr;

        /**
         * Offset of this descriptor from the heap start (offset is specified in descriptors,
         * not an actual index).
         */
        int iDescriptorOffsetInDescriptors;

        /** Type of this descriptor. */
        DirectXDescriptorType descriptorType;
    };
} // namespace ne
