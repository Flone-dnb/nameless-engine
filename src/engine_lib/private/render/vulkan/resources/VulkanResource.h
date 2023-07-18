#pragma once

// Standard.
#include <variant>
#include <memory>

// Custom.
#include "render/general/resources/GpuResource.h"

// External.
#include "VulkanMemoryAllocator/include/vk_mem_alloc.h"

namespace ne {
    class VulkanResourceManager;

    /** Vulkan buffer/memory wrapper. */
    class VulkanResource : public GpuResource {
        // Only resource manager can create this resource
        // (simply because only manager has a memory allocator object).
        friend class VulkanResourceManager;

    public:
        VulkanResource() = delete;
        VulkanResource(const VulkanResource&) = delete;
        VulkanResource& operator=(const VulkanResource&) = delete;

        virtual ~VulkanResource() override;

        /**
         * Creates a new descriptor and binds it to this resource.
         *
         * @param descriptorType Type of descriptor to bind.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindDescriptor(DescriptorType descriptorType) override;

        /**
         * Returns resource name.
         *
         * @return Resource name.
         */
        virtual std::string getResourceName() const override;

        /**
         * Returns internal resource.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @remark Returned pointer is only valid while this object is valid.
         *
         * @return Internal resource.
         */
        inline VkBuffer getInternalResource() const { return pInternalResource; };

        /**
         * Returns memory allocation of the internal resource.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @remark Returned pointer is only valid while this object is valid.
         *
         * @return Memory allocation of the internal resource.
         */
        inline VmaAllocation getInternalResourceMemory() const { return pResourceMemory; }

        /**
         * Returns resource manager that created the resource.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @return Resource manager.
         */
        VulkanResourceManager* getResourceManager() const;

    private:
        /**
         * Constructor. Creates an empty resource.
         *
         * @param pResourceManager  Owner resource manager.
         * @param sResourceName     Name of the resource.
         * @param pInternalResource Created Vulkan resource.
         * @param pResourceMemory   Allocated memory for the created Vulkan resource.
         */
        VulkanResource(
            VulkanResourceManager* pResourceManager,
            const std::string& sResourceName,
            VkBuffer pInternalResource,
            VmaAllocation pResourceMemory);

        /**
         * Creates a new resource.
         *
         * @param pResourceManager Owner resource manager.
         * @param sResourceName    Resource name, used for logging.
         * @param pMemoryAllocator Allocator to create resource.
         * @param bufferInfo       Buffer creation info.
         * @param allocationInfo   Allocation creation info.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        static std::variant<std::unique_ptr<VulkanResource>, Error> create(
            VulkanResourceManager* pResourceManager,
            const std::string& sResourceName,
            VmaAllocator pMemoryAllocator,
            const VkBufferCreateInfo& bufferInfo,
            const VmaAllocationCreateInfo& allocationInfo);

        /** Name of the resource. */
        std::string sResourceName;

        /** Created Vulkan resource. */
        VkBuffer pInternalResource = nullptr;

        /** Allocated memory for @ref pInternalResource. */
        VmaAllocation pResourceMemory = nullptr;

        /** Do not delete. Owner resource manager. */
        VulkanResourceManager* pResourceManager = nullptr;
    };
} // namespace ne
