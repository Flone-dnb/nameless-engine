#pragma once

// Standard.
#include <vector>

// Custom.
#include "render/Renderer.h"
#include "render/general/resource/frame/FrameResourceManager.h"
#include "render/vulkan/resource/VulkanResource.h"
#include "material/Material.h"

// External.
#include "vulkan/vulkan.h"
#include "VulkanMemoryAllocator/include/vk_mem_alloc.h"

namespace ne {
    class CameraProperties;
    class VulkanPipeline;
    struct VulkanFrameResource;
    struct QueuedForExecutionComputeShaders;
    class ComputeShaderInterface;
    class MeshNode;
    class Pipeline;

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
         * Returns format used for depth image.
         *
         * @return Format.
         */
        static constexpr VkFormat getDepthImageFormat() { return depthImageFormat; }

        /**
         * Returns texture format used for shadow maps.
         *
         * @return Shadow map format.
         */
        static constexpr VkFormat getShadowMapFormat() { return shadowMapFormat; }

        /**
         * Returns texture format used for point lights as "color" target (does not actually store
         * color) during shadow pass.
         *
         * @return Shadow map format.
         */
        static constexpr VkFormat getShadowMappingPointLightColorTargetFormat() {
            return shadowMappingPointLightColorTargetFormat;
        }

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
         * Sets name of the object for debugging purposes using Vulkan's debugging utils extension.
         *
         * @remark Does nothing in release builds.
         *
         * @param pRenderer     Vulkan renderer.
         * @param pObject       Object to name.
         * @param objectType    Type of the object.
         * @param sResourceName Name to set.
         */
        static void setObjectDebugOnlyName(
            Renderer* pRenderer, void* pObject, VkObjectType objectType, const std::string& sResourceName);

        /**
         * Returns a texture sampler the specified texture filtering option.
         *
         * @remark Returned pointer will always be valid (after the sampler was created) until the renderer is
         * destroyed.
         *
         * @param filtering Texture filtering mode.
         *
         * @return `nullptr` if not created yet, otherwise valid pointer.
         */
        VkSampler getTextureSampler(TextureFilteringQuality filtering);

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
         * @remark Typically used while @ref getRenderResourcesMutex is locked.
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
         * Creates a one-time submit command buffer to change image layout.
         *
         * @param pImage      Image to use.
         * @param imageFormat Image format.
         * @param aspect      Aspect of the image that will be affected.
         * @param levelCount  Defines how much mipmaps will be affected.
         * @param layerCount  Defines how much image layers will be affected.
         * @param oldLayout   Old (current) image layout.
         * @param newLayout   New image layout.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> transitionImageLayout(
            VkImage pImage,
            VkFormat imageFormat,
            VkImageAspectFlags aspect,
            uint32_t levelCount,
            uint32_t layerCount,
            VkImageLayout oldLayout,
            VkImageLayout newLayout);

        /**
         * Returns size of the render target (size of the underlying render image).
         *
         * @return Render image size in pixels (width and height).
         */
        virtual std::pair<unsigned int, unsigned int> getRenderTargetSize() const override;

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
         * Returns main render pass.
         *
         * @return `nullptr` if render pass is not created yet, otherwise valid pointer.
         */
        VkRenderPass getMainRenderPass() const;

        /**
         * Returns depth only render pass (z-prepass).
         *
         * @return `nullptr` if render pass is not created yet, otherwise valid pointer.
         */
        VkRenderPass getDepthOnlyRenderPass() const;

        /**
         * Returns render pass used for shadow mapping.
         *
         * @param bIsForPointLights Specify `false` if you need render pass for shadow mapping of
         * directional and spot lights, otherwise specify `true` to get render pass for shadow mapping
         * of point lights.
         *
         * @return `nullptr` if render pass is not created yet, otherwise valid pointer.
         */
        VkRenderPass getShadowMappingRenderPass(bool bIsForPointLights) const;

        /**
         * Returns Vulkan command pool used in the renderer.
         *
         * @return `nullptr` if command pool is not created yet, otherwise used command pool.
         */
        VkCommandPool getCommandPool() const;

        /**
         * Returns Vulkan graphics queue used in the renderer.
         *
         * @return `nullptr` if graphics queue is not created yet, otherwise used graphics queue.
         */
        VkQueue getGraphicsQueue() const;

