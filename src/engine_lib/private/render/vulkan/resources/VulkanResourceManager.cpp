#include "VulkanResourceManager.h"

// Standard.
#include <format>
#include <atomic>

// Custom.
#include "render/vulkan/VulkanRenderer.h"
#include "io/Logger.h"
#include "render/vulkan/resources/VulkanResource.h"
#include "render/vulkan/resources/VulkanStorageResourceArrayManager.h"
#include "render/vulkan/resources/KtxLoadingCallbackManager.h"

// External.
#define VMA_IMPLEMENTATION // define in only one .cpp file
#include "VulkanMemoryAllocator/include/vk_mem_alloc.h"
#include "vulkan/vk_enum_string_helper.h"

namespace ne {

    VulkanResourceManager::VulkanResourceManager(VulkanRenderer* pRenderer, VmaAllocator pMemoryAllocator)
        : GpuResourceManager(pRenderer) {
        this->pMemoryAllocator = pMemoryAllocator;

        pStorageResourceArrayManager =
            std::unique_ptr<VulkanStorageResourceArrayManager>(new VulkanStorageResourceArrayManager(this));
    }

    std::variant<std::unique_ptr<VulkanResource>, Error> VulkanResourceManager::createBuffer(
        const std::string& sResourceName,
        VkDeviceSize iBufferSize,
        VkBufferUsageFlags bufferUsage,
        bool bAllowCpuWrite,
        unsigned int iElementSizeInBytes,
        unsigned int iElementCount) {
        // Describe buffer.
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = iBufferSize;
        bufferInfo.usage = bufferUsage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Prepare allocation info.
        VmaAllocationCreateInfo allocationCreateInfo{};
        if (bAllowCpuWrite) {
            allocationCreateInfo.requiredFlags =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        } else {
            allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        // Create resource.
        auto result =
            createBuffer(sResourceName, bufferInfo, allocationCreateInfo, iElementSizeInBytes, iElementCount);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
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
        case (ResourceUsageType::ARRAY_BUFFER): {
            return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
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
        // Explicitly destroy storage array manager so that it will free its GPU resources.
        pStorageResourceArrayManager = nullptr;

        // Explicitly destroy shadow map manager so that it will no longer reference any GPU resources.
        resetShadowMapManager();

        // Explicitly destroy texture manager so that it will no longer reference any GPU resources.
        resetTextureManager();

        // Make sure no resource exist
        // (because in Vulkan resources need memory allocator to be destroyed).
        const auto iTotalAliveResourceCount = getTotalAliveResourceCount();
        const auto iKtxAllocationCount = KtxLoadingCallbackManager::getCurrentAllocationCount();
        if (iTotalAliveResourceCount != 0 || iKtxAllocationCount != 0) [[unlikely]] {
            Error error(std::format(
                "Vulkan resource manager is being destroyed but there are "
                "still {} resource(s) and {} KTX allocations alive, most likely you forgot to explicitly "
                "reset/delete some GPU resources that are used in the vulkan renderer class (only "
                "resources inside of the vulkan renderer class should be explicitly deleted before "
                "the resource manager is destroyed, everything else is expected to be automatically "
                "deleted by world destruction)",
                iTotalAliveResourceCount,
                iKtxAllocationCount));
            error.showError();
            return; // don't throw in destructor, just quit
        }

        vmaDestroyAllocator(pMemoryAllocator);
        pMemoryAllocator = nullptr;
    }

    VkFormat VulkanResourceManager::convertTextureResourceFormatToVkFormat(
        ShaderReadWriteTextureResourceFormat format) {
        switch (format) {
        case (ShaderReadWriteTextureResourceFormat::R32G32_UINT): {
            return VK_FORMAT_R32G32_UINT;
            break;
        }
        case (ShaderReadWriteTextureResourceFormat::SIZE): {
            Error error("invalid format");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
            break;
        }
        }

        static_assert(
            static_cast<size_t>(ShaderReadWriteTextureResourceFormat::SIZE) == 1,
            "add new formats to convert");

        Error error("unhandled case");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
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
                std::format("failed to create memory allocator, error: {}", string_VkResult(result)));
        }

        return std::unique_ptr<VulkanResourceManager>(new VulkanResourceManager(pRenderer, pMemoryAllocator));
    }

