#pragma once

// Custom.
#include "render/Renderer.h"
#include "render/general/resources/FrameResourcesManager.h"

// External.
#include "vulkan/vulkan.h"
#include "VulkanMemoryAllocator/include/vk_mem_alloc.h"

namespace ne {
    /** Renderer made with Vulkan API. */
    class VulkanRenderer : public Renderer {
    public:
        VulkanRenderer() = delete;
        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;

        virtual ~VulkanRenderer() override;

        /**
         * Returns used Vulkan API version.
         *
         * @return Vulkan API version that the renderer uses.
         */
        static uint32_t getUsedVulkanVersion();

        /**
         * Creates a new DirectX renderer.
         *
         * @param pGameManager GameManager object that owns this renderer.
         *
         * @return Error if something went wrong (for ex. if the hardware does not support this
         * renderer), otherwise created renderer.
         */
        static std::variant<std::unique_ptr<Renderer>, Error> create(GameManager* pGameManager);

        /**
         * Looks for video adapters (GPUs) that support this renderer.
         *
         * @remark Note that returned array might differ depending on the used renderer.
         *
         * @return Error if can't find any GPU that supports this renderer,
         * otherwise array with GPU names that can be used for this renderer.
         */
        virtual std::variant<std::vector<std::string>, Error> getSupportedGpuNames() const override;

        /**
         * Returns a list of supported render resolution (pairs of width and height).
         *
         * @return Error if something went wrong, otherwise render resolutions.
         */
        virtual std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
        getSupportedRenderResolutions() const override;

        /**
         * Returns a list of supported screen refresh rates (pairs of numerator and denominator).
         *
         * @remark The list of supported refresh rates depends on the currently used GPU,
         * so if changing used GPU this list might return different values.
         *
         * @return Error if something went wrong, otherwise refresh rates.
         */
        virtual std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
        getSupportedRefreshRates() const override;

        /**
         * Returns renderer's type.
         *
         * @return Renderer's type.
         */
        virtual RendererType getType() const override;

        /**
         * Returns API version or a feature level that the renderer uses.
         *
         * For example DirectX renderer will return used feature level and Vulkan renderer
         * will return used Vulkan API version.
         *
         * @return Used API version.
         */
        virtual std::string getUsedApiVersion() const override;

        /**
         * Returns the name of the GPU that is being currently used.
         *
         * @return Name of the GPU.
         */
        virtual std::string getCurrentlyUsedGpuName() const override;

        /**
         * Returns total video memory (VRAM) size in megabytes.
         *
         * @return Total video memory size in megabytes.
         */
        virtual size_t getTotalVideoMemoryInMb() const override;

        /**
         * Returns the amount of video memory (VRAM) that is currently being used by the renderer.
         *
         * @remark Does not return global (system-wide) used VRAM size, only VRAM used by the renderer
         * (i.e. only VRAM used by this application).
         *
         * @return Size of the video memory used by the renderer in megabytes.
         */
        virtual size_t getUsedVideoMemoryInMb() const override;

        /**
         * Blocks the current thread until the GPU finishes executing all queued commands up to this point.
         *
         * @remark Typically used with @ref getRenderResourcesMutex.
         */
        virtual void waitForGpuToFinishWorkUpToThisPoint() override;

        /**
         * Returns logical device used in the renderer.
         *
         * @return `nullptr` if logical device is not created yet, otherwise used logical device.
         */
        VkDevice getLogicalDevice() const;

        /**
         * Returns physical device used in the renderer.
         *
         * @return `nullptr` if physical device is not created yet, otherwise used physical device.
         */
        VkPhysicalDevice getPhysicalDevice() const;

        /**
         * Returns Vulkan instance used in the renderer.
         *
         * @return `nullptr` if Vulkan instance is not created yet, otherwise used Vulkan instance.
         */
        VkInstance getInstance() const;

        /**
         * Creates a new one-time submit command buffer to be later used with
         * @ref submitWaitDestroyOneTimeSubmitCommandBuffer.
         *
         * @return Error if something went wrong, otherwise created command buffer.
         */
        std::variant<VkCommandBuffer, Error> createOneTimeSubmitCommandBuffer();

        /**
         * Submits a one-time submit command buffer created by @ref createOneTimeSubmitCommandBuffer,
         * then waits for the queue (that recorded commands were submitted to) to be idle and destroys
         * the command buffer.
         *
         * @param pOneTimeSubmitCommandBuffer Command buffer created by @ref createOneTimeSubmitCommandBuffer
         * with recorded commands to submit.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error>
        submitWaitDestroyOneTimeSubmitCommandBuffer(VkCommandBuffer pOneTimeSubmitCommandBuffer);

    protected:
        /**
         * Creates an empty (uninitialized) renderer.
         *
         * @remark Use @ref initialize to initialize the renderer.
         *
         * @param pGameManager GameManager object that owns this renderer.
         */
        VulkanRenderer(GameManager* pGameManager);

