#pragma once

// STL.
#include <optional>

#include "render/directx/resources/DirectXResourceManager.h"

namespace ne {
    class DirectXDescriptorHeap;
    class DirectXResource;

    /** Defines types of different descriptors. */
    enum class DescriptorType : int {
        RTV = 0,
        DSV,
        CBV,
        SRV,
        UAV,
        END // marks the end of this enum
    };

    /**
     * Represents a descriptor (to a resource) that is stored in a descriptor heap.
     * Automatically marked as unused in destructor.
     */
    class DirectXDescriptor {
    public:
        /** Destructor. */
        ~DirectXDescriptor();

        DirectXDescriptor(const DirectXDescriptor& other) = delete;
        DirectXDescriptor& operator=(const DirectXDescriptor& other) = delete;

        /**
         * Move constructor.
         *
         * @param other other object.
         */
        DirectXDescriptor(DirectXDescriptor&& other) noexcept;

        /**
         * Move assignment.
         *
         * @param other other object.
         *
         * @return Result of move assignment.
         */
        DirectXDescriptor& operator=(DirectXDescriptor&& other) noexcept;

    protected:
        // We notify the heap about descriptor being no longer used
        // in destructor.
        friend class DirectXDescriptorHeap;

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
            DescriptorType descriptorType,
            DirectXResource* pResource,
            int iDescriptorOffsetInDescriptors);

        /**
         * Returns resource that owns this descriptor.
         *
         * @return Owner resource.
         */
        DirectXResource* getOwnerResource() const;

    private:
        // !!!
        // if adding new fields consider adding them to move assignment operator
        // !!!

        /** Do not delete. Owner resource of this descriptor. */
        DirectXResource* pResource = nullptr;

        /** Do not delete. Heap of this descriptor. */
        DirectXDescriptorHeap* pHeap = nullptr;

        /**
         * Offset of this descriptor from the heap start (offset is specified in descriptors, not an actual
         * index).
         */
        std::optional<int> iDescriptorOffsetInDescriptors;

        /** Type of this descriptor. */
        DescriptorType descriptorType;

        // !!!
        // if adding new fields consider adding them to move assignment operator
        // !!!
    };
} // namespace ne
