#pragma once

// Custom.
#include "render/Renderer.h"
#include "render/general/resources/FrameResourcesManager.h"
#include "render/vulkan/resources/VulkanResource.h"

// External.
#include "vulkan/vulkan.h"
#include "VulkanMemoryAllocator/include/vk_mem_alloc.h"

namespace ne {
    class CameraProperties;
    class Material;
    class VulkanPipeline;
    struct VulkanFrameResource;

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
         * @param vBlacklistedGpuNames Names of GPUs that should not be used, generally this means
         * that these GPUs were previously used to create the renderer but something went wrong.
         *
         * @return Created renderer if successful, otherwise multiple values in a pair: error and a
         * name of the GPU that the renderer tried to use (can be empty if failed before picking a GPU
         * or if all supported GPUs are blacklisted).
         */
        static std::variant<std::unique_ptr<Renderer>, std::pair<Error, std::string>>
        create(GameManager* pGameManager, const std::vector<std::string>& vBlacklistedGpuNames);

        /**
         * Looks for video adapters (GPUs) that support this renderer.
         *
         * @remark Note that returned array might differ depending on the used renderer.
         *
         * @return Empty array if no GPU supports used renderer,
         * otherwise array with GPU names that can be used for this renderer.
         */
        virtual std::vector<std::string> getSupportedGpuNames() const override;

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
         * Blocks the current thread until the GPU finishes executing all queued commands up to this point.
         *
         * @remark Typically used with @ref getRenderResourcesMutex.
         */
        virtual void waitForGpuToFinishWorkUpToThisPoint() override;

        /**
         * Creates a new one-time submit command buffer to be later used with
         * @ref submitWaitDestroyOneTimeSubmitCommandBuffer.
         *
         * @return Error if something went wrong, otherwise created command buffer.
         */
        std::variant<VkCommandBuffer, Error> createOneTimeSubmitCommandBuffer();

        /**
         * Submits a one-time submit command buffer created by @ref createOneTimeSubmitCommandBuffer,
         * then waits for a temporary fence to be signaled (meaning that submitted commands were executed
         * on the GPU) and destroys the command buffer.
         *
         * @param pOneTimeSubmitCommandBuffer Command buffer created by @ref createOneTimeSubmitCommandBuffer
         * with recorded commands to submit.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error>
        submitWaitDestroyOneTimeSubmitCommandBuffer(VkCommandBuffer pOneTimeSubmitCommandBuffer);

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
         * Returns Vulkan render pass used in the renderer.
         *
         * @return `nullptr` if render pass is not created yet, otherwise used render pass.
         */
        VkRenderPass getRenderPass() const;

        /**
         * Returns Vulkan command pool used in the renderer.
         *
         * @return `nullptr` if command pool is not created yet, otherwise used command pool.
         */
        VkCommandPool getCommandPool() const;

        /**
         * Returns the size of images in the swap chain.
         *
         * @return Empty if the swap chain is not initialized, otherwise the size of images in
         * the swap chain.
         */
        std::optional<VkExtent2D> getSwapChainExtent() const;

        /**
         * Returns sample count of the current MSAA quality.
         *
         * @return MSAA sample count.
         */
        VkSampleCountFlagBits getMsaaSampleCount() const;

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
         * Returns the maximum anti-aliasing quality that can be used on the picked
         * GPU (@ref getCurrentlyUsedGpuName).
         *
         * @remark Note that the maximum supported AA quality can differ depending on the used GPU/renderer.
         *
         * @return Error if something went wrong,
         * otherwise `DISABLED` if AA is not supported or the maximum supported AA quality.
         */
        virtual std::variant<MsaaState, Error> getMaxSupportedAntialiasingQuality() const override;

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
         * Called when the framebuffer size was changed.
         *
         * @param iWidth  New width of the framebuffer (in pixels).
         * @param iHeight New height of the framebuffer (in pixels).
         */
        virtual void onFramebufferSizeChanged(int iWidth, int iHeight) override;

        /**
         * Called after some render setting is changed to recreate internal resources to match the current
         * settings.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> onRenderSettingsChanged() override;

        /**
         * Blocks the current thread until the GPU is finished using the specified frame resource.
         *
         * @remark Generally the current frame resource will be passed and so the current frame resource
         * mutex will be locked at the time of calling and until the function is not finished it will not
         * be unlocked.
         *
         * @param pFrameResource Frame resource to wait for.
         */
        virtual void waitForGpuToFinishUsingFrameResource(FrameResource* pFrameResource) override;