        /**
         * Collects array of engine shaders that will be compiled/verified.
         *
         * @remark Automatically called by the base Renderer class at the end of the renderer's
         * initialization.
         *
         * @return Array of shader descriptions to compile.
         */
        virtual std::vector<ShaderDescription> getEngineShadersToCompile() const override;

        /** Submits a new frame to the GPU. */
        virtual void drawNextFrame() override;

        /**
         * Recreates all render buffers to match current settings.
         *
         * @remark Usually called after some renderer setting was changed or window size changed.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> updateRenderBuffers() override;

        /**
         * (Re)creates depth/stencil buffer.
         *
         * @remark Make sure that the old depth/stencil buffer (if was) is not used by the GPU before
         * calling this function.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> createDepthStencilBuffer() override;

        /**
         * Tells whether the renderer is initialized or not.
         *
         * Initialized renderer means that the hardware supports it and it's safe to use renderer
         * functionality such as @ref updateRenderBuffers.
         *
         * @return Whether the renderer is initialized or not.
         */
        virtual bool isInitialized() const override;

    private:
        /** Groups information about swap chain capabilities. */
        struct SwapChainSupportDetails {
            /** Capabilities. */
            VkSurfaceCapabilitiesKHR capabilities;

            /** Supported surface formats. */
            std::vector<VkSurfaceFormatKHR> vSupportedFormats;

            /** Supported presentation modes. */
            std::vector<VkPresentModeKHR> vSupportedPresentModes;
        };

        /**
         * Stores indices of various types of queue families in the array returned by
         * `vkGetPhysicalDeviceQueueFamilyProperties`.
         */
        struct QueueFamilyIndices {
            QueueFamilyIndices() = default;

            /**
             * Tells if all graphics family indices are set or not.
             *
             * @return `true` if all set, `false` otherwise.
             */
            bool isComplete() const {
                return iGraphicsFamilyIndex.has_value() && iPresentFamilyIndex.has_value();
            }

            /** Index of the graphics queue family, empty if not available. */
            std::optional<uint32_t> iGraphicsFamilyIndex;

            /**
             * Index of the queue family that supports window surface presentation,
             * empty if not available.
             */
            std::optional<uint32_t> iPresentFamilyIndex;
        };

        /**
         * Returns names of essential Vulkan instance extensions that the renderer will use.
         *
         * @return Names of Vulkan instance extension.
         */
        static std::variant<std::vector<const char*>, Error> getRequiredVulkanInstanceExtensions();

        /**
         * Checks if the specified physical device supports used device extensions.
         *
         * @param pGpuDevice GPU to check.
         *
         * @return Empty string if the GPU supports all used device extensions, non-empty string
         * with device extension name that the specified GPU does not support, error message if
         * an internal error occurred.
         */
        static std::variant<std::string, Error>
        isGpuSupportsUsedDeviceExtensions(VkPhysicalDevice pGpuDevice);

#if defined(DEBUG)
        /**
         * Makes sure that @ref vUsedValidationLayerNames are supported.
         *
         * @return Error if something went wrong or used validation layers are not supported,
         * otherwise means that all used validation layers are supported.
         */
        [[nodiscard]] static std::optional<Error> makeSureUsedValidationLayersSupported();

        /**
         * Tries to load the extension function and if successful calls it.
         *
         * @param pInstance       Vulkan instance.
         * @param pCreateInfo     Debug messenger creation info.
         * @param pAllocation     Optional allocator.
         * @param pDebugMessenger Created debug messenger.
         *
         * @return Result of the operation.
         */
        static VkResult createDebugUtilsMessengerEXT(
            VkInstance pInstance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            const VkAllocationCallbacks* pAllocator,
            VkDebugUtilsMessengerEXT* pDebugMessenger);

        /**
         * Tries to load the extension function and if successful calls it.
         *
         * @param pInstance       Vulkan instance.
         * @param pDebugMessenger Debug messenger to destroy.
         * @param pAllocator      Optional allocator.
         */
        static void destroyDebugUtilsMessengerEXT(
            VkInstance pInstance,
            VkDebugUtilsMessengerEXT pDebugMessenger,
            const VkAllocationCallbacks* pAllocator);
#endif

