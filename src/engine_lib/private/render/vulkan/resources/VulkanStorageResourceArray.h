#pragma once

// Standard.
#include <variant>
#include <memory>
#include <queue>
#include <mutex>
#include <optional>
#include <unordered_set>

// Custom.
#include "misc/Error.h"
#include "render/general/resources/UploadBuffer.h"

// External.
#include "vulkan/vulkan.h"

namespace ne {
    class GpuResourceManager;
    class VulkanStorageResourceArray;
    class GlslShaderCpuWriteResource;
    class VulkanRenderer;

    /**
     * Represents a used slot (place) in the array.
     *
     * @remark Automatically notifies the array to free the slot (mark as unused) in destructor.
     */
    class VulkanStorageResourceArraySlot {
        // Only arrays are allowed to create and modify the index.
        friend class VulkanStorageResourceArray;

    public:
        VulkanStorageResourceArraySlot() = delete;
        VulkanStorageResourceArraySlot(const VulkanStorageResourceArraySlot&) = delete;
        VulkanStorageResourceArraySlot& operator=(const VulkanStorageResourceArraySlot&) = delete;

        ~VulkanStorageResourceArraySlot();

        /**
         * Copies the specified data to slot's data.
         *
         * @param pData Data to copy.
         */
        void updateData(void* pData);

    private:
        /**
         * Initializes the slot.
         *
         * @param pArray          Array in which the slot resides.
         * @param iIndexInArray   Into into the array to access the slot's data.
         * @param pShaderResource Shader resource that uses this slot.
         */
        VulkanStorageResourceArraySlot(
            VulkanStorageResourceArray* pArray,
            size_t iIndexInArray,
            GlslShaderCpuWriteResource* pShaderResource);

        /** Array in which the slot resides. */
        VulkanStorageResourceArray* pArray = nullptr;

        /** Shader resource that uses this slot. */
        GlslShaderCpuWriteResource* pShaderResource = nullptr;

        /**
         * Index into @ref pArray to access the slot's data.
         *
         * @remark Updated by array when it's resizing.
         */
        size_t iIndexInArray = 0;
    };

    /**
     * Dynamic array. Allows storing elements of the same size in one storage buffer.
     *
     * @remark Dynamically grows and shrinks when adding/removing elements from the array.
     */
    class VulkanStorageResourceArray {
        // Only slots can mark slots as unused.
        friend class VulkanStorageResourceArraySlot;

        // Only manager can create and manage storage arrays.
        friend class VulkanStorageResourceArrayManager;

    public:
        VulkanStorageResourceArray() = delete;
        VulkanStorageResourceArray(const VulkanStorageResourceArray&) = delete;
        VulkanStorageResourceArray& operator=(const VulkanStorageResourceArray&) = delete;

        ~VulkanStorageResourceArray();

        /**
         * Returns the actual number of elements in the array
         * (smaller or equal to @ref getCapacity).
         *
         * @return The actual number of elements in the array.
         */
        size_t getSize();

        /**
         * Returns the maximum number of elements that could be added to the array without
         * expanding (recreating with a bigger size) the storage buffer.
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

    private:
        /** Array's internal resources. */
        struct InternalResources {
            /** CPU visible storage buffer that stores all elements. */
            std::unique_ptr<UploadBuffer> pStorageBuffer = nullptr;

            /**
             * The maximum number of elements that could be added to the array without
             * expanding (recreating with a bigger size) the storage buffer.
             */
            size_t iCapacity = 0;

            /** The actual number of elements in the array (smaller or equal to @ref iCapacity). */
            size_t iSize = 0;

            /**
             * Index of the next free place in the array.
             * Each new element inserted in the array will fetch this value (to be used) and increment it.
             * Once this value is equal to @ref iCapacity we will use @ref noLongerUsedArrayIndices
             * to see if any old indices are no longer being used.
             */
            size_t iNextFreeArrayIndex = 0;

            /** Indices in the array that were previously used (inserted) but now erased and are free. */
            std::queue<size_t> noLongerUsedArrayIndices;

            /**
             * Set of slots that were inserted (equal to @ref iSize).
             *
             * Storing a raw pointer here because it's only used to update
             * slot's index if the array was resized. Before the slot
             * is destroyed it will be automatically removed from this set (see slot's destructor).
             */
            std::unordered_set<VulkanStorageResourceArraySlot*> activeSlots;
        };