        /**
         * Tells whether the renderer is initialized or not.
         *
         * Initialized renderer means that the hardware supports it and it's safe to use renderer
         * functionality such as @ref onRenderSettingsChanged.
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
         * @param vBlacklistedGpuNames Names of GPUs that should not be used, generally this means
         * that these GPUs were previously used to create the renderer but something went wrong.
         *
         * @return Error if something went wrong (for ex. if the hardware does not support this
         * renderer).
         */
        [[nodiscard]] std::optional<Error> initialize(const std::vector<std::string>& vBlacklistedGpuNames);

        /**
         * Initializes essential Vulkan entities.
         *
         * @param vBlacklistedGpuNames Names of GPUs that should not be used, generally this means
         * that these GPUs were previously used to create the renderer but something went wrong.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error>
        initializeVulkan(const std::vector<std::string>& vBlacklistedGpuNames);

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
         * @param vBlacklistedGpuNames Names of GPUs that should not be used, generally this means
         * that these GPUs were previously used to create the renderer but something went wrong.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error>
        pickPhysicalDevice(const std::vector<std::string>& vBlacklistedGpuNames);

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
         * Chooses the appropriate swap chain size.
         *
         * @param surfaceCapabilities Physical device surface's swap chain surface capabilities.
         *
         * @return Error if something went wrong, otherwise swap chain size to use.
         */
        std::variant<VkExtent2D, Error>
        pickSwapChainExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

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
         *
         * @param bDestroyPipelineManager `true` to destroy the pipeline manager, this is generally
         * used when the renderer is being destroyed, otherwise if you just want to recreate
         * some resources specify `false` and make sure all pipeline resources were released and will not be
         * restored before this function is finished.
         */
        void destroySwapChainAndDependentResources(bool bDestroyPipelineManager);

        /**
         * Creates @ref pTextureSampler using the current texture filtering mode from the render settings.
         *
         * @warning Expects that @ref pLogicalDevice is valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createTextureSampler();

        /**
         * (Re)binds @ref pTextureSampler to sampler descriptors in all graphics pipelines.
         *
         * @warning Expects that @ref pLogicalDevice is valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> updateTextureSamplerDescriptors();

        /**
         * Tells if @ref depthImageFormat is supported by the hardware.
         *
         * @return `true` if supported, `false` otherwise.
         */
        bool isUsedDepthImageFormatSupported();

        /**
         * Creates @ref pDepthImage.
         *
         * @warning Expects that GPU resource manager and @ref swapChainExtent are valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createDepthImage();

        /**
         * Creates @ref pMsaaImage using the current @ref msaaSampleCount (does nothing if only 1 sample
         * is used).
         *
         * @warning Expects that GPU resource manager and @ref swapChainExtent are valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createMsaaImage();

        /**
         * Recreates @ref pSwapChain and all dependent resources after it was created using @ref
         * createSwapChain.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> recreateSwapChainAndDependentResources();

        /**
         * Creates @ref vSwapChainFramebuffers.
         *
         * @remark Expects that @ref vSwapChainImageViews, @ref pRenderPass, @ref pDepthImage and
         * @ref pMsaaImage are valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createSwapChainFramebuffers();

        /**
         * Setups everything for render commands to be recorded (updates frame constants, shader resources,
         * resets command buffers, starts render pass and etc.).
         *
         * @param pCameraProperties Camera properties to use.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> prepareForDrawingNextFrame(CameraProperties* pCameraProperties);

        /**
         * Adds draw commands to command buffer to draw all mesh nodes that use the specified material
         *
         * @param pMaterial                  Material that mesh nodes use.
         * @param pPipeline                  Pipeline that the material uses.
         * @param pCommandBuffer             Command buffer to add commands to.
         * @param iCurrentFrameResourceIndex Index of the current frame resource.
         */
        static void drawMeshNodes(
            Material* pMaterial,
            VulkanPipeline* pPipeline,
            VkCommandBuffer pCommandBuffer,
            size_t iCurrentFrameResourceIndex);