        /**
         * Initializes the renderer.
         *
         * @remark This function is usually called after constructing a new empty (uninitialized)
         * Vulkan renderer.
         *
         * @return Error if something went wrong (for ex. if the hardware does not support this
         * renderer).
         */
        [[nodiscard]] std::optional<Error> initialize();

        /**
         * Initializes essential Vulkan entities.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> initializeVulkan();

        /**
         * Creates Vulkan API instance.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createVulkanInstance();

        /**
         * Creates Vulkan window representation object.
         *
         * @remark Should be called after @ref createVulkanInstance but before @ref pickPhysicalDevice
         * as window surface is used when picking a device.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createWindowSurface();

        /**
         * Tells how good the specified GPU suits the renderer's needs.
         *
         * @param pGpuDevice GPU to test.
         *
         * @return Zero if this GPU cannon be used (see logs for description), otherwise GPU's score
         * (to compare with others).
         */
        size_t rateGpuSuitability(VkPhysicalDevice pGpuDevice);

        /**
         * Checks if the specified GPU fits all essential requirements of the renderer.
         *
         * @param pGpu              GPU to test.
         *
         * @return Empty string if the GPU is suitable, non-empty string
         * with description on what the specified GPU does not support, error message if
         * an internal error occurred.
         */
        std::variant<std::string, Error> isDeviceSuitable(VkPhysicalDevice pGpu);

        /**
         * Queries swap chain support details from the specified GPU.
         *
         * @warning Make sure to check that device extension `VK_KHR_SWAPCHAIN_EXTENSION_NAME` is
         * supported before calling this function.
         *
         * @remark Expects @ref pWindowSurface to be valid.
         *
         * @param pGpu GPU to test.
         *
         * @return Error if something went wrong, otherwise swap chain support details.
         */
        std::variant<SwapChainSupportDetails, Error> querySwapChainSupportDetails(VkPhysicalDevice pGpu);

        /**
         * Retrieves the information about used queue families and their indices.
         *
         * @param pGpu GPU to query for queue families.
         *
         * @remark Expects @ref pWindowSurface to be valid.
         *
         * @return Error if something went wrong, otherwise indices of used queue families
         * returned by `vkGetPhysicalDeviceQueueFamilyProperties`.
         */
        std::variant<VulkanRenderer::QueueFamilyIndices, Error>
        queryQueueFamilyIndices(VkPhysicalDevice pGpu);

        /**
         * Checks if the specified GPU supports all used swap chain formats/modes.
         *
         * @warning Make sure to check that device extension `VK_KHR_SWAPCHAIN_EXTENSION_NAME` is
         * supported before calling this function.
         *
         * @remark Expects @ref pWindowSurface to be valid.
         *
         * @param pGpu GPU to test.
         *
         * @return Empty string if the GPU suits swap chain requirements, non-empty string
         * with description on what the specified GPU does not support, error message if
         * an internal error occurred.
         */
        std::variant<std::string, Error> isGpuSupportsSwapChain(VkPhysicalDevice pGpu);

        /**
         * Picks the first GPU that fits renderer's needs.
         *
         * @warning Expects @ref pInstance to be valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> pickPhysicalDevice();

        /**
         * Creates @ref pLogicalDevice.
         *
         * @warning Expects @ref pPhysicalDevice to be valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createLogicalDevice();

        /**
         * Creates @ref pSwapChain.
         *
         * @warning Expects @ref pPhysicalDevice to be valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createSwapChain();

        /**
         * Creates @ref pCommandPool.
         *
         * @warning Expects @ref pLogicalDevice to be valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createCommandPool();

        /**
         * Creates an image view to the specified image.
         *
         * @param pImage      Image to create a view for.
         * @param iTextureMipLevelCount The number of mip level the texture has.
         * @param imageFormat Image format.
         * @param aspectFlags Bitmask specifying which aspects of an image are included in a view.
         *
         * @return Error if something went wrong, otherwise created image view.
         */
        std::variant<VkImageView, Error> createImageView(
            VkImage pImage,
            uint32_t iTextureMipLevelCount,
            VkFormat imageFormat,
            VkImageAspectFlags aspectFlags);

        /**
         * Chooses the appropriate swap chain size.
         *
         * @param surfaceCapabilities Physical device surface's swap chain surface capabilities.
         *
         * @return Error if something went wrong, otherwise swap chain size to use.
         */
        std::variant<VkExtent2D, Error>
        pickSwapChainExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

