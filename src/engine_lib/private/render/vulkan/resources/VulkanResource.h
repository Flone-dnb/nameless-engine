#pragma once

// Standard.
#include <variant>
#include <memory>
#include <mutex>
#include <format>

// Custom.
#include "render/general/resources/GpuResource.h"

// External.
#include "VulkanMemoryAllocator/include/vk_mem_alloc.h"
#include "ktxvulkan.h"

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
         * Returns internal buffer resource.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @remark Returned pointer is only valid while this object is valid.
         *
         * @return `nullptr` if this resource uses image as internal resource not a buffer,
         * otherwise internal buffer resource.
         */
        inline VkBuffer getInternalBufferResource() const { return pBufferResource; };

        /**
         * Returns internal image view.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @remark Returned pointer is only valid while this object is valid.
         *
         * @return `nullptr` if this resource uses buffer as internal resource not an image,
         * otherwise internal image view.
         */
        inline VkImageView getInternalImageView() const { return pImageView; }

        /**
         * Returns internal image view that only references depth aspect of the image.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @remark Returned pointer is only valid while this object is valid.
         *
         * @return Image view that only references depth aspect if the image was created with depth and
         * stencil aspects, otherwise `nullptr`.
         */
        inline VkImageView getInternalImageViewDepthAspect() const { return pDepthAspectImageView; }

        /**
         * Returns internal image.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @remark Returned pointer is only valid while this object is valid.
         *
         * @return `nullptr` if this resource uses buffer as internal resource not an image,
         * otherwise internal image.
         */
        inline VkImage getInternalImage() const { return pImageResource; }

        /**
         * Returns memory allocation of the internal resource.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @remark Returned pointer is only valid while this object is valid.
         *
         * @return Memory allocation of the internal resource.
         */
        inline std::pair<std::recursive_mutex, VmaAllocation>* getInternalResourceMemory() {
            // Self check:
            if (optionalKtxTexture.has_value()) [[unlikely]] {
                Error error(std::format(
                    "failed to query VmaAllocation of resource \"{}\" because "
                    "this resource is a KTX texture that was loaded via external library "
                    "(accessing VmaAllocation of such object is complicated, if you want "
                    "to access VkDeviceMemory it's a good time to implement such a getter "
                    "because VkDeviceMemory is available for KTX textures)",
                    getResourceName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            return &mtxResourceMemory;
        }

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
         * @param iElementSizeInBytes  Resource size information. Size of one array element (if array),
         * otherwise specify size of the whole resource.
         * @param iElementCount        Resource size information. Total number of elements in the array (if
         * array), otherwise specify 1.
         */
        VulkanResource(
            VulkanResourceManager* pResourceManager,
            const std::string& sResourceName,
            std::variant<VkBuffer, VkImage> pInternalResource,
            VmaAllocation pResourceMemory,
            unsigned int iElementSizeInBytes,
            unsigned int iElementCount);

        /**
         * Constructor. Initializes resources as a wrapper for KTX image.
         *
         * @param pResourceManager  Owner resource manager.
         * @param sResourceName     Name of the resource.
         * @param ktxTexture        Created KTX texture (already loaded in the GPU memory) that this resource
         * will wrap.
         */
        VulkanResource(
            VulkanResourceManager* pResourceManager,
            const std::string& sResourceName,
            ktxVulkanTexture ktxTexture);

        /**
         * Creates a new buffer resource.
         *
         * @param pResourceManager Owner resource manager.
         * @param sResourceName    Resource name, used for logging.
         * @param pMemoryAllocator Allocator to create resource.
         * @param bufferInfo       Buffer creation info.
         * @param allocationInfo   Allocation creation info.
         * @param iElementSizeInBytes  Resource size information. Size of one array element (if array),
         * otherwise specify size of the whole resource.
         * @param iElementCount        Resource size information. Total number of elements in the array (if
         * array), otherwise specify 1.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        static std::variant<std::unique_ptr<VulkanResource>, Error> create(
            VulkanResourceManager* pResourceManager,
            const std::string& sResourceName,
            VmaAllocator pMemoryAllocator,
            const VkBufferCreateInfo& bufferInfo,
            const VmaAllocationCreateInfo& allocationInfo,
            unsigned int iElementSizeInBytes,
            unsigned int iElementCount);

        /**
         * Creates a new image resource.
         *
         * @param pResourceManager Owner resource manager.
         * @param sResourceName    Resource name, used for logging.
         * @param pMemoryAllocator Allocator to create resource.
         * @param imageInfo        Image creation info.
         * @param allocationInfo   Allocation creation info.
         * @param viewDescription  If specified also creates an image view that references the image.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        static std::variant<std::unique_ptr<VulkanResource>, Error> create(
            VulkanResourceManager* pResourceManager,
            const std::string& sResourceName,
            VmaAllocator pMemoryAllocator,
            const VkImageCreateInfo& imageInfo,
            const VmaAllocationCreateInfo& allocationInfo,
            std::optional<VkImageAspectFlags> viewDescription);

        /**
         * Creates a new image resource.
         *
         * @param pResourceManager Owner resource manager.
         * @param sResourceName    Resource name, used for logging.
         * @param ktxTexture       Created KTX texture (already loaded in the GPU memory) that this resource
         * will wrap.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        static std::variant<std::unique_ptr<VulkanResource>, Error> create(
            VulkanResourceManager* pResourceManager,
            const std::string& sResourceName,
            ktxVulkanTexture ktxTexture);

        /** Not empty if the object was created as a wrapper around KTX texture. */
        std::optional<ktxVulkanTexture> optionalKtxTexture;

        /**
         * Created buffer Vulkan resource.
         *
         * @remark `nullptr` if @ref pImageResource is used.
         */
        VkBuffer pBufferResource = nullptr;

        /**
         * Created image Vulkan resource.
         *
         * @remark `nullptr` if @ref pBufferResource is used.
         */
        VkImage pImageResource = nullptr;

        /** Optional view that references @ref pImageResource. */
        VkImageView pImageView = nullptr;

        /**
         * Optional view that references @ref pImageResource depth aspect.
         *
         * @remark Only used when @ref pImageResource specified both depth and stencil aspects.
         */
        VkImageView pDepthAspectImageView = nullptr;

        /**
         * Allocated memory for created resource.
         *
         * @remark Using mutex because "access to a VmaAllocation object must be externally synchronized".
         */
        std::pair<std::recursive_mutex, VmaAllocation> mtxResourceMemory;

        /** Do not delete. Owner resource manager. */
        VulkanResourceManager* pResourceManager = nullptr;
    };
} // namespace ne
