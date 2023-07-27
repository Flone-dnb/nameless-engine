#pragma once

// Standard.
#include <atomic>
#include <optional>

// Custom.
#include "render/general/resources/GpuResourceManager.h"

// External.
#include "VulkanMemoryAllocator/include/vk_mem_alloc.h"

namespace ne {
    class VulkanRenderer;
    class VulkanResource;

    /** Controls resource creation. */
    class VulkanResourceManager : public GpuResourceManager {
        // Resource will request a memory allocator object during destruction.
        friend class VulkanResource;

        // Upload buffer needs access to memory allocator object for map/unmap operations.
        friend class UploadBuffer;

    public:
        VulkanResourceManager() = delete;
        VulkanResourceManager(const VulkanResourceManager&) = delete;
        VulkanResourceManager& operator=(const VulkanResourceManager&) = delete;

        virtual ~VulkanResourceManager() override;

        /**
         * Creates a new resource manager.
         *
         * @param pRenderer Vulkan renderer.
         *
         * @return Error if something went wrong, otherwise created resource manager.
         */
        static std::variant<std::unique_ptr<VulkanResourceManager>, Error> create(VulkanRenderer* pRenderer);

        /**
         * Returns total video memory size (VRAM) in megabytes.
         *
         * @return Total video memory size in megabytes.
         */
        virtual size_t getTotalVideoMemoryInMb() const override;

        /**
         * Returns the amount of video memory (VRAM) occupied by all currently allocated resources.
         *
         * @return Size of the video memory used by allocated resources.
         */
        virtual size_t getUsedVideoMemoryInMb() const override;

        /**
         * Creates a new buffer resource.
         *
         * @param sResourceName   Name of the created resource.
         * @param bufferInfo      Description of the created buffer resource.
         * @param allocationInfo  Description of created buffer's memory allocation.
         *
         * @return Error if something went wrong, otherwise created buffer resource.
         */
        std::variant<std::unique_ptr<VulkanResource>, Error> createResource(
            const std::string& sResourceName,
            const VkBufferCreateInfo& bufferInfo,
            const VmaAllocationCreateInfo& allocationInfo);

        /**
         * Creates a new GPU resource with available CPU access, typically used
         * for resources that needs to be frequently updated from the CPU side.
         *
         * Example:
         * @code
         * struct ObjectData{
         *     glm::mat4x4 world;
         * };
         *
         * auto result = pResourceManager->createResourceWithCpuAccess(
         *     "object constant data",
         *     sizeof(ObjectData),
         *     1,
         *     CpuVisibleShaderResourceUsageDetails(true));
         * @endcode
         *
         * @param sResourceName                  Resource name, used for logging.
         * @param iElementSizeInBytes            Size of one buffer element in bytes.
         * @param iElementCount                  Amount of elements in the resulting buffer.
         * @param isUsedInShadersAsReadOnlyData Determines whether this resource will be used to store
         * shader read-only data (cannon be modified from shaders) or not going to be used in shaders
         * at all (specify empty). You need to specify this because in some internal implementations
         * might pad the specified element size to be a multiple of 256 because of some
         * hardware requirements. Otherwise if you don't plan to use this buffer
         * in shaders (for ex. you can use it as a staging/upload buffer) specify empty.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        virtual std::variant<std::unique_ptr<UploadBuffer>, Error> createResourceWithCpuAccess(
            const std::string& sResourceName,
            size_t iElementSizeInBytes,
            size_t iElementCount,
            std::optional<CpuVisibleShaderResourceUsageDetails> isUsedInShadersAsReadOnlyData) override;

        /**
         * Creates a new GPU resource and fills it with the specified data.
         *
         * Example:
         * @code
         * std::vector<glm::vec3> vVertices;
         *
         * auto result = pResourceManager->createResourceWithData(
         *     "mesh vertex buffer",
         *     vVertices.data(),
         *     vVertices.size() * sizeof(glm::vec3),
         *     true);
         * @endcode
         *
         * @param sResourceName              Resource name, used for logging.
         * @param pBufferData                Pointer to the data that the new resource will contain.
         * @param iDataSizeInBytes           Size in bytes of the data (resource size).
         * @param usage                      Describes how you plan to use this resource.
         * @param bIsShaderReadWriteResource Specify `true` if you plan to modify the resource
         * from shaders, otherwise `false`.
         *
         * @return Error if something went wrong, otherwise created resource with filled data.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> createResourceWithData(
            const std::string& sResourceName,
            const void* pBufferData,
            size_t iDataSizeInBytes,
            ResourceUsageType usage,
            bool bIsShaderReadWriteResource) override;

    private:
        /**
         * Constructor.
         *
         * @param pRenderer        DirectX renderer.
         * @param pMemoryAllocator Created memory allocator to use.
         */
        VulkanResourceManager(VulkanRenderer* pRenderer, VmaAllocator pMemoryAllocator);

        /**
         * Converts `ResourceUsageType` to `VkBufferUsageFlagBits`.
         *
         * @param usage Resource usage type.
         *
         * @return Empty if resource usage type is set to `OTHER`, otherwise Vulkan buffer usage flags.
         */
        static std::optional<VkBufferUsageFlagBits>
        convertResourceUsageTypeToVkBufferUsageType(ResourceUsageType usage);

        /**
         * Creates a new buffer and allocates a new memory for it.
         *
         * @param sResourceName     Name of the created buffer.
         * @param iBufferSize       Size of the buffer in bytes.
         * @param bufferUsage       Buffer usage.
         * @param bAllowCpuAccess   Describes memory properties of the created buffer.
         * If `true` the memory will have `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` and
         * `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`, otherwise `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        std::variant<std::unique_ptr<VulkanResource>, Error> createBuffer(
            const std::string& sResourceName,
            VkDeviceSize iBufferSize,
            VkBufferUsageFlags bufferUsage,
            bool bAllowCpuAccess);

        /** Total number of created resources that were not destroyed yet. */
        std::atomic<size_t> iAliveResourceCount{0};

        /** Vulkan memory allocator. */
        VmaAllocator pMemoryAllocator = nullptr;
    };
} // namespace ne