    size_t VulkanResourceManager::getTotalVideoMemoryInMb() const {
        // Get renderer.
        const auto pRenderer = dynamic_cast<VulkanRenderer*>(getRenderer());
        if (pRenderer == nullptr) [[unlikely]] {
            Error error("expected a Vulkan renderer");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

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
        // Get renderer.
        const auto pRenderer = dynamic_cast<VulkanRenderer*>(getRenderer());
        if (pRenderer == nullptr) [[unlikely]] {
            Error error("expected a Vulkan renderer");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

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

    std::variant<std::unique_ptr<VulkanResource>, Error> VulkanResourceManager::createBuffer(
        const std::string& sResourceName,
        const VkBufferCreateInfo& bufferInfo,
        const VmaAllocationCreateInfo& allocationInfo,
        unsigned int iElementSizeInBytes,
        unsigned int iElementCount) {
        auto result = VulkanResource::create(
            this,
            sResourceName,
            pMemoryAllocator,
            bufferInfo,
            allocationInfo,
            iElementSizeInBytes,
            iElementCount);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        return std::get<std::unique_ptr<VulkanResource>>(std::move(result));
    }

    std::variant<std::unique_ptr<UploadBuffer>, Error>
    VulkanResourceManager::createResourceWithCpuWriteAccess(
        const std::string& sResourceName,
        size_t iElementSizeInBytes,
        size_t iElementCount,
        std::optional<bool> isUsedInShadersAsArrayResource) {
        const size_t iBufferSizeInBytes = iElementSizeInBytes * iElementCount;
        auto usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        if (isUsedInShadersAsArrayResource.has_value()) {
            if (!isUsedInShadersAsArrayResource.value()) {
                usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

                // Get renderer.
                const auto pRenderer = dynamic_cast<VulkanRenderer*>(getRenderer());
                if (pRenderer == nullptr) [[unlikely]] {
                    return Error("expected a Vulkan renderer");
                }

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
                    return Error(std::format(
                        "unable to create the requested uniform buffer with the size {} bytes "
                        "because the GPU limit for uniform buffer sizes is {} bytes",
                        iBufferSizeInBytes,
                        deviceProperties.limits.maxUniformBufferRange));
                }
            } else {
                usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            }
        }

        // Make sure resource information will not hit type limit.
        if (iElementSizeInBytes > std::numeric_limits<unsigned int>::max() ||
            iElementCount > std::numeric_limits<unsigned int>::max()) [[unlikely]] {
            return Error("resource size is too big");
        }

        // Create buffer.
        auto result = createBuffer(
            sResourceName,
            iBufferSizeInBytes,
            usage,
            true,
            static_cast<unsigned int>(iElementSizeInBytes),
            static_cast<unsigned int>(iElementCount));
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto pResource = std::get<std::unique_ptr<VulkanResource>>(std::move(result));

        return std::unique_ptr<UploadBuffer>(
            new UploadBuffer(std::move(pResource), iElementSizeInBytes, iElementCount));
    }

    std::variant<std::unique_ptr<GpuResource>, Error> VulkanResourceManager::createResourceWithData(
        const std::string& sResourceName,
        const void* pBufferData,
        size_t iElementSizeInBytes,
        size_t iElementCount,
        ResourceUsageType usage,
        bool bIsShaderReadWriteResource) {
        // Calculate final data size.
        const auto iDataSizeInBytes = iElementSizeInBytes * iElementCount;

        // Create an upload resource for uploading data.
        auto uploadResourceResult = createResourceWithCpuWriteAccess(sResourceName, iDataSizeInBytes, 1, {});
        if (std::holds_alternative<Error>(uploadResourceResult)) {
            auto error = std::get<Error>(std::move(uploadResourceResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto pUploadResource = std::get<std::unique_ptr<UploadBuffer>>(std::move(uploadResourceResult));

        // Copy data to the allocated upload resource memory.
        pUploadResource->copyDataToElement(0, pBufferData, iDataSizeInBytes);

        // Create the final resource to copy the data to.
        auto finalResourceResult = createResource(
            sResourceName, iElementSizeInBytes, iElementCount, usage, bIsShaderReadWriteResource);
        if (std::holds_alternative<Error>(finalResourceResult)) {
            auto error = std::get<Error>(std::move(finalResourceResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto pFinalResource = std::get<std::unique_ptr<GpuResource>>(std::move(finalResourceResult));

        // Convert resource type.
        auto pFinalVulkanResource = dynamic_cast<VulkanResource*>(pFinalResource.get());
        if (pFinalVulkanResource == nullptr) [[unlikely]] {
            return Error("expected a vulkan resource");
        }

        // Get renderer.
        const auto pRenderer = dynamic_cast<VulkanRenderer*>(getRenderer());
        if (pRenderer == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Create one-time submit command buffer to copy data from the upload resource to
        // the final resource.
        auto commandBufferResult = pRenderer->createOneTimeSubmitCommandBuffer();
        if (std::holds_alternative<Error>(commandBufferResult)) {
            auto error = std::get<Error>(std::move(commandBufferResult));
            error.addCurrentLocationToErrorStack();
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
            pVkUploadResource->getInternalBufferResource(),
            pFinalVulkanResource->getInternalBufferResource(),
            1,
            &copyRegion);

        // Submit command buffer.
        auto optionalError =
            pRenderer->submitWaitDestroyOneTimeSubmitCommandBuffer(pOneTimeSubmitCommandBuffer);
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return pFinalResource;
    }

    std::variant<std::unique_ptr<GpuResource>, Error> VulkanResourceManager::createResource(
        const std::string& sResourceName,
        size_t iElementSizeInBytes,
        size_t iElementCount,
        ResourceUsageType usage,
        bool bIsShaderReadWriteResource) {
        // Calculate data size.
        const auto iDataSizeInBytes = iElementSizeInBytes * iElementCount;

        // Prepare resource usage flags.
        const auto optionalResourceUsage = convertResourceUsageTypeToVkBufferUsageType(usage);
        const auto resourceUsage = optionalResourceUsage.has_value()
                                       ? VK_BUFFER_USAGE_TRANSFER_DST_BIT | optionalResourceUsage.value()
                                       : VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        // Make sure resource information will not hit type limit.
        if (iElementSizeInBytes > std::numeric_limits<unsigned int>::max() ||
            iElementCount > std::numeric_limits<unsigned int>::max()) [[unlikely]] {
            return Error("resource size is too big");
        }

        // Create resource.
        auto resourceResult = createBuffer(
            sResourceName,
            iDataSizeInBytes,
            resourceUsage,
            false,
            static_cast<unsigned int>(iElementSizeInBytes),
            static_cast<unsigned int>(iElementCount));
        if (std::holds_alternative<Error>(resourceResult)) {
            auto error = std::get<Error>(std::move(resourceResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        return std::get<std::unique_ptr<VulkanResource>>(std::move(resourceResult));
    }

    std::variant<std::unique_ptr<GpuResource>, Error>
    VulkanResourceManager::createShaderReadWriteTextureResource(
        const std::string& sResourceName,
        unsigned int iWidth,
        unsigned int iHeight,
        ShaderReadWriteTextureResourceFormat format) {
        // Prepare some variables.
        const auto imageFormat = convertTextureResourceFormatToVkFormat(format);
        const auto imageAspect = VK_IMAGE_ASPECT_COLOR_BIT;

        // Create image.
        auto resourceResult = createImage(
            sResourceName,
            iWidth,
            iHeight,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            imageFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT,
            imageAspect);
        if (std::holds_alternative<Error>(resourceResult)) {
            auto error = std::get<Error>(std::move(resourceResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto pTextureResource = std::get<std::unique_ptr<VulkanResource>>(std::move(resourceResult));

        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(getRenderer());
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Transition layout to general.
        auto optionalError = pVulkanRenderer->transitionImageLayout(
            pTextureResource->getInternalImage(),
            imageFormat,
            imageAspect,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL); // store image can be read/written only in this layout
        if (optionalError.has_value()) [[unlikely]] {
            auto error = std::move(optionalError.value());
            error.addCurrentLocationToErrorStack();
            return error;
        }

        return pTextureResource;
    }

    std::variant<std::unique_ptr<GpuResource>, Error> VulkanResourceManager::createShadowMapTexture(
        const std::string& sResourceName, unsigned int iTextureSize, bool bPointLightColorCubemap) {
        // Check that texture size is power of 2.
        if (!std::has_single_bit(iTextureSize)) [[unlikely]] {
            return Error(std::format(
                "shadow map size {} should be power of 2 (128, 256, 512, 1024, 2048, etc.)", iTextureSize));
        }

        // Prepare description for a usual 2D depth texture.
        auto textureFormat = VulkanRenderer::getShadowMapFormat();
        auto usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        auto aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (bPointLightColorCubemap) {
            textureFormat = VulkanRenderer::getShadowMappingPointLightColorTargetFormat();
            usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        // Create image.
        auto resourceResult = createImage(
            sResourceName,
            iTextureSize,
            iTextureSize,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            textureFormat,
            VK_IMAGE_TILING_OPTIMAL,
            usage,
            aspect,
            bPointLightColorCubemap);
        if (std::holds_alternative<Error>(resourceResult)) {
            auto error = std::get<Error>(std::move(resourceResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        return std::get<std::unique_ptr<VulkanResource>>(std::move(resourceResult));
    }

    std::variant<std::unique_ptr<VulkanResource>, Error> VulkanResourceManager::createImage(
        const std::string& sResourceName,
        uint32_t iImageWidth,
        uint32_t iImageHeight,
        uint32_t iTextureMipLevelCount,
        VkSampleCountFlagBits sampleCount,
        VkFormat imageFormat,
        VkImageTiling imageTilingMode,
        VkImageUsageFlags imageUsage,
        std::optional<VkImageAspectFlags> viewDescription,
        bool bIsCubeMap) {
        // Describe an image object.
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = iImageWidth;
        imageInfo.extent.height = iImageHeight;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = iTextureMipLevelCount;
        imageInfo.arrayLayers = bIsCubeMap ? 6 : 1; // NOLINT: cubemap face count
        imageInfo.format = imageFormat;
        imageInfo.tiling = imageTilingMode;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = imageUsage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = sampleCount;
        imageInfo.flags = bIsCubeMap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

        // Prepare allocation info for memory allocator.
        VmaAllocationCreateInfo allocationInfo = {};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        // Create resource.
        auto result = VulkanResource::create(
            this, sResourceName, pMemoryAllocator, imageInfo, allocationInfo, viewDescription, bIsCubeMap);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        return std::get<std::unique_ptr<VulkanResource>>(std::move(result));
    }

    std::variant<std::unique_ptr<GpuResource>, Error> VulkanResourceManager::loadTextureFromDisk(
        const std::string& sResourceName, const std::filesystem::path& pathToTextureFile) {
        // Make sure the specified path exists.
        if (!std::filesystem::exists(pathToTextureFile)) [[unlikely]] {
            return Error(
                std::format("the specified path \"{}\" does not exists", pathToTextureFile.string()));
        }

        // Make sure the specified path points to a file.
        if (std::filesystem::is_directory(pathToTextureFile)) [[unlikely]] {
            return Error(std::format(
                "expected the specified path \"{}\" to point to a file", pathToTextureFile.string()));
        }

        // Make sure the file has the ".KTX" extension.
        if (pathToTextureFile.extension() != ".KTX" && pathToTextureFile.extension() != ".ktx") [[unlikely]] {
            return Error(std::format(
                "only KTX file extension is supported for texture loading, the path \"{}\" points to a "
                "non-KTX file",
                pathToTextureFile.string()));
        }

        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(getRenderer());
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Prepare device info for texture loading.
        ktxVulkanDeviceInfo ktxDeviceInfo;
        auto result = ktxVulkanDeviceInfo_Construct(
            &ktxDeviceInfo,
            pVulkanRenderer->getPhysicalDevice(),
            pVulkanRenderer->getLogicalDevice(),
            pVulkanRenderer->getGraphicsQueue(),
            pVulkanRenderer->getCommandPool(),
            nullptr);
        if (result != KTX_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed create device info to load texture from file \"{}\", error: {}",
                pathToTextureFile.string(),
                ktxErrorString(result)));
        }

        // Load texture from disk.
        ktxTexture* pKtxUploadTexture = nullptr;
        result = ktxTexture_CreateFromNamedFile(
            pathToTextureFile.string().c_str(), KTX_TEXTURE_CREATE_NO_FLAGS, &pKtxUploadTexture);
        if (result != KTX_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to load texture from file \"{}\", error: {}",
                pathToTextureFile.string(),
                ktxErrorString(result)));
        }

        // Prepare callbacks for loading texture.
        auto subAllocCallbacks = KtxLoadingCallbackManager::getKtxSubAllocatorCallbacks();

        // Load texture to the GPU memory.
        ktxVulkanTexture textureData;
        result = ktxTexture_VkUploadEx_WithSuballocator(
            pKtxUploadTexture,
            &ktxDeviceInfo,
            &textureData,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            &subAllocCallbacks);
        if (result != KTX_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to load texture from file \"{}\" to the GPU memory, error: {}",
                pathToTextureFile.string(),
                ktxErrorString(result)));
        }

        // Cleanup.
        ktxTexture_Destroy(pKtxUploadTexture);
        ktxVulkanDeviceInfo_Destruct(&ktxDeviceInfo);

        // Wait for operations to be finished (just in case).
        pVulkanRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Wrap created texture data.
        auto createResult = VulkanResource::create(this, sResourceName, textureData);
        if (std::holds_alternative<Error>(createResult)) {
            auto error = std::get<Error>(std::move(createResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        return std::get<std::unique_ptr<VulkanResource>>(std::move(createResult));
    }

    VulkanStorageResourceArrayManager* VulkanResourceManager::getStorageResourceArrayManager() const {
        return pStorageResourceArrayManager.get();
    }

    std::string VulkanResourceManager::getCurrentStateInfo() {
        // Allocate stats.
        char* pState = nullptr;
        vmaBuildStatsString(pMemoryAllocator, &pState, VK_TRUE);

        // Copy to string.
        std::string sStatsJson(pState);

        // Free stats.
        vmaFreeStatsString(pMemoryAllocator, pState);

        return sStatsJson;
    }

} // namespace ne
