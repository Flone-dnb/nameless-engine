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
    class VulkanStorageResourceArrayManager;

    /** Controls resource creation. */
    class VulkanResourceManager : public GpuResourceManager {
        // Resource will request a memory allocator object during destruction.
        friend class VulkanResource;

        // Upload buffer needs access to memory allocator object for map/unmap operations.
        friend class UploadBuffer;

        // Uses memory allocator to load KTX textures.
        friend class KtxLoadingCallbackManager;

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
        std::variant<std::unique_ptr<VulkanResource>, Error> createBuffer(
            const std::string& sResourceName,
            const VkBufferCreateInfo& bufferInfo,
            const VmaAllocationCreateInfo& allocationInfo);

        /**
         * Creates a new image and allocates a new memory for it.
         *
         * @param sResourceName     Name of the created resource.
         * @param iImageWidth       Width of the image in pixels.
         * @param iImageHeight      Height of the image in pixels.
         * @param iTextureMipLevelCount The number of mip level the texture has.
         * @param sampleCount       The number of samples per pixel. Usually 1 and more than 1 for MSAA.
         * @param imageFormat       Format of the image.
         * @param imageTilingMode   Image tiling mode.
         * @param imageUsage        Image usage.
         * @param viewDescription   If specified also creates an image view that references the image.
         *
         * @return Created image.
         */
        std::variant<std::unique_ptr<VulkanResource>, Error> createImage(
            const std::string& sResourceName,
            uint32_t iImageWidth,
            uint32_t iImageHeight,
            uint32_t iTextureMipLevelCount,
            VkSampleCountFlagBits sampleCount,
            VkFormat imageFormat,
            VkImageTiling imageTilingMode,
            VkImageUsageFlags imageUsage,
            std::optional<VkImageAspectFlags> viewDescription);

        /**
         * Loads a texture from a KTX file in the GPU memory.
         *
         * @param sResourceName     Resource name, used for logging.
         * @param pathToTextureFile Path to the image file that stores texture data.
         *
         * @return Error if something went wrong, otherwise created GPU resource that stores texture data.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> loadTextureFromDisk(
            const std::string& sResourceName, const std::filesystem::path& pathToTextureFile) override;

        /**
         * Creates a new GPU resource (buffer) with available CPU write access (only write not read),
         * typically used for resources that needs to be frequently updated from the CPU side.
         *
         * Example:
         * @code
         * struct ObjectData{
         *     glm::mat4x4 world;
         * };
         *
         * auto result = pResourceManager->createResourceWithCpuWriteAccess(
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
        virtual std::variant<std::unique_ptr<UploadBuffer>, Error> createResourceWithCpuWriteAccess(
            const std::string& sResourceName,
            size_t iElementSizeInBytes,
            size_t iElementCount,
            std::optional<CpuVisibleShaderResourceUsageDetails> isUsedInShadersAsReadOnlyData) override;

        /**
         * Creates a new GPU resource (buffer, not a texture) and fills it with the specified data.
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

        /**
         * Returns manager that controls storage buffers that act as arrays for
         * shader CPU read/write resources.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @return Manager.
         */
        VulkanStorageResourceArrayManager* getStorageResourceArrayManager() const;

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
         * @param sResourceName   Name of the created buffer.
         * @param iBufferSize     Size of the buffer in bytes.
         * @param bufferUsage     Buffer usage.
         * @param bAllowCpuWrite  Describes memory properties of the created buffer.
         * If `true` the memory will be `HOST_VISIBLE`, `HOST_COHERENT`,
         * otherwise `DEVICE_LOCAL`.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        std::variant<std::unique_ptr<VulkanResource>, Error> createBuffer(
            const std::string& sResourceName,
            VkDeviceSize iBufferSize,
            VkBufferUsageFlags bufferUsage,
            bool bAllowCpuWrite);

        /** Total number of created resources that were not destroyed yet. */
        std::atomic<size_t> iAliveResourceCount{0};

        /** Controls storage buffers that act as arrays for shader CPU write resources. */
        std::unique_ptr<VulkanStorageResourceArrayManager> pStorageResourceArrayManager;

        /** Vulkan memory allocator. */
        VmaAllocator pMemoryAllocator = nullptr;
    };
} // namespace ne