        /**
         * Creates a new array.
         *
         * @param pResourceManager     Resource manager that will allocate storage buffers.
         * @param sHandledResourceName Name of the shader resource this array handles. It will be used
         * to update descriptors in all pipelines once the array is resized (recreated) to make descriptors
         * reference a new VkBuffer.
         * @param iElementSizeInBytes  Size (in bytes) of one element in the array.
         * @param iCapacityStepSizeMultiplier Specify value bigger than 1 if you plan to store
         * multiple copies of each data for different frame resources (frames in-flight). Resulting
         * capacity step size will be multiplied by this value. Should be bigger than 0 and smaller
         * or equal to the number of frame in-flight.
         *
         * @return Error if something went wrong, otherwise created array.
         */
        static std::variant<std::unique_ptr<VulkanStorageResourceArray>, Error> create(
            GpuResourceManager* pResourceManager,
            const std::string& sHandledResourceName,
            size_t iElementSizeInBytes,
            size_t iCapacityStepSizeMultiplier = 1);

        /**
         * Creates initialized array.
         *
         * @remark Only used internally, for creating new arrays use @ref create.
         *
         * @param pResourceManager     Resource manager that will allocate storage buffers.
         * @param sHandledResourceName Name of the shader resource this array handles. It will be used
         * to update descriptors in all pipelines once the array is resized (recreated) to make descriptors
         * reference a new VkBuffer.
         * @param iElementSizeInBytes Size (in bytes) of one element in the array.
         * @param iCapacityStepSize   Capacity step size to use. Expects it to be even and not zero.
         */
        VulkanStorageResourceArray(
            GpuResourceManager* pResourceManager,
            const std::string& sHandledResourceName,
            size_t iElementSizeInBytes,
            size_t iCapacityStepSize);

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
         * @param iElementSizeInBytes         Size (in bytes) of one element in the array.
         * @param iCapacityStepSizeMultiplier Multiplier for calculated capacity step size.
         * Should be in range [1; FrameResourcesManager::getFrameResourcesCount()].
         *
         * @return Error if something went wrong, otherwise capacity step size to add/remove
         * this value when expanding/shrinking the array.
         */
        static std::variant<size_t, Error>
        calculateCapacityStepSize(size_t iElementSizeInBytes, size_t iCapacityStepSizeMultiplier = 1);

        /**
         * Inserts a new element in the array.
         *
         * @remark There is no public `erase` function because slot destruction automatically
         * uses internal `erase`, see documentation on the returned slot object.
         *
         * @remark Note that one shader resource can use multiple slots.
         *
         * @param pShaderResource Shader resource that requires a slot in the array.
         * If the array will resize the specified resource (if it has an active slot in the array)
         * will be marked as "needs update" through the shader resource manager.
         *
         * @return Error if something went wrong, otherwise slot of the newly added element in
         * the array.
         */
        std::variant<std::unique_ptr<VulkanStorageResourceArraySlot>, Error>
        insert(GlslShaderCpuWriteResource* pShaderResource);

        /**
         * Called by slots in their destructors to notify the array that the slot can be reused.
         *
         * @param pSlot Slot that can be reused now.
         */
        void markSlotAsNoLongerBeingUsed(VulkanStorageResourceArraySlot* pSlot);

        /**
         * Called by slots to update their data.
         *
         * @param pSlot Slot that needs to be updated.
         * @param pData Data to copy to slot.
         */
        void updateSlotData(VulkanStorageResourceArraySlot* pSlot, void* pData);

        /**
         * (Re)creates the internal storage buffer with the specified capacity.
         *
         * @remark Indices in the currently active slots (created with @ref insert) will be updated
         * to reference new indices in the array.
         *
         * @remark Only updates the buffer resource, internal capacity and updates all active slots
         * (if any), other internal values are not changed and should be corrected afterwards.
         *
         * @param iCapacity Size of the array (in elements).
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

        /**
         * Updates descriptors in all pipelines to make descriptors reference the current underlying VkBuffer.
         *
         * @warning Expects that the GPU is not doing any work and that no new frames are being submitted now.
         *
         * @remark Generally called inside @ref createArray after the underlying VkBuffer is changed.
         *
         * @param pVulkanRenderer Vulkan renderer.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> updateDescriptors(VulkanRenderer* pVulkanRenderer);

        /**
         * Internal resources of the array.
         *
         * @remark Must be access only when the mutex is locked.
         */
        std::pair<std::recursive_mutex, InternalResources> mtxInternalResources;

        /** Allocates new storage buffers. */
        GpuResourceManager* pResourceManager = nullptr;

        /**
         * Capacity to add to the new (expanded) storage buffer when there is no more free space left
         * in the current storage buffer or capacity to remove from the new (shrinked) storage
         * buffer when shrinking.
         */
        const size_t iCapacityStepSize = 0;

        /** Name of the resource this array handles. */
        const std::string sHandledResourceName;

        /** Size in bytes of one element in the array. */
        const size_t iElementSizeInBytes = 0;
    };
} // namespace ne
