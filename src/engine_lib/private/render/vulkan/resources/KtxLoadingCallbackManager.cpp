#include "KtxLoadingCallbackManager.h"

// Custom.
#include "game/GameManager.h"
#include "game/Window.h"
#include "render/vulkan/resources/VulkanResourceManager.h"
#include "misc/Error.h"
#include "render/vulkan/VulkanRenderer.h"

// External.
#include "vulkan/vk_enum_string_helper.h"

namespace ne {

    // Initialize static field.
    std::pair<std::recursive_mutex, KtxLoadingCallbackManager::Data> KtxLoadingCallbackManager::mtxData{};

    size_t KtxLoadingCallbackManager::getCurrentAllocationCount() {
        std::scoped_lock guard(mtxData.first);
        return mtxData.second.allocations.size();
    }

    ktxVulkanTexture_subAllocatorCallbacks KtxLoadingCallbackManager::getKtxSubAllocatorCallbacks() {
        ktxVulkanTexture_subAllocatorCallbacks subAllocCallbacks;
        subAllocCallbacks.allocMemFuncPtr = allocMem;
        subAllocCallbacks.bindBufferFuncPtr = bindBuffer;
        subAllocCallbacks.bindImageFuncPtr = bindImage;
        subAllocCallbacks.memoryMapFuncPtr = memoryMap;
        subAllocCallbacks.memoryUnmapFuncPtr = memoryUnmap;
        subAllocCallbacks.freeMemFuncPtr = freeMem;

        return subAllocCallbacks;
    }

