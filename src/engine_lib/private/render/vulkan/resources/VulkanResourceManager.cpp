#include "VulkanResourceManager.h"

// Custom.
#include "render/vulkan/VulkanRenderer.h"
#include "io/Logger.h"
#include "render/vulkan/resources/VulkanResource.h"

// External.
#include "fmt/core.h"
#define VMA_IMPLEMENTATION // define in only one .cpp file
#include "VulkanMemoryAllocator/include/vk_mem_alloc.h"
#include "vulkan/vk_enum_string_helper.h"

namespace ne {

    VulkanResourceManager::VulkanResourceManager(VulkanRenderer* pRenderer, VmaAllocator pMemoryAllocator) {
        this->pRenderer = pRenderer;
        this->pMemoryAllocator = pMemoryAllocator;
    }

    std::variant<std::unique_ptr<VulkanResource>, Error> VulkanResourceManager::createBuffer(
        const std::string& sResourceName,
        VkDeviceSize iBufferSize,
        VkBufferUsageFlags bufferUsage,
        bool bAllowCpuAccess) {
        // Describe buffer.
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = iBufferSize;
        bufferInfo.usage = bufferUsage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Prepare allocation info.
        VmaAllocationCreateInfo allocationCreateInfo{};
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        if (bAllowCpuAccess) {
            allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            allocationCreateInfo.requiredFlags =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        } else {
            allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        // Create resource.
        auto result = createResource(sResourceName, bufferInfo, allocationCreateInfo);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }

        return std::get<std::unique_ptr<VulkanResource>>(std::move(result));
    }

    std::optional<VkBufferUsageFlagBits>
    VulkanResourceManager::convertResourceUsageTypeToVkBufferUsageType(ResourceUsageType usage) {
        switch (usage) {
        case (ResourceUsageType::VERTEX_BUFFER): {
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            break;
        }
        case (ResourceUsageType::INDEX_BUFFER): {
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;
        }
        case (ResourceUsageType::OTHER): {
            return {};
            break;
        }
        }

        Error error("unhandled case");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

    VulkanResourceManager::~VulkanResourceManager() {
        // Make sure there are no resources exist.
        const auto iTotalAliveResourceCount = iAliveResourceCount.load();
        if (iTotalAliveResourceCount != 0) [[unlikely]] {
            Logger::get().error(fmt::format(
                "Vulkan resource manager is being destroyed but there are "
                "still {} resource(s) alive",
                iTotalAliveResourceCount));
        }

        vmaDestroyAllocator(pMemoryAllocator);
        pMemoryAllocator = nullptr;
    }

    std::variant<std::unique_ptr<VulkanResourceManager>, Error>
    VulkanResourceManager::create(VulkanRenderer* pRenderer) {
        const auto pLogicalDevice = pRenderer->getLogicalDevice();
        const auto pPhysicalDevice = pRenderer->getPhysicalDevice();
        const auto pInstance = pRenderer->getInstance();

        // Make sure logical device is created.
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("expected logical device to be created at this point");
        }

        // Make sure physical device is created.
        if (pPhysicalDevice == nullptr) [[unlikely]] {
            return Error("expected physical device to be created at this point");
        }

        // Make sure instance is created.
        if (pInstance == nullptr) [[unlikely]] {
            return Error("expected Vulkan instance to be created at this point");
        }

        // Prepare to create memory allocator.
        VmaAllocatorCreateInfo createInfo{};
        createInfo.device = pLogicalDevice;
        createInfo.physicalDevice = pPhysicalDevice;
        createInfo.instance = pInstance;
        createInfo.vulkanApiVersion = VulkanRenderer::getUsedVulkanVersion();

        // Create memory allocator.
        VmaAllocator pMemoryAllocator = nullptr;
        const auto result = vmaCreateAllocator(&createInfo, &pMemoryAllocator);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(
                fmt::format("failed to create memory allocator, error: {}", string_VkResult(result)));
        }

