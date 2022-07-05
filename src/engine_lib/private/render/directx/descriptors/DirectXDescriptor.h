#pragma once

// STL.
#include <optional>

namespace ne {
    class DirectXDescriptorHeapManager;
    class DirectXResource;

    /** Represents a descriptor (to a resource) that is stored in a descriptor heap. */
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
        friend class DirectXDescriptorHeapManager;

        /**
         * Constructor.
         *
         * @param pHeap                          Owner object of this descriptor.
         * @param pResource                      Owner resource of this descriptor.
         * @param iDescriptorOffsetInDescriptors Offset of this descriptor from the heap start
         * (offset is specified in descriptors, not an actual index).
         */
        DirectXDescriptor(
            DirectXDescriptorHeapManager* pHeap,
            DirectXResource* pResource,
            int iDescriptorOffsetInDescriptors);

        /**
         * Returns resource that owns this descriptor.
         *
         * @return Owner resource.
         */
        DirectXResource* getOwnerResource() const;

    private:
        /** Do not delete. Owner heap. */
        DirectXDescriptorHeapManager* pHeap{};

        /** Do not delete. Owner resource of this descriptor. */
        DirectXResource* pResource{};

        /**
         * Offset of this descriptor from the heap start (offset is specified in descriptors, not an actual
         * index).
         */
        std::optional<int> iDescriptorOffsetInDescriptors;

        // !!!
        // if adding new fields consider adding them to move assignment operator
        // !!!
    };
} // namespace ne