    uint64_t KtxLoadingCallbackManager::allocMem(
        VkMemoryAllocateInfo* pAllocationInfo,
        VkMemoryRequirements* pMemoryRequirements,
        uint64_t* pPageCount) {
        std::scoped_lock guard(mtxData.first);

        // Find a unique unused allocation ID.
        bool bTryNextAllocationId = true;
        do {
            if (mtxData.second.iAllocationId == 0) {
                // Increment allocation ID to avoid returning 0 as it would mean "out of memory" (this is
                // how our external dependency works).
                mtxData.second.iAllocationId += 1;
                continue;
            }

            const auto it = mtxData.second.allocations.find(mtxData.second.iAllocationId);
            if (it == mtxData.second.allocations.end()) {
                bTryNextAllocationId = false;
            } else {
                mtxData.second.iAllocationId += 1;
            }
        } while (bTryNextAllocationId);

        // Save and increment allocation ID for future allocations.
        const auto iAllocationIdToUse = mtxData.second.iAllocationId;
        mtxData.second.iAllocationId += 1;

        // See whether we need GPU dedicated memory or host visible.

        // Get physical memory properties.
        VkPhysicalDeviceMemoryProperties physicalMemoryProperties;
        vkGetPhysicalDeviceMemoryProperties(getPhysicalDevice(), &physicalMemoryProperties);

        // Make sure we won't access out of bounds.
        if (physicalMemoryProperties.memoryTypeCount <= pAllocationInfo->memoryTypeIndex) [[unlikely]] {
            Error error(std::format(
                "requested memory type index {} is out of bounds, valid range[0; {})",
                pAllocationInfo->memoryTypeIndex,
                physicalMemoryProperties.memoryTypeCount));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Prepare allocation info.
        VmaAllocationCreateInfo allocationInfo = {};
        allocationInfo.memoryTypeBits = pMemoryRequirements->memoryTypeBits;

        // Check memory properties.
        auto bUsingHostVisibleMemory = false;
        if (((physicalMemoryProperties.memoryTypes[pAllocationInfo->memoryTypeIndex].propertyFlags &
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) ||
            ((physicalMemoryProperties.memoryTypes[pAllocationInfo->memoryTypeIndex].propertyFlags &
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0)) {
            // Use host visible memory.
            bUsingHostVisibleMemory = true;
            allocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        } else {
            // Use dedicated memory.
            allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        }

        // Allocate memory.
        const auto pMemoryAllocator = getMemoryAllocator();
        VmaAllocation pAllocation = nullptr;
        const auto result =
            vmaAllocateMemory(pMemoryAllocator, pMemoryRequirements, &allocationInfo, &pAllocation, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(
                std::format("failed to allocate memory for a texture, error: {}", string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Set allocation name.
        vmaSetAllocationName(
            pMemoryAllocator,
            pAllocation,
            std::format(
                "KTX texture allocation #{} {}",
                iAllocationIdToUse,
                bUsingHostVisibleMemory ? "(upload resource)" : "")
                .c_str());

        // Add new allocation to the global map of allocations.
        mtxData.second.allocations[iAllocationIdToUse] = {pAllocation, pMemoryRequirements->size};

        // Update `pageCount`.
        *pPageCount = 1;

        // Self check: make sure we are not returning 0.
        if (iAllocationIdToUse == 0) [[unlikely]] {
            Error error("picked allocation ID of 0 which should not happen");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        return iAllocationIdToUse;
    }

    VkResult KtxLoadingCallbackManager::bindBuffer(VkBuffer pBuffer, uint64_t iAllocationId) {
        std::scoped_lock guard(mtxData.first);

        // Find allocation by the specified ID.
        const auto it = mtxData.second.allocations.find(iAllocationId);
        if (it == mtxData.second.allocations.end()) [[unlikely]] {
            Error error(std::format("failed to find allocation by ID {}", iAllocationId));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Bind buffer.
        const auto result = vmaBindBufferMemory(getMemoryAllocator(), it->second.first, pBuffer);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(std::format("failed to bind buffer memory, error: {}", string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        return VK_SUCCESS;
    }

    VkResult KtxLoadingCallbackManager::bindImage(VkImage pImage, uint64_t iAllocationId) {
        std::scoped_lock guard(mtxData.first);

        // Find allocation by the specified ID.
        const auto it = mtxData.second.allocations.find(iAllocationId);
        if (it == mtxData.second.allocations.end()) [[unlikely]] {
            Error error(std::format("failed to find allocation by ID {}", iAllocationId));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Bind image.
        const auto result = vmaBindImageMemory(getMemoryAllocator(), it->second.first, pImage);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(std::format("failed to bind image memory, error: {}", string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        return VK_SUCCESS;
    }

    VkResult KtxLoadingCallbackManager::memoryMap(
        uint64_t iAllocationId, uint64_t iPageNumber, VkDeviceSize* pMapLength, void** pData) {
        std::scoped_lock guard(mtxData.first);

        // Find allocation by the specified ID.
        const auto it = mtxData.second.allocations.find(iAllocationId);
        if (it == mtxData.second.allocations.end()) [[unlikely]] {
            Error error(std::format("failed to find allocation by ID {}", iAllocationId));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Specify map size.
        *pMapLength = it->second.second;

        // Map memory.
        const auto result = vmaMapMemory(getMemoryAllocator(), it->second.first, pData);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(std::format("failed to map memory, error: {}", string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        return VK_SUCCESS;
    }

    void KtxLoadingCallbackManager::memoryUnmap(uint64_t iAllocationId, uint64_t iPageNumber) {
        std::scoped_lock guard(mtxData.first);

        // Find allocation by the specified ID.
        const auto it = mtxData.second.allocations.find(iAllocationId);
        if (it == mtxData.second.allocations.end()) [[unlikely]] {
            Error error(std::format("failed to find allocation by ID {}", iAllocationId));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        vmaUnmapMemory(getMemoryAllocator(), it->second.first);
    }

    void KtxLoadingCallbackManager::freeMem(uint64_t iAllocationId) {
        std::scoped_lock guard(mtxData.first);

        // Find allocation by the specified ID.
        const auto it = mtxData.second.allocations.find(iAllocationId);
        if (it == mtxData.second.allocations.end()) [[unlikely]] {
            Error error(std::format("failed to find allocation by ID {}", iAllocationId));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Free memory.
        vmaFreeMemory(getMemoryAllocator(), it->second.first);

        // Remove pointer from map of all allocations.
        mtxData.second.allocations.erase(it);
    }

    VmaAllocator KtxLoadingCallbackManager::getMemoryAllocator() {
        return reinterpret_cast<VulkanResourceManager*>(
                   GameManager::get()->getWindow()->getRenderer()->getResourceManager())
            ->pMemoryAllocator;
    }

    VkPhysicalDevice KtxLoadingCallbackManager::getPhysicalDevice() {
        return reinterpret_cast<VulkanRenderer*>(GameManager::get()->getWindow()->getRenderer())
            ->getPhysicalDevice();
    }

}