        /**
         * Returns Vulkan texture sampler for fetching texels in compute shaders.
         *
         * @remark Used for compute shaders that need to read textures.
         *
         * @remark Guaranteed to never be re-created.
         *
         * @return `nullptr` if not created yet, otherwise valid sampler.
         */
        VkSampler getComputeTextureSampler() const;

        /**
         * Returns Vulkan texture sampler for sampling shadow textures.
         *
         * @remark Guaranteed to never be re-created.
         *
         * @return `nullptr` if not created yet, otherwise valid sampler.
         */
        VkSampler getShadowTextureSampler() const;

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

        /**
         * Returns pointer to the texture resource that represents renderer's depth texture
         * without multisampling (resolved resource).
         *
         * @warning If MSAA is enabled this function will return one resource (pointer to a separate
         * depth resolved resource), if it's disabled it will return the other resource (pointer to depth
         * texture). So it may be a good idea to query this pointer every time you need it instead of saving
         * it and reusing it because every frame this pointer may change (due to other reasons such as
         * render target resize and etc).
         *
         * @return Pointer to depth texture.
         */
        virtual GpuResource* getDepthTextureNoMultisampling() override;

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
         * @return Error if something went wrong, otherwise empty if AA is not supported.
         */
        virtual std::variant<std::optional<AntialiasingQuality>, Error>
        getMaxSupportedAntialiasingQuality() const override;

        /**
         * Called when the framebuffer size was changed.
         *
         * @param iWidth  New width of the framebuffer (in pixels).
         * @param iHeight New height of the framebuffer (in pixels).
         */
        virtual void onFramebufferSizeChangedDerived(int iWidth, int iHeight) override;

        /**
         * Called after some render setting is changed to recreate internal resources to match the current
         * settings.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> onRenderSettingsChangedDerived() override;

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

        /** Groups semaphores related to swap chain images. */
        struct SwapChainImageSemaphores {
            /**
             * Array of semaphores that are passed to `vkAcquireNextImageKHR`.
             *
             * @remark Size of this array is equal to the number of swap chain images.
             */
            VkSemaphore pAcquireImageSemaphore = nullptr;

            /**
             * Array of semaphores that are passed to `vkQueueSubmit`.
             *
             * @remark Size of this array is equal to the number of swap chain images.
             */
            VkSemaphore pQueueSubmitSemaphore = nullptr;

            /**
             * Index of the frame resource to wait for its fence before using `vkAcquireNextImageKHR`
             * to guarantee that @ref pAcquireImageSemaphore is in the unsignaled state.
             */
            size_t iUsedFrameResourceIndex = 0;
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

        /**
         * Adds render pass start commands to the specified command buffer with the specified
         * shadow mapping render pass.
         *
         * @param pShadowMappingRenderPass @ref pShadowMappingDirectionalSpotRenderPass or @ref
         * pShadowMappingPointRenderPass.
         * @param pCommandBuffer           Command buffer to modify.
         * @param pFramebufferToUse        Framebuffer to use.
         * @param iShadowMapSize           Size of the framebuffer image.
         */
        static void startShadowMappingRenderPass(
            VkRenderPass pShadowMappingRenderPass,
            VkCommandBuffer pCommandBuffer,
            VkFramebuffer pFramebufferToUse,
            uint32_t iShadowMapSize);

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

        /**
         * Tries to load the extension function and return an address to it.
         *
         * @param pInstance      Vulkan instance.
         *
         * @return Pointer to the extension function.
         */
        static PFN_vkCmdBeginDebugUtilsLabelEXT requestVkCmdBeginDebugUtilsLabelEXT(VkInstance pInstance);

        /**
         * Tries to load the extension function and return an address to it.
         *
         * @param pInstance      Vulkan instance.
         *
         * @param Pointer to the extension function.
         */
        static PFN_vkCmdEndDebugUtilsLabelEXT requestVkCmdEndDebugUtilsLabelEXT(VkInstance pInstance);
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
         * Creates render passes using the current @ref msaaSampleCount.
         *
         * @warning Expects @ref pLogicalDevice to be valid.
         *
         * @param bIsRendererInitialization Specify `true` if the renderer is doing initialization,
         * `false` if some render settings (or similar) was changed and the renderer re-creates
         * resources that may depend on changed settings/parameters.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createRenderPasses(bool bIsRendererInitialization);

