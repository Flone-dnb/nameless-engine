#pragma once

// Standard.
#include <string>
#include <stdexcept>
#include <format>
#include <queue>
#include <unordered_set>
#include <mutex>
#include <variant>
#include <memory>
#include <optional>

// Custom.
#include "render/general/resources/UploadBuffer.h"
#include "misc/Error.h"

namespace ne {
    class GpuResourceManager;
    class ShaderCpuWriteResource;
    class VulkanRenderer;
    class VulkanPipeline;

    /**
     * Represents a used slot (place) in a shader resource array.
     *
     * @remark Automatically notifies the array to free the slot (mark as unused) in destructor.
     */
    class DynamicCpuWriteShaderResourceArraySlot {
        // Only arrays are allowed to create and modify the internal index of this slot.
        friend class DynamicCpuWriteShaderResourceArray;

    public:
        DynamicCpuWriteShaderResourceArraySlot() = delete;

        DynamicCpuWriteShaderResourceArraySlot(const DynamicCpuWriteShaderResourceArraySlot&) = delete;
        DynamicCpuWriteShaderResourceArraySlot&
        operator=(const DynamicCpuWriteShaderResourceArraySlot&) = delete;

        ~DynamicCpuWriteShaderResourceArraySlot();

        /**
         * Copies the specified data to slot's memory.
         *
         * @warning Should only be called when shader resource manager tells that it's the time
         * to update shader resource data.
         *
         * @param pData Data to copy.
         */
        void updateData(void* pData);

        /**
         * Returns index into the array (that owns this slot) to access the slot's data.
         *
         * @return Index into the array.
         */
        inline unsigned int getIndexIntoArray() const { return iIndexInArray; }

    private:
        /**
         * Initializes the slot.
         *
         * @param pArray          Array in which the slot resides.
         * @param iIndexInArray   Into into the array to access the slot's data.
         * @param pShaderResource Shader resource that uses this slot.
         */
        DynamicCpuWriteShaderResourceArraySlot(
            DynamicCpuWriteShaderResourceArray* pArray,
            size_t iIndexInArray,
            ShaderCpuWriteResource* pShaderResource);

        /**
         * Should be called by array to update the index.
         *
         * @param iNewIndex New index.
         */
        inline void updateIndex(size_t iNewIndex) {
            // Self check:
            static_assert(
                std::is_same_v<decltype(this->iIndexInArray), unsigned int>,
                "update cast and type limit check below");

            // Check before converting to unsigned int, see slot index docs for more info.
            if (iNewIndex > UINT_MAX) [[unlikely]] {
                Error error(std::format("received slot index {} exceeds type limit", iNewIndex));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            iIndexInArray = static_cast<unsigned int>(iNewIndex);
        }

        /** Array in which the slot resides. */
        DynamicCpuWriteShaderResourceArray* const pArray = nullptr;

        /** Shader resource that uses this slot. */
        ShaderCpuWriteResource* const pShaderResource = nullptr;

        /**
         * Index into @ref pArray to access the slot's data.
         *
         * @remark Updated by array when it's resizing (use @ref updateIndex).
         *
         * @remark Using `unsigned int` because it will be copied to root/push constants which store `uint`s.
         */
        unsigned int iIndexInArray = 0;
    };

    /**
     * Allows storing elements of the same size in one GPU buffer.
     *
     * @remark Automatically handles binding the array to the specified shader resource.
     *
     * @remark Dynamically grows and shrinks when adding/removing elements from the array.
     */
    class DynamicCpuWriteShaderResourceArray {
        // Slots notify the array in their destructor.
        friend class DynamicCpuWriteShaderResourceArraySlot;

        // Only manager should create the arrays and insert new slots.
        friend class DynamicCpuWriteShaderResourceArrayManager;

    public:
        /** Groups mutex-guarded internal resources. */
        struct InternalResources {
            InternalResources() = default;

            /** CPU visible GPU buffer that stores all elements. */
            std::unique_ptr<UploadBuffer> pUploadBuffer = nullptr;

            /**
             * The maximum number of elements that could be added to the array without
             * expanding (recreating with a bigger size) the GPU buffer.
             */
            size_t iCapacity = 0;

            /**
             * Index of the next free place in the array.
             * Each new element inserted in the array will fetch this value (to be used) and increment it.
             * Once this value is equal to @ref iCapacity we will use @ref noLongerUsedArrayIndices
             * to see if any old indices are no longer being used.
             */
            size_t iNextFreeArrayIndex = 0;

            /** Indices in the array that were previously used but now unused. */
            std::queue<size_t> noLongerUsedArrayIndices;

            /**
             * Set of slots that were inserted (size of this set is the actual number of elements in the array
             * (smaller or equal to @ref iCapacity)).
             *
             * Storing a raw pointer here is safe because it's only used to update slot's index if the array
             * was resized. Before the slot is destroyed it will be automatically removed from this set (see
             * slot's destructor).
             */
            std::unordered_set<DynamicCpuWriteShaderResourceArraySlot*> activeSlots;
        };

        DynamicCpuWriteShaderResourceArray() = delete;
        DynamicCpuWriteShaderResourceArray(const DynamicCpuWriteShaderResourceArray&) = delete;
        DynamicCpuWriteShaderResourceArray& operator=(const DynamicCpuWriteShaderResourceArray&) = delete;

        /**
         * Returns the actual number of elements in the array
         * (smaller or equal to @ref getCapacity).
         *
         * @return The actual number of elements in the array.
         */
        size_t getSize();

        /**
         * Returns the maximum number of elements that could be added to the array without
         * expanding (recreating with a bigger size) the buffer.
         *
         * @return The maximum possible number of elements without expanding.
         */
        size_t getCapacity();

