#pragma once

// Standard.
#include <memory>
#include <optional>
#include <variant>

// Custom.
#include "DirectXDescriptorType.hpp"
#include "misc/Error.h"

namespace ne {
    class DirectXDescriptorHeap;
    class ContinuousDirectXDescriptorRange;
    class DirectXResource;

    /**
     * Represents a descriptor (to a resource) that is stored in a descriptor heap.
     * Automatically marked as unused in destructor.
     */
    class DirectXDescriptor {
        // We notify the heap about descriptor being no longer used in destructor.
        friend class DirectXDescriptorHeap;

    public:
        DirectXDescriptor() = delete;

        /** Notifies the heap. */
        ~DirectXDescriptor();

        DirectXDescriptor(const DirectXDescriptor& other) = delete;
        DirectXDescriptor& operator=(const DirectXDescriptor& other) = delete;

        // Intentionally disable `move` because heap stores raw pointers to descriptors
        // and wraps descriptors into unique ptr to provide move functionality.
        DirectXDescriptor(DirectXDescriptor&& other) noexcept = delete;
        DirectXDescriptor& operator=(DirectXDescriptor&& other) noexcept = delete;

        /**
         * Returns offset of this descriptor from the heap start (offset is specified in descriptors, not an
         * actual index).
         *
         * @warning Returned value is only valid until the descriptor heap has not resized, so it's only safe
         * to call this function when you know that the descriptor heap will not resize. This function
         * is generally used during rendering when we know that the descriptor heap will not resize.
         *
         * @warning You shouldn't store returned offset for more that 1 frame as it might change after a frame
         * is submitted (because the descriptor heap may resize).
         *
         * @return Descriptor offset.
         */
        int getOffsetInDescriptorsOnCurrentFrame() const { return iDescriptorOffsetInDescriptors; }

        /**
         * Calculates an offset of the descriptor from the start of the range (see @ref getDescriptorRange)
         * that this descriptor was allocated from.
         *
         * @warning Returned value is only valid until the descriptor heap has not resized, so it's only safe
         * to call this function when you know that the descriptor heap will not resize. This function
         * is generally used during rendering when we know that the descriptor heap will not resize.
         *
         * @warning You shouldn't store returned offset for more that 1 frame as it might change after a frame
         * is submitted (because the descriptor heap may resize).
         *
         * @return Error if this descriptor was not allocated from a range or something went wrong, otherwise
         * offset (in descriptors) of the descriptor from range start.
         */
        std::variant<unsigned int, Error> getOffsetFromRangeStartOnCurrentFrame();

        /**
         * Returns heap that this descriptor uses.
         *
         * @return Descriptor heap.
         */
        DirectXDescriptorHeap* getDescriptorHeap() const { return pHeap; }

        /**
         * Returns descriptor range that this descriptor was allocated from.
         *
         * @return `nullptr` if this descriptor was not allocated from a range, otherwise a valid pointer.
         */
        std::shared_ptr<ContinuousDirectXDescriptorRange> getDescriptorRange() const { return pRange; }

        /**
         * Returns resource that owns this descriptor.
         *
         * @return `nullptr` if this descriptor is being destroyed, otherwise owner resource.
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
         * @param referencedCubemapFaceIndex     Specify empty if this descriptor does not reference
         * a cubemap, otherwise index of cubemap's face that it references.
         * @param pRange                         Range that this descriptor was allocated from.
         * `nullptr` if allocated not from a range.
         */
        DirectXDescriptor(
            DirectXDescriptorHeap* pHeap,
            DirectXDescriptorType descriptorType,
            DirectXResource* pResource,
            int iDescriptorOffsetInDescriptors,
            std::optional<size_t> referencedCubemapFaceIndex,
            const std::shared_ptr<ContinuousDirectXDescriptorRange>& pRange = nullptr);

    private:
        /**
         * Offset of this descriptor from the heap start (offset is specified in descriptors,
         * not an actual index).
         */
        int iDescriptorOffsetInDescriptors;

        /** Do not delete. Always valid unless we are in the destructor. Owner resource of this descriptor. */
        DirectXResource* pResource = nullptr;

        /** Do not delete. Heap of this descriptor. */
        DirectXDescriptorHeap* const pHeap = nullptr;

        /** Range that allocated this descriptor (`nullptr` if allocated not from a range). */
        std::shared_ptr<ContinuousDirectXDescriptorRange> const pRange = nullptr;

        /** Not empty if this descriptor references a cubemap's face. */
        const std::optional<size_t> referencedCubemapFaceIndex;

        /** Type of this descriptor. */
        const DirectXDescriptorType descriptorType;
    };
} // namespace ne