        /**
         * Creates @ref pMainRenderPass using the current @ref msaaSampleCount.
         *
         * @warning Expects @ref pLogicalDevice to be valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createMainRenderPass();

        /**
         * Creates @ref pDepthOnlyRenderPass (z-prepass) using the current @ref msaaSampleCount.
         *
         * @warning Expects @ref pLogicalDevice to be valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createDepthOnlyRenderPass();

        /**
         * Creates render passes used in shadow mapping.
         *
         * @warning Expects @ref pLogicalDevice to be valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createShadowMappingRenderPasses();

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
         * Creates texture samplers that will never be recreated (destroyed during renderer's destruction).
         *
         * @warning Expects that @ref pLogicalDevice is valid.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createTextureSamplers();

        /**
         * Creates @ref pComputeTextureSampler.
         *
         * @warning Expects that @ref pLogicalDevice is valid.
         *
         * @warning Expected to be called only once so that this sampler will never be re-created.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createComputeTextureSampler();

        /**
         * Creates sampler for shadow mapping.
         *
         * @warning Expects that @ref pLogicalDevice is valid.
         *
         * @warning Expected to be called only once so that this sampler will never be re-created.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createShadowTextureSampler();

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
         * Creates swap chain framebuffers.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createSwapChainFramebuffers();

        /**
         * Called before @ref prepareForDrawingNextFrame to do early frame preparations.
         *
         * @remark It's expected that render target's size will not change after this function is finished
         * and before a new frame is submitted.
         */
        virtual void prepareRenderTargetForNextFrame() override;

        /**
         * Setups everything for render commands to be recorded (resets command buffers and etc.).
         *
         * @warning Expects that render resources mutex is locked.
         *
         * @remark When this function is called this means that the current frame resource is no longer
         * used by the GPU.
         *
         * @param pCameraProperties     Camera properties to use.
         * @param pCurrentFrameResource Frame resource of the frame being submitted.
         */
        virtual void prepareForDrawingNextFrame(
            CameraProperties* pCameraProperties, FrameResource* pCurrentFrameResource) override;

        /**
         * Adds render pass start commands to the specified command buffer with @ref pMainRenderPass.
         *
         * @param pCommandBuffer      Command buffer to modify.
         * @param iAcquiredImageIndex Index of the framebuffer to use.
         */
        void startMainRenderPass(VkCommandBuffer pCommandBuffer, size_t iAcquiredImageIndex);

        /**
         * Adds render pass start commands to the specified command buffer with @ref pDepthOnlyRenderPass.
         *
         * @param pCommandBuffer      Command buffer to modify.
         * @param iAcquiredImageIndex Index of the framebuffer to use.
         */
        void startDepthOnlyRenderPass(VkCommandBuffer pCommandBuffer, size_t iAcquiredImageIndex);

        /**
         * Submits commands to draw world from the perspective of all spawned light sources to capture
         * shadow maps.
         *
         * @param pCurrentFrameResource      Frame resource of the frame being submitted.
         * @param iCurrentFrameResourceIndex Index of the current frame resource.
         * @param pGraphicsPipelines         Graphics pipelines to draw.
         */
        virtual void drawShadowMappingPass(
            FrameResource* pCurrentFrameResource,
            size_t iCurrentFrameResourceIndex,
            GraphicsPipelineRegistry* pGraphicsPipelines) override;

        /**
         * Submits commands to draw meshes and the specified depth only (vertex shader only) pipelines.
         *
         * @param pCurrentFrameResource      Frame resource of the frame being submitted.
         * @param iCurrentFrameResourceIndex Index of the current frame resource.
         * @param vOpaquePipelines           Opaque pipelines (depth pipeline will be retrieved from them).
         */
        virtual void drawMeshesDepthPrepass(
            FrameResource* pCurrentFrameResource,
            size_t iCurrentFrameResourceIndex,
            const std::vector<Renderer::MeshesInFrustum::PipelineInFrustumInfo>& vOpaquePipelines) override;

        /**
         * Executes compute shaders of the specified stage.
         *
         * @warning Expects that mutex for compute shaders is locked.
         *
         * @param pCurrentFrameResource      Frame resource of the frame being submitted.
         * @param iCurrentFrameResourceIndex Index of the current frame resource.
         * @param stage                      Stage of compute shaders to execute.
         */
        virtual void executeComputeShadersOnGraphicsQueue(
            FrameResource* pCurrentFrameResource,
            size_t iCurrentFrameResourceIndex,
            ComputeExecutionStage stage) override;