        /**
         * Does final logic in drawing next frame (ends render pass, ends command buffer, etc.).
         *
         * @param pCurrentFrameResource Current frame resource. Expects that frame resources mutex is
         * locked and will not be unlocked until the function is finished.
         * @param iCurrentFrameResourceIndex Index of the current frame resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error>
        finishDrawingNextFrame(VulkanFrameResource* pCurrentFrameResource, size_t iCurrentFrameResourceIndex);

        /**
         * Queries the current render settings for MSAA quality and updates @ref msaaSampleCount.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> updateMsaaSampleCount();

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
        std::unique_ptr<VulkanResource> pDepthImage = nullptr;

        /** Image with multiple samples per pixel for MSAA. */
        std::unique_ptr<VulkanResource> pMsaaImage = nullptr;

        /** Render pass. */
        VkRenderPass pRenderPass = nullptr;

        /** Used to create command buffers. */
        VkCommandPool pCommandPool = nullptr;

        /** Texture sampler. */
        VkSampler pTextureSampler = nullptr;

#if defined(DEBUG)
        /**
         * Debug messenger to use our custom callback function instead of printing validation layer
         * errors to stdout.
         */
        VkDebugUtilsMessengerEXT pValidationLayerDebugMessenger = nullptr;
#endif

        /**
         * Swap chain images.
         *
         * @remark Size of this array is equal to @ref iSwapChainImageCount.
         */
        std::vector<VkImage> vSwapChainImages;

        /**
         * Views to @ref vSwapChainImages.
         *
         * @remark Size of this array is equal to @ref iSwapChainImageCount.
         */
        std::vector<VkImageView> vSwapChainImageViews;

        /**
         * Framebuffers that point to @ref vSwapChainImageViews.
         *
         * @remark Size of this array is equal to @ref iSwapChainImageCount.
         */
        std::vector<VkFramebuffer> vSwapChainFramebuffers;

        /**
         * Stores references to fences from frame resources.
         *
         * @remark Used to synchronize `vkAcquireNextImageKHR` calls and wait for a frame resource that uses
         * the swap chain image.
         *
         * @remark Size of this array is equal to @ref iSwapChainImageCount.
         */
        std::vector<VkFence> vSwapChainImageFenceRefs;

        /** List of supported GPUs, filled during @ref pickPhysicalDevice. */
        std::vector<std::string> vSupportedGpuNames;

        /** Name of the @ref pPhysicalDevice. */
        std::string sUsedGpuName;

        /** Queue family indices of current @ref pPhysicalDevice. */
        QueueFamilyIndices physicalDeviceQueueFamilyIndices;

        /** Size of images in the swap chain. */
        std::optional<VkExtent2D> swapChainExtent;

        /** Used MSAA sample count. */
        VkSampleCountFlagBits msaaSampleCount = VK_SAMPLE_COUNT_1_BIT;

        /** The number of swap chain images that we have in @ref pSwapChain. */
        uint32_t iSwapChainImageCount = 0;

        /** Tells if @ref initializeVulkan was finished successfully or not. */
        bool bIsVulkanInitialized = false;

        /** Whether the window that owns the renderer is minimized or not. */
        bool bIsWindowMinimized = false;

        /** Marked as `true` when entered destructor. */
        bool bIsBeingDestroyed = false;

        /** Whether or not we logged that acquired swap chain image index is not expected. */
        bool bLoggedAboutUnexpectedSwapChainImageAcquired = false;

        /** Index of the color attachment in @ref pRenderPass. */
        static constexpr size_t iRenderPassColorAttachmentIndex = 0;

        /** Index of the depth attachment in @ref pRenderPass. */
        static constexpr size_t iRenderPassDepthAttachmentIndex = 1;

        /** Index of the color resolve target attachment in @ref pRenderPass. */
        static constexpr size_t iRenderPassColorResolveTargetAttachmentIndex = 2;

        /** Format of @ref vSwapChainImages. */
        static constexpr VkFormat swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

        /** Color space of @ref vSwapChainImages. */
        static constexpr VkColorSpaceKHR swapChainImageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        /** Format of @ref pDepthImage. */
        static constexpr VkFormat depthImageFormat = VK_FORMAT_D24_UNORM_S8_UINT;

        /** Tiling option for @ref pDepthImage. */
        static constexpr VkImageTiling depthImageTiling = VK_IMAGE_TILING_OPTIMAL;

        /** Format of indices. */
        static constexpr VkIndexType indexTypeFormat = VK_INDEX_TYPE_UINT32;

        /** Version of the Vulkan API that the renderer uses. */
        static constexpr uint32_t iUsedVulkanVersion = VK_API_VERSION_1_1;

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