        /**
         * Returns size in bytes that this array takes up.
         *
         * @return Size of array in bytes.
         */
        size_t getSizeInBytes();

        /**
         * Returns size (in bytes) of one element in the array.
         *
         * @return Size in bytes of one element.
         */
        size_t getElementSize() const;

        /**
         * Returns the capacity to add to the new (expanded) array when there is no more
         * free space left in the current array or capacity to remove from the new
         * (shrinked) array when shrinking.
         *
         * @return Capacity step size.
         */
        size_t getCapacityStepSize() const;

        /**
         * Returns name of the shader resource (from shader code) this array handles.
         *
         * @return Shader resource name.
         */
        std::string_view getHandledShaderResourceName() const;

        /**
         * Returns internal resources.
         *
         * @remark Generally used by automated tests.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @return Internal resources.
         */
        std::pair<std::recursive_mutex, InternalResources>* getInternalResources();

    private:
        /**
         * Creates a new initialized array.
         *
         * @param pResourceManager           Resource manager that will be used to allocate GPU buffers.
         * @param sHandledShaderResourceName Name of the shader resource this array handles. It will be used
         * to bind the array to pipelines.
         * @param iElementSizeInBytes        Size (in bytes) of one element in the array.
         *
         * @return Error if something went wrong, otherwise created array.
         */
        static std::variant<std::unique_ptr<DynamicCpuWriteShaderResourceArray>, Error> create(
            GpuResourceManager* pResourceManager,
            const std::string& sHandledShaderResourceName,
            size_t iElementSizeInBytes);

        /**
         * Formats the specified size in bytes to the following format: "<number> KB",
         * for example the number 1512 will be formatted to the following text: "1.47 KB".
         *
         * @param iSizeInBytes Size in bytes to format.
         *
         * @return Formatted text.
         */
        static std::string formatBytesToKilobytes(size_t iSizeInBytes);

        /**
         * Calculates array capacity step size depending on the size of the elements in the array.
         *
         * @param iElementSizeInBytes Size (in bytes) of one element in the array.
         *
         * @return Error if something went wrong, otherwise capacity step size to add/remove
         * this value when expanding/shrinking the array.
         */
        static std::variant<size_t, Error> calculateCapacityStepSize(size_t iElementSizeInBytes);

        /**
         * Creates initialized array.
         *
         * @remark Only used internally, for creating new arrays use @ref create.
         *
         * @param pResourceManager           Resource manager that will allocate GPU buffers.
         * @param sHandledShaderResourceName Name of the shader resource this array handles. It will be used
         * to bind the array to pipelines.
         * @param iElementSizeInBytes        Size (in bytes) of one element in the array.
         * @param iCapacityStepSize          Capacity step size to use. Expecting it to be even and not zero.
         */
        DynamicCpuWriteShaderResourceArray(
            GpuResourceManager* pResourceManager,
            const std::string& sHandledShaderResourceName,
            size_t iElementSizeInBytes,
            size_t iCapacityStepSize);

        /**
         * Inserts a new element in the array.
         *
         * @remark There is no `erase` function because slot destruction automatically
         * uses internal `erase`, see documentation on the returned slot object.
         *
         * @param pShaderResource Shader resource that requires a slot in the array.
         * If the array will resize the specified resource (if it has an active slot in the array)
         * will be marked as "needs update" through the shader resource manager.
         *
         * @return Error if something went wrong, otherwise slot of the newly added element in
         * the array.
         */
        std::variant<std::unique_ptr<DynamicCpuWriteShaderResourceArraySlot>, Error>
        insert(ShaderCpuWriteResource* pShaderResource);

        /**
         * Called by slots in their destructors to notify the array that the slot can be reused.
         *
         * @param pSlot Slot that can be reused now.
         */
        void markSlotAsNoLongerBeingUsed(DynamicCpuWriteShaderResourceArraySlot* pSlot);

        /**
         * Called by slots to update their data.
         *
         * @param pSlot Slot that needs to be updated.
         * @param pData Data to copy to slot.
         */
        void updateSlotData(DynamicCpuWriteShaderResourceArraySlot* pSlot, void* pData);

        /**
         * (Re)creates the internal GPU buffer with the specified capacity.
         *
         * @remark Indices in the currently active slots (created with @ref insert) will be updated
         * to reference new indices in the array.
         *
         * @remark Only updates the GPU buffer, internal capacity and all active slots
         * (if any), other internal values are not changed and should be corrected afterwards if needed.
         *
         * @param iCapacity Size of the array (in elements, not bytes).
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createArray(size_t iCapacity);

        /**
         * Recreates the array to expand its capacity to support @ref iCapacityStepSize more elements.
         *
         * @remark All internal values and active slots will be updated.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> expandArray();

        /**
         * Recreates the array to shrink its capacity to support @ref iCapacityStepSize less elements.
         *
         * @remark All internal values and active slots will be updated.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> shrinkArray();

        /** Internal resources of the array. */
        std::pair<std::recursive_mutex, InternalResources> mtxInternalResources;

        /** Used to allocate GPU buffers. */
        GpuResourceManager* const pResourceManager = nullptr;

        /**
         * Used for 2 things:
         * 1. Capacity to add for the new (expanded) GPU buffer when there is no more free space left
         * in the current GPU buffer
         * 2. Capacity to remove for the new (shrinked) GPU buffer when shrinking.
         */
        const size_t iCapacityStepSize = 0;

        /** Name of the shader resource (from shader code) this array handles. */
        const std::string sHandledShaderResourceName;

        /** Size in bytes of one element in the array. */
        const size_t iElementSizeInBytes = 0;
    };
}
