#include "UploadBuffer.h"

// Standard.
#include <stdexcept>
#include <format>

// Custom.
#include "render/vulkan/resources/VulkanResource.h"
#include "render/vulkan/resources/VulkanResourceManager.h"
#if defined(WIN32)
#include "render/directx/resources/DirectXResource.h"
#endif

// External.
#include "vulkan/vk_enum_string_helper.h"

namespace ne {

    UploadBuffer::UploadBuffer(
        std::unique_ptr<GpuResource> pGpuResourceToUse, size_t iElementSizeInBytes, size_t iElementCount) {
        // Initialize.
        pGpuResource = std::move(pGpuResourceToUse);
        this->iElementSizeInBytes = iElementSizeInBytes;
        this->iElementCount = iElementCount;

#if defined(WIN32)
        if (auto pDirectXResource = dynamic_cast<DirectXResource*>(pGpuResource.get())) {
            // Map the resource.
            auto hResult = pDirectXResource->getInternalResource()->Map(
                0, nullptr, reinterpret_cast<void**>(&pMappedResourceData));
            if (FAILED(hResult)) [[unlikely]] {
                Error error(hResult);
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
            return;
        }
#endif

        if (auto pVulkanResource = dynamic_cast<VulkanResource*>(pGpuResource.get())) {
            // Get memory allocator.
            const auto pMemoryAllocator = pVulkanResource->getResourceManager()->pMemoryAllocator;

            // Lock resource memory.
            const auto pMtxResourceMemory = pVulkanResource->getInternalResourceMemory();
            std::scoped_lock guard(pMtxResourceMemory->first);

            // Map the resource.
            const auto result = vmaMapMemory(
                pMemoryAllocator, pMtxResourceMemory->second, reinterpret_cast<void**>(&pMappedResourceData));
            if (result != VK_SUCCESS) [[unlikely]] {
                Error error(std::format(
                    "failed to map memory of resource, error: {}",
                    pVulkanResource->getResourceName(),
                    string_VkResult(result)));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
            return;
        }
    }

    size_t UploadBuffer::getElementCount() const { return iElementCount; }

    size_t UploadBuffer::getElementSizeInBytes() const { return iElementSizeInBytes; }

    UploadBuffer::~UploadBuffer() {
#if defined(WIN32)
        if (auto pDirectXResource = dynamic_cast<DirectXResource*>(pGpuResource.get())) {
            // Unmap the resource.
            pDirectXResource->getInternalResource()->Unmap(0, nullptr);
            pMappedResourceData = nullptr;

            return;
        }
#endif

        if (auto pVulkanResource = dynamic_cast<VulkanResource*>(pGpuResource.get())) {
            // Get memory allocator.
            const auto pMemoryAllocator = pVulkanResource->getResourceManager()->pMemoryAllocator;

            // Lock resource memory.
            const auto pMtxResourceMemory = pVulkanResource->getInternalResourceMemory();
            std::scoped_lock guard(pMtxResourceMemory->first);

            // Unmap the resource.
            vmaUnmapMemory(pMemoryAllocator, pMtxResourceMemory->second);
            pMappedResourceData = nullptr;

            return;
        }
    }

    GpuResource* UploadBuffer::getInternalResource() const { return pGpuResource.get(); }

} // namespace ne
