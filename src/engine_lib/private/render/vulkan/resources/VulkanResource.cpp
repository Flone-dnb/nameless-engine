#include "VulkanResource.h"

// Standard.
#include <stdexcept>

// Custom.
#include "render/vulkan/resources/VulkanResourceManager.h"
#include "io/Logger.h"
#include "render/vulkan/VulkanRenderer.h"

// External.
#include "fmt/core.h"
#include "vulkan/vk_enum_string_helper.h"

namespace ne {

    VulkanResource::VulkanResource(
        VulkanResourceManager* pResourceManager,
        const std::string& sResourceName,
        VkBuffer pInternalResource,
        VmaAllocation pResourceMemory) {
        this->pResourceManager = pResourceManager;
        this->pInternalResource = pInternalResource;
        this->pResourceMemory = pResourceMemory;
        this->sResourceName = sResourceName;

        // Increment counter of alive objects.
        pResourceManager->iAliveResourceCount.fetch_add(1);
    }

    VulkanResource::~VulkanResource() {
        // Don't log here to avoid spamming.

        // Make sure the GPU is not using this resource.
        pResourceManager->getRenderer()->waitForGpuToFinishWorkUpToThisPoint();

        // Destroy the resource and its memory.
        vmaDestroyBuffer(pResourceManager->pMemoryAllocator, pInternalResource, pResourceMemory);

        // Decrement counter of alive objects.
        const auto iPreviousTotalResourceCount = pResourceManager->iAliveResourceCount.fetch_sub(1);

        // Self check: make sure the counter is not below zero.
        if (iPreviousTotalResourceCount == 0) [[unlikely]] {
            Logger::get().error("total alive Vulkan resource counter just went below zero");
        }
    }

    std::optional<Error> VulkanResource::bindDescriptor(DescriptorType descriptorType) {
        throw std::runtime_error("not implemented");
    }

    std::string VulkanResource::getResourceName() const { return sResourceName; }

    VulkanResourceManager* VulkanResource::getResourceManager() const { return pResourceManager; }

    std::variant<std::unique_ptr<VulkanResource>, Error> VulkanResource::create(
        VulkanResourceManager* pResourceManager,
        const std::string& sResourceName,
        VmaAllocator pMemoryAllocator,
        const VkBufferCreateInfo& bufferInfo,
        const VmaAllocationCreateInfo& allocationInfo) {
        // Prepare variables for created data.
        VkBuffer pCreatedBuffer = nullptr;
        VmaAllocation pCreatedMemory = nullptr;

        // Create buffer.
        const auto result = vmaCreateBuffer(
            pMemoryAllocator, &bufferInfo, &allocationInfo, &pCreatedBuffer, &pCreatedMemory, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
                "failed to create resource \"{}\", error: {}", sResourceName, string_VkResult(result)));
        }

        // Set allocation name.
        vmaSetAllocationName(pMemoryAllocator, pCreatedMemory, sResourceName.c_str());

        return std::unique_ptr<VulkanResource>(
            new VulkanResource(pResourceManager, sResourceName, pCreatedBuffer, pCreatedMemory));
    }

} // namespace ne