        /**
         * Creates semaphores and fences used for synchronizing rendering operations.
         *
         * @warning Expects @ref pLogicalDevice to be valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createSynchronizationObjects();

        /**
         * Creates @ref pRenderPass using the current @ref msaaSampleCount.
         *
         * @warning Expects @ref pLogicalDevice to be valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createRenderPass();

        /**
         * Destroys swap chain, framebuffers, graphics pipeline, render pass, image views,
         * frees command buffers and other objects that depend on the swap chain images.
         */
        void destroySwapChainAndDependentResources();

        /** Vulkan API instance. */
        VkInstance pInstance = nullptr;

        /** Vulkan window representation. */
        VkSurfaceKHR pWindowSurface = nullptr;

        /** GPU that is being used by this renderer. */
        VkPhysicalDevice pPhysicalDevice = nullptr;

        /** Logical device to interface with @ref pPhysicalDevice. */
        VkDevice pLogicalDevice = nullptr;

        /** Graphics queue. */
        VkQueue pGraphicsQueue = nullptr;

        /** Presentation queue. */
        VkQueue pPresentQueue = nullptr;

        /** Swap chain. */
        VkSwapchainKHR pSwapChain = nullptr;

        /** Depth buffer. */
        VkImage pDepthImage = nullptr;

        /** Render pass. */
        VkRenderPass pRenderPass = nullptr;

        /** Used to create command buffers. */
        VkCommandPool pCommandPool = nullptr;

#if defined(DEBUG)
        /**
         * Debug messenger to use our custom callback function instead of printing validation layer
         * errors to stdout.
         */
        VkDebugUtilsMessengerEXT pValidationLayerDebugMessenger = nullptr;
#endif

        /** Swap chain images. */
        std::array<VkImage, getSwapChainBufferCount()> vSwapChainImages;

        /** Views to @ref vSwapChainImages. */
        std::array<VkImageView, getSwapChainBufferCount()> vSwapChainImageViews;

        /**
         * Semaphores to signal that an image from the swapchain was acquired and is ready
         * for rendering.
         */
        std::array<VkSemaphore, FrameResourcesManager::getFrameResourcesCount()>
            vSemaphoreSwapChainImageReadyForRendering;

        /**
         * Semaphores to signal that the rendering to a swapchain image was finished and the image is now
         * ready for presenting.
         */
        std::array<VkSemaphore, FrameResourcesManager::getFrameResourcesCount()>
            vSemaphoreSwapChainImageReadyForPresenting;

        /**
         * Fences used to avoid waiting for the GPU after the CPU
         * has submitted a frame. With these fences we can now submit up to `iFrameResourceCount` frames
         * without waiting for the GPU.
         */
        std::array<VkFence, FrameResourcesManager::getFrameResourcesCount()> vFences;

        /** List of supported GPUs, filled during @ref pickPhysicalDevice. */
        std::vector<std::string> vSupportedGpuNames;

        /** Queue family indices of current @ref pPhysicalDevice. */
        QueueFamilyIndices physicalDeviceQueueFamilyIndices;

        /** Size of the images in the swap chain. */
        VkExtent2D swapChainExtent;

        /** Used MSAA sample count. */
        VkSampleCountFlagBits msaaSampleCount = VK_SAMPLE_COUNT_8_BIT;

        /** Tells if @ref initializeVulkan was finished successfully or not. */
        bool bIsVulkanInitialized = false;

        /** Index of the color attachment in @ref pRenderPass. */
        static constexpr size_t iRenderPassColorAttachmentIndex = 0;

        /** Index of the depth attachment in @ref pRenderPass. */
        static constexpr size_t iRenderPassDepthAttachmentIndex = 1;

        /** Index of the color resolve attachment in @ref pRenderPass. */
        static constexpr size_t iRenderPassColorResolveAttachmentIndex = 2;

        /** Format of @ref vSwapChainImages. */
        static constexpr VkFormat swapChainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;

        /** Format of @ref pDepthImage. */
        static constexpr VkFormat depthImageFormat = VK_FORMAT_D24_UNORM_S8_UINT;

        /** Color space of @ref vSwapChainImages. */
        static constexpr VkColorSpaceKHR swapChainImageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        /** Version of the Vulkan API that the renderer uses. */
        static constexpr uint32_t iUsedVulkanVersion = VK_API_VERSION_1_0;

        /** Array of physical device extensions that we use. */
        static inline const std::vector<const char*> vUsedDeviceExtensionNames = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#if defined(DEBUG)
        /**
         * Used validation layers.
         *
         * @remark Use @ref makeSureUsedValidationLayersSupported to check if they are supported.
         */
        static inline const std::vector<const char*> vUsedValidationLayerNames = {
            "VK_LAYER_KHRONOS_validation"};
#endif
    };
} // namespace ne
