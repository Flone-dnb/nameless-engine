﻿#pragma once

// Standard.
#include <optional>

namespace ne {
    class DirectXDescriptorHeap;
    class DirectXResource;

    /** Types of descriptors that point to resources and specify how a resource should be used. */
    enum class DirectXDescriptorType : size_t {
        RTV = 0,
        DSV,
        CBV,
        SRV,
        UAV,

        END // marks the size of this enum
    };

    /**
     * Represents a descriptor (to a resource) that is stored in a descriptor heap.
     * Automatically marked as unused in destructor.
     */
    class DirectXDescriptor {
        // We notify the heap about descriptor being no longer used in destructor.
        friend class DirectXDescriptorHeap;

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

        /**
         * Returns offset of this descriptor from the heap start
         * (offset is specified in descriptors, not an actual index).
         *
         * @return Empty if this object was moved (i.e. invalid now), otherwise descriptor offset.
         */
        inline std::optional<int> getDescriptorOffsetInDescriptors() const {
            return iDescriptorOffsetInDescriptors;
        }

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
        std::optional<int> iDescriptorOffsetInDescriptors;

        /** Type of this descriptor. */
        DirectXDescriptorType descriptorType;
    };
} // namespace ne
