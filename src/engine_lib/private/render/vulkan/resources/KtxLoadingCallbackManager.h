#pragma once

// Standard.
#include <mutex>
#include <unordered_map>
#include <format>

// External.
#include "VulkanMemoryAllocator/include/vk_mem_alloc.h"
#include "ktxvulkan.h"

namespace ne {
    /**
     * Provides static memory-related callbacks for KTX-Software (external dependency) because it does not
     * support our memory allocator out of the box.
     */
    class KtxLoadingCallbackManager {
    public:
        /**
         * Returns the current amount of active (not freed) allocations.
         *
         * @return The number of current allocations.
         */
        static size_t getCurrentAllocationCount();

        /**
         * Returns callbacks used with KTX images for KTX-Software (external dependency).
         *
         * @return Callbacks.
         */
        static ktxVulkanTexture_subAllocatorCallbacks getKtxSubAllocatorCallbacks();

        /**
         * KTX loader (external dependency) callback.
         *
         * @param pAllocationInfo
         * @param pMemoryRequirements
         * @param pPageCount
         *
         * @return Allocation ID.
         */
        static uint64_t allocMem(
            VkMemoryAllocateInfo* pAllocationInfo,
            VkMemoryRequirements* pMemoryRequirements,
            uint64_t* pPageCount);

        /**
         * KTX loader (external dependency) callback.
         *
         * @param pBuffer
         * @param iAllocationId
         *
         * @return Result of the operation.
         */
        static VkResult bindBuffer(VkBuffer pBuffer, uint64_t iAllocationId);

        /**
         * KTX loader (external dependency) callback.
         *
         * @param pImage
         * @param iAllocationId
         *
         * @return Result of the operation.
         */
        static VkResult bindImage(VkImage pImage, uint64_t iAllocationId);

        /**
         * KTX loader (external dependency) callback.
         *
         * @param iAllocationId
         * @param iPageNumber
         * @param pMapLength
         * @param pData
         *
         * @return Result of the operation.
         */
        static VkResult
        memoryMap(uint64_t iAllocationId, uint64_t iPageNumber, VkDeviceSize* pMapLength, void** pData);

        /**
         * KTX loader (external dependency) callback.
         *
         * @param iAllocationId
         * @param iPageNumber
         */
        static void memoryUnmap(uint64_t iAllocationId, uint64_t iPageNumber);

        /**
         * KTX loader (external dependency) callback.
         *
         * @param iAllocationId
         */
        static void freeMem(uint64_t iAllocationId);

    private:
        /** Groups internal data. */
        struct Data {
            /**
             * Stores pairs of "allocation ID" - "{allocation, map size}" of all currently active (not-freed)
             * allocations.
             */
            std::unordered_map<uint64_t, std::pair<VmaAllocation, VkDeviceSize>> allocations;

            /** Allocation ID that you can attempt to use on your new allocation for @ref allocations. */
            uint64_t iAllocationId = 0;
        };

        /**
         * Returns memory allocator of resource manager.
         *
         * @return Memory allocator.
         */
        static VmaAllocator getMemoryAllocator();

        /**
         * Returns renderer's physical device.
         *
         * @return Physical device.
         */
        static VkPhysicalDevice getPhysicalDevice();

        /** Internal data. */
        static std::pair<std::recursive_mutex, Data> mtxData;
    };
} // namespace ne