        /**
         * Submits commands to draw meshes for main (color) pass.
         *
         * @param pCurrentFrameResource       Frame resource of the frame being submitted.
         * @param iCurrentFrameResourceIndex  Index of the current frame resource.
         * @param vOpaquePipelines            Opaque pipelines to draw.
         * @param vTransparentPipelines       Transparent pipelines to draw.
         */
        virtual void drawMeshesMainPass(
            FrameResource* pCurrentFrameResource,
            size_t iCurrentFrameResourceIndex,
            const std::vector<Renderer::MeshesInFrustum::PipelineInFrustumInfo>& vOpaquePipelines,
            const std::vector<Renderer::MeshesInFrustum::PipelineInFrustumInfo>& vTransparentPipelines)
            override;

        /**
         * Does the final frame rendering logic to present the frame on the screen.
         *
         * @param pCurrentFrameResource       Frame resource of the frame being submitted.
         * @param iCurrentFrameResourceIndex  Index of the current frame resource.
         */
        virtual void
        present(FrameResource* pCurrentFrameResource, size_t iCurrentFrameResourceIndex) override;

        /**
         * Submits commands to draw meshes and pipelines of specific types (only opaque or transparent).
         *
         * @param pipelinesOfSpecificType    Pipelines to use.
         * @param pCommandBuffer             Command buffer to use.
         * @param iCurrentFrameResourceIndex Index of the current frame resource.
         */
        void drawMeshesMainPassSpecificPipelines(
            const std::vector<Renderer::MeshesInFrustum::PipelineInFrustumInfo>& pipelinesOfSpecificType,
            VkCommandBuffer pCommandBuffer,
            size_t iCurrentFrameResourceIndex);

        /**
         * Queries the current render settings for MSAA quality and updates @ref msaaSampleCount.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> updateMsaaSampleCount();

        /**
         * Submits compute dispatch commands using @ref pGraphicsQueue.
         *
         * @param pCurrentFrameResource      Current frame resource.
         * @param iCurrentFrameResourceIndex Index of the current frame resource.
         * @param computePipelinesToSubmit   Compute shaders and their pipelines to dispatch.
         *
         * @return `true` if dispatched some shaders, `false` if nothing to dispatch.
         */
        static bool dispatchComputeShadersOnGraphicsQueue(
            VulkanFrameResource* pCurrentFrameResource,
            size_t iCurrentFrameResourceIndex,
            std::unordered_map<Pipeline*, std::unordered_set<ComputeShaderInterface*>>&
                computePipelinesToSubmit);

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

        /**
         * Depth buffer without multisampling (for light culling compute shader).
         *
         * @warning When @ref pDepthImage does not use multisampling this buffer is not used
         * and does not store contents of @ref pDepthImage.
         *
         * @remark Stores non-multisampled depth data from @ref pDepthImage for shaders.
         */
        std::unique_ptr<VulkanResource> pDepthImageNoMultisampling = nullptr;

        /** Image with multiple samples per pixel for MSAA. */
        std::unique_ptr<VulkanResource> pMsaaImage = nullptr;

        /** Render pass for z-prepass. */
        VkRenderPass pDepthOnlyRenderPass = nullptr;

        /** Render pass for main pass. */
        VkRenderPass pMainRenderPass = nullptr;

        /** Render pass for shadow mapping of directional and spot lights. */
        VkRenderPass pShadowMappingDirectionalSpotRenderPass = nullptr;

        /** Render pass for shadow mapping of point lights. */
        VkRenderPass pShadowMappingPointRenderPass = nullptr;

        /** Used to create command buffers. */
        VkCommandPool pCommandPool = nullptr;

        /**
         * Texture sampler with point filtering.
         *
         * @remark Will never be recreated (destroyed during renderer's destruction).
         */
        VkSampler pTextureSamplerPointFiltering = nullptr;

        /**
         * Texture sampler with linear filtering.
         *
         * @remark Will never be recreated (destroyed during renderer's destruction).
         */
        VkSampler pTextureSamplerLinearFiltering = nullptr;

        /**
         * Texture sampler with anisotropic filtering.
         *
         * @remark Will never be recreated (destroyed during renderer's destruction).
         */
        VkSampler pTextureSamplerAnisotropicFiltering = nullptr;

        /**
         * Texture sampler with nearest filtering and mipmapping for fetching texels in compute shader.
         *
         * @remark Always valid and not re-created when texture filtering (render setting) is changed.
         */
        VkSampler pComputeTextureSampler = nullptr;