        return std::unique_ptr<VulkanResourceManager>(new VulkanResourceManager(pRenderer, pMemoryAllocator));
    }

    size_t VulkanResourceManager::getTotalVideoMemoryInMb() const {
        // Get physical device.
        const auto pPhysicalDevice = pRenderer->getPhysicalDevice();

        // Make sure physical device is created.
        if (pPhysicalDevice == nullptr) [[unlikely]] {
            Error error("expected physical device to be created at this point");
            Logger::get().error(error.getFullErrorMessage());
            return 0;
        }

        // Get budget statistics.
        VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
        vmaGetHeapBudgets(pMemoryAllocator, budgets);

        // Get supported heap types.
        VkPhysicalDeviceMemoryProperties physicalMemoryProperties;
        vkGetPhysicalDeviceMemoryProperties(pPhysicalDevice, &physicalMemoryProperties);

        // Find a heap with a DEVICE_LOCAL bit.
        for (size_t i = 0; i < physicalMemoryProperties.memoryHeapCount; i++) {
            if ((physicalMemoryProperties.memoryHeaps[i].flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
                // Found device local heap.
                return physicalMemoryProperties.memoryHeaps[i].size / 1024 / 1024; // NOLINT
            }
        }

        Logger::get().error("failed to find a memory heap with `DEVICE_LOCAL` bit");
        return 0;
    }

    size_t VulkanResourceManager::getUsedVideoMemoryInMb() const {
        // Get physical device.
        const auto pPhysicalDevice = pRenderer->getPhysicalDevice();

        // Make sure physical device is created.
        if (pPhysicalDevice == nullptr) [[unlikely]] {
            Error error("expected physical device to be created at this point");
            Logger::get().error(error.getFullErrorMessage());
            return 0;
        }

        // Get budget statistics.
        VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
        vmaGetHeapBudgets(pMemoryAllocator, budgets);

        // Get supported heap types.
        VkPhysicalDeviceMemoryProperties physicalMemoryProperties;
        vkGetPhysicalDeviceMemoryProperties(pPhysicalDevice, &physicalMemoryProperties);

        // Find a heap with a DEVICE_LOCAL bit.
        for (size_t i = 0; i < physicalMemoryProperties.memoryHeapCount; i++) {
            if ((physicalMemoryProperties.memoryHeaps[i].flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
                // Found device local heap.
                return static_cast<size_t>(budgets[i].usage) / 1024 / 1024; // NOLINT
            }
        }

        Logger::get().error("failed to find a memory heap with `DEVICE_LOCAL` bit");
        return 0;
    }

    std::variant<std::unique_ptr<VulkanResource>, Error> VulkanResourceManager::createResource(
        const std::string& sResourceName,
        const VkBufferCreateInfo& bufferInfo,
        const VmaAllocationCreateInfo& allocationInfo) {
        auto result =
            VulkanResource::create(this, sResourceName, pMemoryAllocator, bufferInfo, allocationInfo);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }
        return std::get<std::unique_ptr<VulkanResource>>(std::move(result));
    }

    std::variant<std::unique_ptr<UploadBuffer>, Error> VulkanResourceManager::createResourceWithCpuAccess(
        const std::string& sResourceName,
        size_t iElementSizeInBytes,
        size_t iElementCount,
        bool bIsUsedInShadersAsReadOnlyData) {
        const size_t iBufferSizeInBytes = iElementSizeInBytes * iElementCount;
        auto usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        if (bIsUsedInShadersAsReadOnlyData) {
            usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

            // Make sure the physical device is valid.
            const auto pPhysicalDevice = pRenderer->getPhysicalDevice();
            if (pPhysicalDevice == nullptr) [[unlikely]] {
                return Error("expected physical device to be valid");
            }

            // Get GPU limits.
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(pPhysicalDevice, &deviceProperties);

            // Check if the requested buffer size exceed UBO size limit.
            if (iBufferSizeInBytes > deviceProperties.limits.maxUniformBufferRange) {
                return Error(fmt::format(
                    "unable to create the requested uniform buffer with the size {} bytes because the GPU "
                    "limit for uniform buffer sizes is {} bytes",
                    iBufferSizeInBytes,
                    deviceProperties.limits.maxUniformBufferRange));
            }
        }

        // Create buffer.
        auto result = createBuffer(sResourceName, iBufferSizeInBytes, usage, true);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }
        auto pResource = std::get<std::unique_ptr<VulkanResource>>(std::move(result));

        return std::unique_ptr<UploadBuffer>(
            new UploadBuffer(std::move(pResource), iElementSizeInBytes, iElementCount));
    }

    std::variant<std::unique_ptr<UploadBuffer>, Error>
    VulkanResourceManager::createStorageBufferWithCpuAccess(
        const std::string& sResourceName, size_t iElementSizeInBytes, size_t iElementCount) {
        // Create buffer.
        auto result = createBuffer(
            sResourceName, iElementSizeInBytes * iElementCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }
        auto pResource = std::get<std::unique_ptr<VulkanResource>>(std::move(result));

        return std::unique_ptr<UploadBuffer>(
            new UploadBuffer(std::move(pResource), iElementSizeInBytes, iElementCount));
    }

    std::variant<std::unique_ptr<GpuResource>, Error> VulkanResourceManager::createResourceWithData(
        const std::string& sResourceName,
        const void* pBufferData,
        size_t iDataSizeInBytes,
        ResourceUsageType usage,
        bool bIsShaderReadWriteResource) {
        // Create an upload resource for uploading data.
        auto uploadResourceResult = createResourceWithCpuAccess(sResourceName, iDataSizeInBytes, 1, false);
        if (std::holds_alternative<Error>(uploadResourceResult)) {
            auto error = std::get<Error>(std::move(uploadResourceResult));
            error.addEntry();
            return error;
        }
        auto pUploadResource = std::get<std::unique_ptr<UploadBuffer>>(std::move(uploadResourceResult));

        // Copy data to the allocated upload resource memory.
        pUploadResource->copyDataToElement(0, pBufferData, iDataSizeInBytes);

        // Prepare resource usage flags.
        const auto optionalResourceUsage = convertResourceUsageTypeToVkBufferUsageType(usage);
        const auto resourceUsage = optionalResourceUsage.has_value()
                                       ? VK_BUFFER_USAGE_TRANSFER_DST_BIT | optionalResourceUsage.value()
                                       : VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        // Create the final GPU resource to copy the data to.
        auto finalResourceResult = createBuffer(sResourceName, iDataSizeInBytes, resourceUsage, false);
        if (std::holds_alternative<Error>(finalResourceResult)) {
            auto error = std::get<Error>(std::move(finalResourceResult));
            error.addEntry();
            return error;
        }
        auto pFinalResource = std::get<std::unique_ptr<VulkanResource>>(std::move(finalResourceResult));

        // Create one-time submit command buffer to copy data from the upload resource to
        // the final resource.
        auto commandBufferResult = pRenderer->createOneTimeSubmitCommandBuffer();
        if (std::holds_alternative<Error>(commandBufferResult)) {
            auto error = std::get<Error>(std::move(commandBufferResult));
            error.addEntry();
            return error;
        }
        const auto pOneTimeSubmitCommandBuffer = std::get<VkCommandBuffer>(commandBufferResult);

        // Cast upload resource to Vulkan type.
        const auto pVkUploadResource = dynamic_cast<VulkanResource*>(pUploadResource->getInternalResource());
        if (pVkUploadResource == nullptr) [[unlikely]] {
            return Error("expected created upload resource to be a Vulkan resource");
        }

        // Record a copy command.
        VkBufferCopy copyRegion{};
        copyRegion.size = iDataSizeInBytes;
        vkCmdCopyBuffer(
            pOneTimeSubmitCommandBuffer,
            pVkUploadResource->getInternalResource(),
            pFinalResource->getInternalResource(),
            1,
            &copyRegion);

        // Submit command buffer.
        auto optionalError =
            pRenderer->submitWaitDestroyOneTimeSubmitCommandBuffer(pOneTimeSubmitCommandBuffer);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        return pFinalResource;
    }

} // namespace ne
