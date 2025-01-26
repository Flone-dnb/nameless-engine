#pragma once

// Standard.
#include <variant>
#include <memory>
#include <mutex>
#include <format>
#include <string>

// Custom.
#include "render/general/resource/GpuResource.h"
#include "io/Logger.h"
#include "material/TextureFilteringPreference.h"

// External.
#include "VulkanMemoryAllocator/include/vk_mem_alloc.h"
#include "ktxvulkan.h"

namespace ne {
    class VulkanResourceManager;
    class Renderer;

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
         * Returns internal image view for cubemap textures.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @remark Returned pointer is only valid while this object is valid.
         *
         * @param iCubemapFaceIndex Index of the cubemap face to get the view to.
         *
         * @return `nullptr` if this resource uses buffer as internal resource not an image or not a cubemap,
         * otherwise internal image view.
         */
        inline VkImageView getInternalCubemapImageView(size_t iCubemapFaceIndex = 0) {
            // Make sure we won't access out of bounds.
            if (iCubemapFaceIndex >= vCubeMapViews.size()) [[unlikely]] { // <- remove if no log
                // Log an error because this case is marked as unlikely.
                // ! REMOVE [[unlikely]] IF REMOVING LOG !
                Logger::get().error(std::format(
                    "cubemap view was requested on resource \"{}\" with an out of bounds index "
                    "{} while cubemap view count is {}",
                    getResourceName(),
                    iCubemapFaceIndex,
                    vCubeMapViews.size()));
                return nullptr;
            }

            return vCubeMapViews[iCubemapFaceIndex];
        }

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
         * Tells if this resource is a storage buffer/image or not.
         *
         * @return Storage bit from resource usage.
         */
        bool isStorageResource() const;

        /**
         * If this resource represents an image this function returns a sampler that uses preferred texture
         * filtering.
         *
         * @return Texture sampler from the renderer, always valid while the renderer is valid.
         */
        VkSampler getTextureSamplerForThisImage() const;

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

    private:
        /**
         * Constructor. Creates an empty resource.
         *
         * @param pResourceManager  Owner resource manager.
         * @param sResourceName     Name of the resource.
         * @param pInternalResource Created Vulkan resource.
         * @param isStorageResource Defines if this resource is a storage buffer/image or not.
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
            bool isStorageResource,
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
         * @param filteringPreference Texture filtering to use.
         */
        VulkanResource(
            VulkanResourceManager* pResourceManager,
            const std::string& sResourceName,
            ktxVulkanTexture ktxTexture,
            TextureFilteringPreference filteringPreference);

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
         * @param bIsCubeMapView   `true` to create a view to a cubemap, `false` to create a 2D texture
         * view. Ignored if view description is not specified.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        static std::variant<std::unique_ptr<VulkanResource>, Error> create(
            VulkanResourceManager* pResourceManager,
            const std::string& sResourceName,
            VmaAllocator pMemoryAllocator,
            const VkImageCreateInfo& imageInfo,
            const VmaAllocationCreateInfo& allocationInfo,
            std::optional<VkImageAspectFlags> viewDescription,
            bool bIsCubeMapView = false);

        /**
         * Creates a new image resource from the specified KTX texture.
         *
         * @param pResourceManager Owner resource manager.
         * @param sResourceName    Resource name, used for logging.
         * @param ktxTexture       Created KTX texture (already loaded in the GPU memory) that this resource
         * will wrap.
         * @param filteringPreference Texture filtering to use.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        static std::variant<std::unique_ptr<VulkanResource>, Error> create(
            VulkanResourceManager* pResourceManager,
            const std::string& sResourceName,
            ktxVulkanTexture ktxTexture,
            TextureFilteringPreference filteringPreference);

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

        /** Views to each face of the cubemap texture (only valid if the resource is a cubemap texture). */
        std::vector<VkImageView> vCubeMapViews;

        /**
         * Allocated memory for created resource.
         *
         * @remark Using mutex because "access to a VmaAllocation object must be externally synchronized".
         */
        std::pair<std::recursive_mutex, VmaAllocation> mtxResourceMemory;

        /** Texture filtering to use (if this resource is an image). */
        const TextureFilteringPreference textureFilteringPreference;

        /** Defines if this resource is a storage buffer/image or not. */
        const bool isUsedAsStorageResource = false;
    };
} // namespace ne