        /**
         * Texture sampler for directional and spot shadow maps.
         *
         * @remark Always valid and not re-created when texture filtering (render setting) is changed.
         */
        VkSampler pShadowTextureSampler = nullptr;

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
         * Framebuffers that point to @ref vSwapChainImageViews and reference main render pass.
         *
         * @remark Size of this array is equal to @ref iSwapChainImageCount.
         */
        std::vector<VkFramebuffer> vSwapChainFramebuffersMainRenderPass;

        /**
         * Framebuffers that point to @ref vSwapChainImageViews and reference depth only render pass.
         *
         * @remark Size of this array is equal to @ref iSwapChainImageCount.
         */
        std::vector<VkFramebuffer> vSwapChainFramebuffersDepthOnlyRenderPass;

        /**
         * Stores pairs of "references to fences of a frame resources" - "frame resource index".
         *
         * @remark Used to synchronize `vkAcquireNextImageKHR` calls and wait for a frame resource that uses
         * the swap chain image.
         *
         * @remark Size of this array is equal to @ref iSwapChainImageCount.
         */
        std::vector<std::pair<VkFence, size_t>> vSwapChainImageFenceRefs;

        /**
         * Semaphores related to swap chain images.
         *
         * @remark Size of this array is equal to @ref iSwapChainImageCount.
         */
        std::vector<SwapChainImageSemaphores> vImageSemaphores;

        /** Index into @ref vImageSemaphores. */
        size_t iCurrentImageSemaphore = 0;

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

        /** Index of the last acquired image from the swap chain. */
        uint32_t iLastAcquiredImageIndex = 0;

        /** Tells if @ref initializeVulkan was finished successfully or not. */
        bool bIsVulkanInitialized = false;

        /**
         * `true` if we received `VK_SUBOPTIMAL_KHR` and need to re-create the swapchain,
         * `false` otherwise.
         */
        bool bNeedToRecreateSwapchain = false;

        /** Tells if MSAA is enabled or not and whether we are using multisampled render target or not. */
        bool bIsUsingMsaaRenderTarget = false;

        /** Marked as `true` when entered destructor. */
        bool bIsBeingDestroyed = false;

        /** Index of the color attachment in @ref pMainRenderPass. */
        static constexpr size_t iMainRenderPassColorAttachmentIndex = 0;

        /** Index of the depth attachment in @ref pMainRenderPass. */
        static constexpr size_t iMainRenderPassDepthAttachmentIndex = 1;

        /** Index of the color resolve target attachment in @ref pMainRenderPass. */
        static constexpr size_t iMainRenderPassColorResolveTargetAttachmentIndex = 2;

        /** Index of @ref pDepthImage in @ref pDepthOnlyRenderPass. */
        static constexpr size_t iDepthOnlyRenderPassDepthImageAttachmentIndex = 0;

        /** Index of @ref pDepthImageNoMultisampling in @ref pDepthOnlyRenderPass. */
        static constexpr size_t iDepthOnlyRenderPassDepthResolveTargetAttachmentIndex = 1;

        /** Format of @ref vSwapChainImages. */
        static constexpr auto swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

        /** Color space of @ref vSwapChainImages. */
        static constexpr auto swapChainImageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        /** Format of @ref pDepthImage. */
        static constexpr auto depthImageFormat = VK_FORMAT_D32_SFLOAT;

        /** Format used for shadow maps. */
        static constexpr auto shadowMapFormat = VK_FORMAT_D32_SFLOAT;

        /**
         * Format used for point lights as "color" target (does not actually store color) during shadow pass.
         */
        static constexpr auto shadowMappingPointLightColorTargetFormat = VK_FORMAT_R32_SFLOAT;

        /** Tiling option for @ref pDepthImage. */
        static constexpr auto depthImageTiling = VK_IMAGE_TILING_OPTIMAL;

        /** Format of indices. */
        static constexpr auto indexTypeFormat = VK_INDEX_TYPE_UINT32;

        /** Used mode for resolving multisampled depth image. */
        static constexpr auto depthResolveMode = VK_RESOLVE_MODE_MAX_BIT;

        /** Used mode for resolving multisampled depth image. */
        static constexpr auto stencilResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;

        /** Version of the Vulkan API that the renderer uses. */
        static constexpr uint32_t iUsedVulkanVersion = VK_API_VERSION_1_2;

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
