#include "render/vulkan/VulkanRenderer.h"

// Standard.
#include <format>

// Custom.
#include "render/general/pipeline/PipelineManager.h"
#include "window/GLFW.hpp"
#include "game/GameManager.h"
#include "game/Window.h"
#include "render/vulkan/resource/VulkanResourceManager.h"
#include "render/vulkan/resource/VulkanFrameResource.h"
#include "render/general/resource/frame/FrameResourceManager.h"
#include "render/general/pipeline/PipelineShaderConstantsManager.hpp"
#include "game/camera/CameraManager.h"
#include "game/camera/CameraProperties.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"
#include "game/node/MeshNode.h"
#include "shader/general/resource/binding/cpuwrite/ShaderCpuWriteResourceBinding.h"
#include "shader/glsl/resource/binding/texture/GlslShaderTextureResourceBinding.h"
#include "shader/general/resource/cpuwrite/DynamicCpuWriteShaderResourceArrayManager.h"
#include "render/general/resource/shadow/ShadowMapHandle.h"
#include "shader/glsl/GlslComputeShaderInterface.h"
#include "shader/general/resource/LightingShaderResourceManager.h"
#include "misc/Profiler.hpp"

// External.
#include "vulkan/vk_enum_string_helper.h"

namespace ne {
// Prepare a macro to create a scoped GPU mark for RenderDoc.
#if defined(DEBUG)
    /** Pointer to the extension function. */
    static inline PFN_vkCmdBeginDebugUtilsLabelEXT pVkCmdBeginDebugUtilsLabelEXT = nullptr;

    /** Pointer to the extension function. */
    static inline PFN_vkCmdEndDebugUtilsLabelEXT pVkCmdEndDebugUtilsLabelEXT = nullptr;

    class GpuVulkanDebugMarkScopeGuard {
    public:
        GpuVulkanDebugMarkScopeGuard(VkCommandBuffer pCommandBuffer, const char* pLabelName) {
            this->pCommandBuffer = pCommandBuffer;

            VkDebugUtilsLabelEXT label{};
            label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            label.pLabelName = pLabelName;

            pVkCmdBeginDebugUtilsLabelEXT(pCommandBuffer, &label);
        }
        GpuVulkanDebugMarkScopeGuard(const GpuVulkanDebugMarkScopeGuard&) = delete;
        GpuVulkanDebugMarkScopeGuard& operator=(const GpuVulkanDebugMarkScopeGuard&) = delete;
        ~GpuVulkanDebugMarkScopeGuard() { pVkCmdEndDebugUtilsLabelEXT(pCommandBuffer); }

    private:
        VkCommandBuffer pCommandBuffer = nullptr;
    };

#define GPU_MARK_FUNC(pCommandBuffer)                                                                        \
    GpuVulkanDebugMarkScopeGuard gpuMarkFuncGuard(pCommandBuffer, __FUNCTION__);
#else
#define GPU_MARK_FUNC(pCommandBuffer) // does nothing
#endif

    VulkanRenderer::VulkanRenderer(GameManager* pGameManager) : Renderer(pGameManager) {
        // Make sure we use the same formats as in DirectX.
        static_assert(
            swapChainImageFormat == VK_FORMAT_B8G8R8A8_UNORM,
            "also change format in DirectX renderer for (visual) consistency");
        static_assert(
            swapChainImageColorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            "also change format in DirectX renderer for (visual) consistency");
        static_assert(
            depthImageFormat == VK_FORMAT_D32_SFLOAT,
            "also change format in DirectX renderer for (visual) consistency");

        // Check shadow map format.
        static_assert(
            shadowMapFormat == VK_FORMAT_D32_SFLOAT,
            "also change format in DirectX renderer for (visual) consistency");
        static_assert(
            shadowMappingPointLightColorTargetFormat == VK_FORMAT_R32_SFLOAT,
            "also change format in DirectX renderer for (visual) consistency");

        // Check light culling compute shader:
        static_assert(
            depthImageFormat == VK_FORMAT_D32_SFLOAT,
            "light culling compute shader expects the depth values to be in range [0..1] for atomic "
            "operations, please review the light culling compute shader and make sure atomics will work "
            "correctly");

        // Check depth resolve mode.
        static_assert(
            getMaxDepth() >= 0.99F && depthResolveMode == VK_RESOLVE_MODE_MAX_BIT,
            "if you are implementing a reversed depth buffer you need to change depth resolve mode to min");
    }

    std::variant<std::optional<AntialiasingQuality>, Error>
    VulkanRenderer::getMaxSupportedAntialiasingQuality() const {
        // Make sure physical device is valid.
        if (pPhysicalDevice == nullptr) [[unlikely]] {
            return Error("expected physical device to be valid at this point");
        }

        // Get physical device properties.
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(pPhysicalDevice, &physicalDeviceProperties);

        // Combine supported color and depth buffer sample count.
        VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                                    physicalDeviceProperties.limits.framebufferDepthSampleCounts;
        if ((counts & VK_SAMPLE_COUNT_4_BIT) != 0) {
            return AntialiasingQuality::HIGH;
        }
        if ((counts & VK_SAMPLE_COUNT_2_BIT) != 0) {
            return AntialiasingQuality::MEDIUM;
        }

        return std::optional<AntialiasingQuality>{};
    }

    VulkanRenderer::~VulkanRenderer() {
        bIsBeingDestroyed = true;

        if (pInstance == nullptr) {
            // Nothing to destroy.
            return;
        }

        if (pLogicalDevice != nullptr) {
            // Wait for all GPU operations to be finished.
            const auto result = vkDeviceWaitIdle(pLogicalDevice);
            if (result != VK_SUCCESS) [[unlikely]] {
                Logger::get().error(
                    std::format("failed to wait for device to be idle, error: {}", string_VkResult(result)));
                return;
            }
        }

        // Reset lighting manager before pipeline manager and GPU resource manager
        // because it uses compute shaders which reference compute pipelines.
        resetLightingShaderResourceManager();

        // Destroy pipeline manager and other draw resources.
        destroySwapChainAndDependentResources(true);

        // Destroy texture samplers.
        if (pTextureSamplerPointFiltering != nullptr || pTextureSamplerLinearFiltering != nullptr ||
            pTextureSamplerAnisotropicFiltering != nullptr) {
            // They're all should be valid.
            if (pTextureSamplerPointFiltering == nullptr || pTextureSamplerLinearFiltering == nullptr ||
                pTextureSamplerAnisotropicFiltering == nullptr) [[unlikely]] {
                Logger::get().error(std::format("expected all fixed texture samplers to be valid"));
                return;
            }
            vkDestroySampler(pLogicalDevice, pTextureSamplerPointFiltering, nullptr);
            vkDestroySampler(pLogicalDevice, pTextureSamplerLinearFiltering, nullptr);
            vkDestroySampler(pLogicalDevice, pTextureSamplerAnisotropicFiltering, nullptr);
            pTextureSamplerPointFiltering = nullptr;
            pTextureSamplerLinearFiltering = nullptr;
            pTextureSamplerAnisotropicFiltering = nullptr;
        }
        if (pComputeTextureSampler != nullptr) {
            vkDestroySampler(pLogicalDevice, pComputeTextureSampler, nullptr);
            pComputeTextureSampler = nullptr;
        }
        if (pShadowTextureSampler != nullptr) {
            vkDestroySampler(pLogicalDevice, pShadowTextureSampler, nullptr);
            pShadowTextureSampler = nullptr;
        }

        if (pLogicalDevice != nullptr) {
            // Explicitly delete frame resources manager before command pool because command buffers
            // in frame resources use command pool to be freed.
            // Also delete frame resources before GPU resource manager because they use memory allocator
            // for destruction.
            resetFrameResourceManager();

            // Explicitly delete memory allocator before all essential Vulkan objects.
            resetGpuResourceManager();

            if (pCommandPool != nullptr) {
                // Destroy command pool.
                vkDestroyCommandPool(pLogicalDevice, pCommandPool, nullptr);
                pCommandPool = nullptr;
            }

            // Destroy logical device.
            vkDestroyDevice(pLogicalDevice, nullptr);
            pLogicalDevice = nullptr;
        }

#if defined(DEBUG)
        if (pValidationLayerDebugMessenger != nullptr) {
            // Destroy validation debug messenger.
            destroyDebugUtilsMessengerEXT(pInstance, pValidationLayerDebugMessenger, nullptr);
            pValidationLayerDebugMessenger = nullptr;
        }
#endif

        if (pWindowSurface != nullptr) {
            // Destroy window surface.
            vkDestroySurfaceKHR(pInstance, pWindowSurface, nullptr);
            pWindowSurface = nullptr;
        }

        // Destroy Vulkan instance.
        vkDestroyInstance(pInstance, nullptr);
        pInstance = nullptr;
    }

    uint32_t VulkanRenderer::getUsedVulkanVersion() { return iUsedVulkanVersion; }

    std::optional<Error> VulkanRenderer::initialize(const std::vector<std::string>& vBlacklistedGpuNames) {
        std::scoped_lock frameGuard(*getRenderResourcesMutex());

        // Initialize essential renderer entities (such as RenderSettings).
        auto optionalError = initializeRenderer();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Initialize Vulkan.
        optionalError = initializeVulkan(vBlacklistedGpuNames);
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error>
    VulkanRenderer::initializeVulkan(const std::vector<std::string>& vBlacklistedGpuNames) {
        // Create Vulkan instance.
        auto optionalError = createVulkanInstance();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create window surface.
        optionalError = createWindowSurface();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Pick physical device.
        optionalError = pickPhysicalDevice(vBlacklistedGpuNames);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Update render settings.
        optionalError = clampSettingsToMaxSupported();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Update MSAA sample count using render settings.
        optionalError = updateMsaaSampleCount();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create logical device.
        optionalError = createLogicalDevice();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create swap chain.
        optionalError = createSwapChain();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create render pass.
        optionalError = createRenderPasses(true);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create command pool.
        optionalError = createCommandPool();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Initialize resource managers.
        optionalError = initializeResourceManagers();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        {
            auto mtxAllFrameResource = getFrameResourceManager()->getAllFrameResources();
            std::scoped_lock frameResourceGuard(*mtxAllFrameResource.first);

            for (size_t i = 0; i < mtxAllFrameResource.second.size(); i++) {
                // Self check: make sure allocated frame resource is of expected type
                // so we may just `reinterpret_cast` later because they won't change.
                const auto pVulkanFrameResource =
                    dynamic_cast<VulkanFrameResource*>(mtxAllFrameResource.second[i]);
                if (pVulkanFrameResource == nullptr) [[unlikely]] {
                    return Error("expected a Vulkan frame resource");
                }
            }
        }

        // Now that GPU resource manager is created:
        // Create depth image.
        optionalError = createDepthImage();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create sampler for compute shaders.
        optionalError = createComputeTextureSampler();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create sampler for shadow maps.
        optionalError = createShadowTextureSampler();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create texture samplers.
        optionalError = createTextureSamplers();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create MSAA render image (if MSAA is enabled).
        optionalError = createMsaaImage();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create framebuffers.
        optionalError = createSwapChainFramebuffers();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        bIsVulkanInitialized = true;

        return {};
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL validationLayerMessageCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
        Logger::get().error(std::format("[validation layer] {}", pCallbackData->pMessage));
        return VK_FALSE;
    }

    std::optional<Error> VulkanRenderer::createVulkanInstance() {
        // Check which extensions are available.
        uint32_t iExtensionCount = 0;
        auto result = vkEnumerateInstanceExtensionProperties(nullptr, &iExtensionCount, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to enumerate available Vulkan instance extension count, error: {}",
                string_VkResult(result)));
        }
        std::vector<VkExtensionProperties> vExtensions(iExtensionCount);
        result = vkEnumerateInstanceExtensionProperties(nullptr, &iExtensionCount, vExtensions.data());
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to enumerate available Vulkan instance extensions, error: {}",
                string_VkResult(result)));
        }

        // Fill information about the application.
        const auto sApplicationName = Globals::getApplicationName();
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = sApplicationName.c_str();
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "nameless engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = iUsedVulkanVersion;

        // Fill information for Vulkan instance creation.
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // Get all extension that we will use.
        auto extensionsResult = getRequiredVulkanInstanceExtensions();
        if (std::holds_alternative<Error>(extensionsResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(extensionsResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto vRequiredExtensions = std::get<std::vector<const char*>>(std::move(extensionsResult));

        // Specify Vulkan instance extensions that we will use.
        createInfo.enabledExtensionCount = static_cast<uint32_t>(vRequiredExtensions.size());
        createInfo.ppEnabledExtensionNames = vRequiredExtensions.data();

#if defined(DEBUG)
        // Make sure that used validation layers supported.
        auto optionalError = makeSureUsedValidationLayersSupported();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Set validation layers.
        createInfo.enabledLayerCount = static_cast<uint32_t>(vUsedValidationLayerNames.size());
        createInfo.ppEnabledLayerNames = vUsedValidationLayerNames.data();
        Logger::get().info(std::format("{} validation layer(s) enabled", createInfo.enabledLayerCount));

        // Fill debug messenger creation info (to use our custom callback for validation layer messages).
        VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
        debugMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugMessengerCreateInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugMessengerCreateInfo.pfnUserCallback = validationLayerMessageCallback;
        debugMessengerCreateInfo.pUserData = nullptr;

        // Specify debug messenger creation info to Vulkan instance creation info to debug
        // instance create/destroy functions.
        createInfo.pNext = &debugMessengerCreateInfo;
#endif

        // Create Vulkan instance.
        result = vkCreateInstance(&createInfo, nullptr, &pInstance);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to create Vulkan instance, make sure your GPU drivers are updated, error: {}",
                string_VkResult(result)));
        }

#if defined(DEBUG)
        // Make validation layers use our custom message callback.
        result = createDebugUtilsMessengerEXT(
            pInstance, &debugMessengerCreateInfo, nullptr, &pValidationLayerDebugMessenger);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to create validation layer debug messenger, error: {}", string_VkResult(result)));
        }

        // Save addresses to some helper debug functions.
        pVkCmdBeginDebugUtilsLabelEXT = requestVkCmdBeginDebugUtilsLabelEXT(pInstance);
        pVkCmdEndDebugUtilsLabelEXT = requestVkCmdEndDebugUtilsLabelEXT(pInstance);
#endif

        return {};
    }

    std::optional<Error> VulkanRenderer::createWindowSurface() {
        // Get game manager.
        const auto pGameManager = getGameManager();
        if (pGameManager == nullptr) [[unlikely]] {
            return Error("game manager is nullptr");
        }

        // Get window.
        const auto pWindow = pGameManager->getWindow();
        if (pWindow == nullptr) [[unlikely]] {
            return Error("window is nullptr");
        }

        // Get GLFW window.
        const auto pGlfwWindow = pWindow->getGlfwWindow();
        if (pGlfwWindow == nullptr) [[unlikely]] {
            return Error("GLFW window is nullptr");
        }

        // Create window surface.
        const auto result = glfwCreateWindowSurface(pInstance, pGlfwWindow, nullptr, &pWindowSurface);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create window surface, error: {}", string_VkResult(result)));
        }

        return {};
    }

    size_t VulkanRenderer::rateGpuSuitability(VkPhysicalDevice pGpuDevice) {
        // Get device properties.
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(pGpuDevice, &deviceProperties);

        // Make sure this device is suitable for this renderer.
        auto result = isDeviceSuitable(pGpuDevice);
        if (std::holds_alternative<Error>(result)) {
            const auto error = std::get<Error>(std::move(result));
            Logger::get().info(std::format(
                "failed to test if the GPU \"{}\" is suitable due to the following error: {}",
                deviceProperties.deviceName,
                error.getFullErrorMessage()));
            return 0;
        }
        const auto sMissingSupportMessage = std::get<std::string>(std::move(result));
        if (!sMissingSupportMessage.empty()) {
            Logger::get().info(std::format("{} and thus cannon be used", sMissingSupportMessage));
            return 0;
        }

        // Prepare a variable for the final score.
        size_t iFinalScore = 0;

        // Check if this is a discrete GPU.
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            iFinalScore += 100'000'000; // NOLINT: magic number, discrete GPUs are highly preferred.
        }

        // Add score for max texture dimension.
        iFinalScore += deviceProperties.limits.maxImageDimension2D;

        // Get supported heap types.
        VkPhysicalDeviceMemoryProperties gpuMemoryProperties;
        vkGetPhysicalDeviceMemoryProperties(pGpuDevice, &gpuMemoryProperties);

        // Find a heap with a DEVICE_LOCAL bit.
        for (size_t i = 0; i < gpuMemoryProperties.memoryHeapCount; i++) {
            if ((gpuMemoryProperties.memoryHeaps[i].flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
                // Found device local heap.
                // Add memory to score.
                if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                    // Probably dedicated memory, more important.
                    iFinalScore += gpuMemoryProperties.memoryHeaps[i].size;
                } else {
                    // Probably shared memory, less important.
                    iFinalScore += gpuMemoryProperties.memoryHeaps[i].size / 1024 / 1024; // NOLINT
                }

                break;
            }
        }

        return iFinalScore;
    }

    std::variant<std::string, Error> VulkanRenderer::isDeviceSuitable(VkPhysicalDevice pGpu) {
        // Get device properties.
        VkPhysicalDeviceDepthStencilResolveProperties depthStencilResolve{};
        depthStencilResolve.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;
        depthStencilResolve.pNext = nullptr;

        VkPhysicalDeviceProperties2 deviceProperties;
        deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProperties.pNext = &depthStencilResolve;
        vkGetPhysicalDeviceProperties2(pGpu, &deviceProperties);

        // Make sure this GPU supports used Vulkan version.
        if (deviceProperties.properties.apiVersion < iUsedVulkanVersion) {
            return std::format(
                "GPU \"{}\" does not support used Vulkan version {}",
                deviceProperties.properties.deviceName,
                getUsedApiVersion());
        }

        // Make sure this GPU has all needed queue families.
        auto queueFamiliesResult = queryQueueFamilyIndices(pGpu);
        if (std::holds_alternative<Error>(queueFamiliesResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(queueFamiliesResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto queueFamiliesIndices = std::get<QueueFamilyIndices>(std::move(queueFamiliesResult));
        if (!queueFamiliesIndices.isComplete()) {
            return std::format(
                "GPU \"{}\" does not support all required queue families",
                deviceProperties.properties.deviceName);
        }

        // Make sure this GPU supports all used device extensions.
        auto result = isGpuSupportsUsedDeviceExtensions(pGpu);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto sMissingDeviceExtension = std::get<std::string>(std::move(result));
        if (!sMissingDeviceExtension.empty()) {
            return std::format(
                "GPU \"{}\" does not support required device extension \"{}\"",
                deviceProperties.properties.deviceName,
                sMissingDeviceExtension);
        }

        // Only after checking for device extensions support.
        // Check swap chain support.
        result = isGpuSupportsSwapChain(pGpu);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto sMissingSwapChainDetailDescription = std::get<std::string>(std::move(result));
        if (!sMissingSwapChainDetailDescription.empty()) {
            return sMissingSwapChainDetailDescription;
        }

        // Prepare a linked list of features that will be filled in `vkGetPhysicalDeviceFeatures2` below
        // so that we can check their support.
        VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
        descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        descriptorIndexingFeatures.pNext = nullptr;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &descriptorIndexingFeatures;

        // Get supported features.
        vkGetPhysicalDeviceFeatures2(pGpu, &deviceFeatures2);

        // Make sure anisotropic filtering is supported
        // (because RenderSettings don't check whether it's supported or not when deserialized/changed)
        if (deviceFeatures2.features.samplerAnisotropy == VK_FALSE) {
            return std::format(
                "GPU \"{}\" does not support anisotropic filtering", deviceProperties.properties.deviceName);
        }

        // Make sure that maximum push constants size that we use is supported.
        if (PipelineShaderConstantsManager::getMaxConstantsSizeInBytes() >
            deviceProperties.properties.limits.maxPushConstantsSize) {
            return std::format(
                "GPU \"{}\" max push constants size is only {} while we expect {}",
                deviceProperties.properties.deviceName,
                deviceProperties.properties.limits.maxPushConstantsSize,
                PipelineShaderConstantsManager::getMaxConstantsSizeInBytes());
        }

        // Make sure used indexing features are supported.
        if (descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing == VK_FALSE ||
            descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind == VK_FALSE ||
            descriptorIndexingFeatures.descriptorBindingPartiallyBound == VK_FALSE ||
            descriptorIndexingFeatures.runtimeDescriptorArray == VK_FALSE) {
            return std::format(
                "GPU \"{}\" does not support used indexing features", deviceProperties.properties.deviceName);
        }

        // Make sure used depth resolve mode is supported.
        if ((depthStencilResolve.supportedDepthResolveModes & depthResolveMode) == 0) {
            return std::format(
                "GPU \"{}\" does not support used depth resolve mode",
                deviceProperties.properties.deviceName);
        }
        if ((depthStencilResolve.supportedStencilResolveModes & stencilResolveMode) == 0) {
            return std::format(
                "GPU \"{}\" does not support used stencil resolve mode",
                deviceProperties.properties.deviceName);
        }

        // Make sure engine shader read/write texture resource formats are supported as storage images.
        std::array<
            ShaderReadWriteTextureResourceFormat,
            static_cast<size_t>(ShaderReadWriteTextureResourceFormat::SIZE)>
            vFormatsToCheck = {ShaderReadWriteTextureResourceFormat::R32G32_UINT};
        static_assert(
            static_cast<size_t>(ShaderReadWriteTextureResourceFormat::SIZE) == 1, "add new formats to check");
        for (const auto& format : vFormatsToCheck) {
            // Get format support details.
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(
                pGpu,
                VulkanResourceManager::convertTextureResourceFormatToVkFormat(format),
                &formatProperties);

            // Make sure this format can be used as a storage image.
            if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0) {
                return std::format(
                    "GPU \"{}\" does not support one of the used texture resource formats to be used as "
                    "a storage image",
                    deviceProperties.properties.deviceName);
            }
        }

        return "";
    }

    std::variant<VulkanRenderer::SwapChainSupportDetails, Error>
    VulkanRenderer::querySwapChainSupportDetails(VkPhysicalDevice pGpu) {
        // Make sure window surface is valid.
        if (pWindowSurface == nullptr) [[unlikely]] {
            return Error("expected window surface to be created at this point");
        }

        SwapChainSupportDetails swapChainSupportDetails{};

        // Query capabilities.
        auto result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            pGpu, pWindowSurface, &swapChainSupportDetails.capabilities);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to query physical device capabilities, error: {}", string_VkResult(result)));
        }

        // Query supported surface format count.
        uint32_t iSupportedFormatCount = 0;
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(pGpu, pWindowSurface, &iSupportedFormatCount, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to query supported physical device surface format count, error: {}",
                string_VkResult(result)));
        }

        if (iSupportedFormatCount != 0) {
            swapChainSupportDetails.vSupportedFormats.resize(iSupportedFormatCount);

            // Query supported surface formats.
            result = vkGetPhysicalDeviceSurfaceFormatsKHR(
                pGpu,
                pWindowSurface,
                &iSupportedFormatCount,
                swapChainSupportDetails.vSupportedFormats.data());
            if (result != VK_SUCCESS) [[unlikely]] {
                return Error(std::format(
                    "failed to query supported physical device surface formats, error: {}",
                    string_VkResult(result)));
            }
        }

        // Query supported presentation mode count.
        uint32_t iSupportedPresentationModeCount = 0;
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(
            pGpu, pWindowSurface, &iSupportedPresentationModeCount, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to query supported physical device present mode count, error: {}",
                string_VkResult(result)));
        }

        if (iSupportedPresentationModeCount != 0) {
            swapChainSupportDetails.vSupportedPresentModes.resize(iSupportedPresentationModeCount);

            // Query supported presentation modes.
            result = vkGetPhysicalDeviceSurfacePresentModesKHR(
                pGpu,
                pWindowSurface,
                &iSupportedPresentationModeCount,
                swapChainSupportDetails.vSupportedPresentModes.data());
            if (result != VK_SUCCESS) [[unlikely]] {
                return Error(std::format(
                    "failed to query supported physical device present modes, error: {}",
                    string_VkResult(result)));
            }
        }

        return swapChainSupportDetails;
    }

    std::variant<VulkanRenderer::QueueFamilyIndices, Error>
    VulkanRenderer::queryQueueFamilyIndices(VkPhysicalDevice pGpu) {
        // Make sure window surface is created.
        if (pWindowSurface == nullptr) [[unlikely]] {
            return Error("expected window surface to be created at this point");
        }

        // Get available queue family count.
        uint32_t iQueueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pGpu, &iQueueFamilyCount, nullptr);

        // Make sure there is at least one queue family available.
        if (iQueueFamilyCount == 0) {
            return Error("failed to query queue families because there are 0 available");
        }

        // Get available queue families.
        std::vector<VkQueueFamilyProperties> vQueueFamilies(iQueueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pGpu, &iQueueFamilyCount, vQueueFamilies.data());

        QueueFamilyIndices queueFamilyIndices;

        // Collect available queue indices.
        uint32_t iCurrentIndex = 0;
        for (const auto& queueFamilyInfo : vQueueFamilies) {
            // See if this is a graphics queue family.
            if ((queueFamilyInfo.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                queueFamilyIndices.iGraphicsFamilyIndex = iCurrentIndex;
            }

            // See if this queue family supports presenting to window surface.
            VkBool32 iHasPresentationSupport = 0;
            const auto result = vkGetPhysicalDeviceSurfaceSupportKHR(
                pGpu, iCurrentIndex, pWindowSurface, &iHasPresentationSupport);
            if (result != VK_SUCCESS) [[unlikely]] {
                return Error(std::format(
                    "failed to get physical device surface support details, error: {}",
                    string_VkResult(result)));
            }
            if (iHasPresentationSupport != 0) {
                queueFamilyIndices.iPresentFamilyIndex = iCurrentIndex;
            }

            // ... new queue family checks go here ...

            if (queueFamilyIndices.isComplete()) {
                // Found all needed queue families.
                break;
            }

            iCurrentIndex += 1;
        }

        return queueFamilyIndices;
    }

    std::variant<std::string, Error> VulkanRenderer::isGpuSupportsSwapChain(VkPhysicalDevice pGpu) {
        // Get device properties.
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(pGpu, &deviceProperties);

        // Get swap chain support details.
        auto swapChainSupportResult = querySwapChainSupportDetails(pGpu);
        if (std::holds_alternative<Error>(swapChainSupportResult)) {
            auto error = std::get<Error>(std::move(swapChainSupportResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto swapChainSupportDetails =
            std::get<SwapChainSupportDetails>(std::move(swapChainSupportResult));

        // Make sure there is at least one supported swap chain image format and a presentation mode.
        if (swapChainSupportDetails.vSupportedFormats.empty() ||
            swapChainSupportDetails.vSupportedPresentModes.empty()) {
            return std::format(
                "GPU \"{}\" swap chain support lacks formats/present modes", deviceProperties.deviceName);
        }

        // Make sure swap chain supports used back buffer format.
        bool bFoundBackBufferFormat = false;
        for (const auto& formatInfo : swapChainSupportDetails.vSupportedFormats) {
            if (formatInfo.format == swapChainImageFormat &&
                formatInfo.colorSpace == swapChainImageColorSpace) {
                bFoundBackBufferFormat = true;
                break;
            }
        }
        if (!bFoundBackBufferFormat) {
            return std::format(
                "GPU \"{}\" swap chain does not support used back buffer format",
                deviceProperties.deviceName);
        }

        // Make sure swap chain supports used presentation modes.
        bool bFoundImmediatePresentMode = false;
        bool bFoundDefaultFifoPresentMode = false;
        for (const auto& presentMode : swapChainSupportDetails.vSupportedPresentModes) {
            if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                bFoundImmediatePresentMode = true;
            } else if (presentMode == VK_PRESENT_MODE_FIFO_KHR) {
                bFoundDefaultFifoPresentMode = true;
            }
        }
        if (!bFoundImmediatePresentMode) {
            return std::format(
                "GPU \"{}\" swap chain does not support immediate present mode", deviceProperties.deviceName);
        }
        if (!bFoundDefaultFifoPresentMode) {
            return std::format(
                "GPU \"{}\" swap chain does not support default FIFO present mode",
                deviceProperties.deviceName);
        }

        const auto iRecommendedSwapChainImageCount = getRecommendedSwapChainBufferCount();

        // 0 max image count means "no limit" so we only check if it's not 0
        if (swapChainSupportDetails.capabilities.maxImageCount > 0) {
            // Make sure we can have at least recommended number of swap chain images.
            if (iRecommendedSwapChainImageCount > swapChainSupportDetails.capabilities.maxImageCount) {
                return std::format(
                    "GPU \"{}\" swap chain can't have {} swap chain images, instead this GPU can only "
                    "have {} swap chain image(s) at max",
                    deviceProperties.deviceName,
                    iRecommendedSwapChainImageCount,
                    swapChainSupportDetails.capabilities.minImageCount);
            }
        }

        return "";
    }

    std::optional<Error>
    VulkanRenderer::pickPhysicalDevice(const std::vector<std::string>& vBlacklistedGpuNames) {
        // Get total GPU count.
        uint32_t iSupportedGpuCount = 0;
        auto result = vkEnumeratePhysicalDevices(pInstance, &iSupportedGpuCount, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(
                std::format("failed to enumerate physical device count, error: {}", string_VkResult(result)));
        }

        // Make sure there is at least one GPU.
        if (iSupportedGpuCount == 0) {
            // Note that some vendor specific validation layers can filter the list of available GPUs
            // and combinations of some layers (like AMD + NV) might filter out all GPUs in some
            // system setups (like AMD iGPU + NVIDIA dGPU).
            return Error("failed to pick a GPU for the renderer because there is no GPU that "
                         "supports used Vulkan instance/extensions/layers (this does not always "
                         "mean that your GPU(s) don't fit engine requirements, in some cases "
                         "this might mean that there's a bug in the engine that causes this "
                         "so please let the developers know about this issue and tell them about your "
                         "CPU and GPU(s) model)");
        }

        // Get information about the GPUs.
        std::vector<VkPhysicalDevice> vGpus(iSupportedGpuCount);
        result = vkEnumeratePhysicalDevices(pInstance, &iSupportedGpuCount, vGpus.data());
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(
                std::format("failed to enumerate physical devices, error: {}", string_VkResult(result)));
        }

        // Pick a GPU with the highest suitability score.
        struct GpuScore {
            size_t iScore = 0;
            VkPhysicalDevice pGpu = nullptr;
            std::string sGpuName;
            bool bIsBlacklisted = false;
        };
        std::vector<GpuScore> vScores;
        vScores.reserve(vGpus.size());

        // Rate all GPUs.
        vSupportedGpuNames.clear();
        for (const auto& pGpu : vGpus) {
            // Rate GPU.
            GpuScore score;
            score.iScore = rateGpuSuitability(pGpu);

            if (score.iScore == 0) {
                // Skip not suitable GPUs.
                continue;
            }

            score.pGpu = pGpu;

            // Save GPU name.
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(pGpu, &deviceProperties);
            score.sGpuName = deviceProperties.deviceName;

            // Add to be considered.
            vScores.push_back(score);

            // Save to the list of supported GPUs.
            vSupportedGpuNames.push_back(score.sGpuName);
        }

        // Make sure there is at least one GPU.
        if (vScores.empty()) {
            return Error("failed to find a suitable GPU");
        }

        // Sort GPUs by score.
        std::sort(vScores.begin(), vScores.end(), [](const GpuScore& scoreA, const GpuScore& scoreB) -> bool {
            return scoreA.iScore > scoreB.iScore;
        });

        // Mark GPUs that are blacklisted and log scores.
        std::string sRating = std::format("found and rated {} suitable GPU(s):", vScores.size());
        for (size_t i = 0; i < vScores.size(); i++) {
            // See if this GPU is blacklisted.
            bool bIsBlacklisted = false;
            for (const auto& sBlacklistedGpuName : vBlacklistedGpuNames) {
                if (vScores[i].sGpuName == sBlacklistedGpuName) {
                    bIsBlacklisted = true;
                    break;
                }
            }

            // Add new entry in rating log.
            sRating += std::format("\n{}. ", i + 1);

            // Process status.
            if (bIsBlacklisted) {
                sRating += "[blacklisted] ";
                vScores[i].bIsBlacklisted = true;
            }

            // Append GPU name and score.
            sRating += std::format("{}, suitability score: {}", vScores[i].sGpuName, vScores[i].iScore);
        }

        // Print scores.
        Logger::get().info(sRating);

        // Get render settings.
        const auto mtxRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(*mtxRenderSettings.first);

        // Check if the GPU to use is set.
        auto sGpuNameToUse = mtxRenderSettings.second->getGpuToUse();
        if (!sGpuNameToUse.empty()) {
            // Find the GPU in the list of available GPUs.
            std::optional<size_t> iFoundIndex{};
            for (size_t i = 0; i < vScores.size(); i++) {
                if (vScores[i].sGpuName == sGpuNameToUse) {
                    iFoundIndex = i;
                    break;
                }
            }
            if (!iFoundIndex.has_value()) {
                // Not found. Log event.
                Logger::get().info(std::format(
                    "unable to find the GPU \"{}\" (that was specified in the renderer's "
                    "config file) in the list of available GPUs for this renderer",
                    sGpuNameToUse));
            } else if (iFoundIndex.value() > 0) {
                // Put found GPU in the first place.
                auto temp = vScores[0];
                vScores[0] = vScores[iFoundIndex.value()];
                vScores[iFoundIndex.value()] = temp;
            }
        }

        // Pick the best suiting GPU.
        for (size_t i = 0; i < vScores.size(); i++) {
            const auto& currentGpuInfo = vScores[i];

            // Skip this GPU is it's blacklisted.
            if (currentGpuInfo.bIsBlacklisted) {
                Logger::get().info(std::format(
                    "ignoring GPU \"{}\" because it's blacklisted (previously the renderer failed to "
                    "initialize using this GPU)",
                    currentGpuInfo.sGpuName));
                continue;
            }

            // Save (cache) queue family indices of this device.
            auto queueFamilyIndicesResult = queryQueueFamilyIndices(currentGpuInfo.pGpu);
            if (std::holds_alternative<Error>(queueFamilyIndicesResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(queueFamilyIndicesResult));
                error.addCurrentLocationToErrorStack();
                Logger::get().error(std::format(
                    "failed to query queue family indices for the rated GPU \"{}\" (error: {})",
                    currentGpuInfo.sGpuName,
                    error.getFullErrorMessage()));
                continue;
            }
            physicalDeviceQueueFamilyIndices =
                std::get<QueueFamilyIndices>(std::move(queueFamilyIndicesResult));

            // Get swap chain capabilities.
            auto swapChainSupportResult = querySwapChainSupportDetails(currentGpuInfo.pGpu);
            if (std::holds_alternative<Error>(swapChainSupportResult)) {
                auto error = std::get<Error>(std::move(swapChainSupportResult));
                error.addCurrentLocationToErrorStack();
                Logger::get().error(std::format(
                    "failed to query swap chain capabilities for the rated GPU \"{}\" (error: {})",
                    currentGpuInfo.sGpuName,
                    error.getFullErrorMessage()));
                continue;
            }
            const auto swapChainSupportDetails =
                std::get<SwapChainSupportDetails>(std::move(swapChainSupportResult));

            // See how much swap chain images we will use.
            const auto iRecommendedSwapChainImageCount = getRecommendedSwapChainBufferCount();
            if (iRecommendedSwapChainImageCount < swapChainSupportDetails.capabilities.minImageCount) {
                Logger::get().info(std::format(
                    "GPU \"{}\" only allows to create {} swap chain images or more "
                    "but we expected to create {} images (this limitation is probably caused by old / low "
                    "performance GPU / drivers), will try to use {} swap chain images but this might "
                    "make the performance just slightly worse",
                    currentGpuInfo.sGpuName,
                    swapChainSupportDetails.capabilities.minImageCount,
                    iRecommendedSwapChainImageCount,
                    swapChainSupportDetails.capabilities.minImageCount));
                iSwapChainImageCount = swapChainSupportDetails.capabilities.minImageCount;
            } else {
                iSwapChainImageCount = iRecommendedSwapChainImageCount;
            }

            // Self check: make sure picked swap chain image count is not bigger than allowed max.
            if (swapChainSupportDetails.capabilities.maxImageCount > 0 &&
                iSwapChainImageCount > swapChainSupportDetails.capabilities.maxImageCount) {
                Logger::get().error(std::format(
                    "picked {} swap chain images for GPU \"{}\" but only {} is allowed (this is a bug, "
                    "report to developers)",
                    iSwapChainImageCount,
                    currentGpuInfo.sGpuName,
                    swapChainSupportDetails.capabilities.maxImageCount));
                continue;
            }

            // Log used GPU.
            if (sGpuNameToUse == currentGpuInfo.sGpuName) {
                Logger::get().info(std::format(
                    "using the following GPU: \"{}\" (was specified as preferred in the renderer's "
                    "config file)",
                    currentGpuInfo.sGpuName));
            } else {
                Logger::get().info(std::format("using the following GPU: \"{}\"", currentGpuInfo.sGpuName));
            }

            // Select physical device.
            pPhysicalDevice = currentGpuInfo.pGpu;

            // Save GPU name in the settings.
            mtxRenderSettings.second->setGpuToUse(currentGpuInfo.sGpuName);

            // Save GPU name.
            sUsedGpuName = currentGpuInfo.sGpuName;

            break;
        }

        // Make sure we picked a GPU.
        if (pPhysicalDevice == nullptr) {
            return Error(std::format(
                "found {} suitable GPU(s) but no GPU was picked (see reason above)", vScores.size()));
        }

        return {};
    }

    std::optional<Error> VulkanRenderer::createLogicalDevice() {
        // Make sure physical device is created.
        if (pPhysicalDevice == nullptr) [[unlikely]] {
            return Error("expected physical device to be created at this point");
        }

        // Prepare information about the queues to create with the logical device.
        std::vector<VkDeviceQueueCreateInfo> vQueueCreateInfo;
        std::set<uint32_t> uniqueQueueFamilyIndices = {
            physicalDeviceQueueFamilyIndices.iGraphicsFamilyIndex.value(),
            physicalDeviceQueueFamilyIndices.iPresentFamilyIndex.value()};

        // Fill queue creation info.
        const auto queuePriority = 1.0F;
        vQueueCreateInfo.reserve(uniqueQueueFamilyIndices.size());
        for (const auto& iQueueIndex : uniqueQueueFamilyIndices) {
            // Fill info to create a queue.
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = iQueueIndex;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            // Add to array of queues to be created.
            vQueueCreateInfo.push_back(queueCreateInfo);
        }

        // Fill info to create a logical device.
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(vQueueCreateInfo.size());
        createInfo.pQueueCreateInfos = vQueueCreateInfo.data();
        createInfo.pEnabledFeatures = nullptr; // NULL because we specify pNext below

        // Specify needed device extensions.
        createInfo.enabledExtensionCount = static_cast<uint32_t>(vUsedDeviceExtensionNames.size());
        createInfo.ppEnabledExtensionNames = vUsedDeviceExtensionNames.data();

#if defined(DEBUG)
        // Setup validation layers (for compatibility with older implementations).
        createInfo.enabledLayerCount = static_cast<uint32_t>(vUsedValidationLayerNames.size());
        createInfo.ppEnabledLayerNames = vUsedValidationLayerNames.data();
#endif

        // Describe features that we will use.
        VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
        descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        descriptorIndexingFeatures.pNext = nullptr;
        descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
        descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
        // ... if adding new features add them to `isDeviceSuitable` function ...

        // Describe device features that we will use.
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        // ... if adding new features add them to `isDeviceSuitable` function ...

        // Prepare device features to enable
        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &descriptorIndexingFeatures;
        deviceFeatures2.features = deviceFeatures;

        // Specify device features to enable.
        createInfo.pNext = &deviceFeatures2;

        // Create device.
        const auto result = vkCreateDevice(pPhysicalDevice, &createInfo, nullptr, &pLogicalDevice);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create logical device, error: {}", string_VkResult(result)));
        }

        // Save reference to created graphics queue.
        vkGetDeviceQueue(
            pLogicalDevice,
            physicalDeviceQueueFamilyIndices.iGraphicsFamilyIndex.value(),
            0,
            &pGraphicsQueue);

        // Save reference to created presentation queue.
        vkGetDeviceQueue(
            pLogicalDevice, physicalDeviceQueueFamilyIndices.iPresentFamilyIndex.value(), 0, &pPresentQueue);

        return {};
    }

    std::optional<Error> VulkanRenderer::createSwapChain() {
        // Make sure physical device is valid.
        if (pPhysicalDevice == nullptr) [[unlikely]] {
            return Error("expected physical device to be initialized at this point");
        }

        // Make sure the number of swap chain images to create is initialized.
        if (iSwapChainImageCount == 0) [[unlikely]] {
            return Error("the number of swap chain images to create is not initialized (zero)");
        }

        // Prepare swap chain size.
        auto swapChainSupportDetailsResult = querySwapChainSupportDetails(pPhysicalDevice);
        if (std::holds_alternative<Error>(swapChainSupportDetailsResult)) {
            auto error = std::get<Error>(std::move(swapChainSupportDetailsResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto swapChainSupportDetails =
            std::get<SwapChainSupportDetails>(std::move(swapChainSupportDetailsResult));

        // Pick swap chain image size.
        auto swapChainExtentResult = pickSwapChainExtent(swapChainSupportDetails.capabilities);
        if (std::holds_alternative<Error>(swapChainExtentResult)) {
            auto error = std::get<Error>(std::move(swapChainExtentResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        swapChainExtent = std::get<VkExtent2D>(std::move(swapChainExtentResult));

        // Update render resolution in the render settings.
        {
            const auto mtxRenderSettings = getRenderSettings();
            std::scoped_lock guard(*mtxRenderSettings.first);
            mtxRenderSettings.second->setRenderResolution({swapChainExtent->width, swapChainExtent->height});
        }

        // Prepare swap chain creation info.
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = pWindowSurface;

        // Fill swap chain image related info.
        createInfo.minImageCount = iSwapChainImageCount;
        createInfo.imageFormat = swapChainImageFormat;
        createInfo.imageColorSpace = swapChainImageColorSpace;
        createInfo.imageExtent = swapChainExtent.value();
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // Fill info about how images will be shared across queues.
        if (physicalDeviceQueueFamilyIndices.iGraphicsFamilyIndex.value() !=
            physicalDeviceQueueFamilyIndices.iPresentFamilyIndex.value()) {
            std::array<uint32_t, 2> vQueueFamilyIndices = {
                physicalDeviceQueueFamilyIndices.iGraphicsFamilyIndex.value(),
                physicalDeviceQueueFamilyIndices.iPresentFamilyIndex.value()};

            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = static_cast<uint32_t>(vQueueFamilyIndices.size());
            createInfo.pQueueFamilyIndices = vQueueFamilyIndices.data();
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;     // optional, only considered when MODE_CONCURRENT
            createInfo.pQueueFamilyIndices = nullptr; // optional, only considered when MODE_CONCURRENT
        }

        // Determine present mode.
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        {
            const auto mtxRenderSettings = getRenderSettings();
            std::scoped_lock guard(*mtxRenderSettings.first);
            if (mtxRenderSettings.second->isVsyncEnabled()) {
                presentMode = VK_PRESENT_MODE_FIFO_KHR;
            }
        }

        // Specify other parameters.
        createInfo.preTransform = swapChainSupportDetails.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        // Create swap chain.
        auto result = vkCreateSwapchainKHR(pLogicalDevice, &createInfo, nullptr, &pSwapChain);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create swap chain, error: {}", string_VkResult(result)));
        }

        // Query image count in the created swap chain.
        uint32_t iActualSwapChainImageCount = 0;
        result = vkGetSwapchainImagesKHR(pLogicalDevice, pSwapChain, &iActualSwapChainImageCount, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to save references to swap chain image count, error: {}", string_VkResult(result)));
        }

        // Check how much images was created.
        if (iActualSwapChainImageCount > iSwapChainImageCount) {
            // Log this event.
            Logger::get().info(std::format(
                "{} swap chain images were created although we requested only {}, "
                "will use all {} created images",
                iActualSwapChainImageCount,
                iSwapChainImageCount,
                iActualSwapChainImageCount));

            // Save new image count.
            iSwapChainImageCount = iActualSwapChainImageCount;
        } else if (iActualSwapChainImageCount < iSwapChainImageCount) [[unlikely]] {
            return Error(std::format(
                "failed to create swap chain images, requested: {}, created: {}",
                iSwapChainImageCount,
                iActualSwapChainImageCount));
        }

        // Allocate swap chain image data.
        vSwapChainImages.resize(iSwapChainImageCount);

        // Save references to swap chain images.
        result = vkGetSwapchainImagesKHR(
            pLogicalDevice, pSwapChain, &iSwapChainImageCount, vSwapChainImages.data());
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to save references to swap chain images, error: {}", string_VkResult(result)));
        }

        // Allocate swap chain image views data.
        vSwapChainImageViews.resize(iSwapChainImageCount);

        // Create image views to swap chain images.
        for (size_t i = 0; i < vSwapChainImages.size(); i++) {
            // Describe image view.
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = vSwapChainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = swapChainImageFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            // Create image view.
            const auto result =
                vkCreateImageView(pLogicalDevice, &viewInfo, nullptr, &vSwapChainImageViews[i]);
            if (result != VK_SUCCESS) [[unlikely]] {
                return Error(std::format("failed to create image view, error: {}", string_VkResult(result)));
            }
        }

        // Prepare to create semaphores for swap chain images.
        vImageSemaphores.resize(iSwapChainImageCount);
        iCurrentImageSemaphore = 0;

        // Describe semaphore.
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        size_t iFrameResourceIndex = 0;
        for (size_t i = 0; i < iSwapChainImageCount; i++) {
            // Create semaphore for `vkAcquireNextImage`.
            result = vkCreateSemaphore(
                pLogicalDevice, &semaphoreInfo, nullptr, &vImageSemaphores[i].pAcquireImageSemaphore);
            if (result != VK_SUCCESS) [[unlikely]] {
                return Error(std::format("failed to create a semaphore, error: {}", string_VkResult(result)));
            }

            // Create semaphore for `vkQueueSubmit`.
            result = vkCreateSemaphore(
                pLogicalDevice, &semaphoreInfo, nullptr, &vImageSemaphores[i].pQueueSubmitSemaphore);
            if (result != VK_SUCCESS) [[unlikely]] {
                return Error(std::format("failed to create a semaphore, error: {}", string_VkResult(result)));
            }

            // Save "fence to use" index.
            vImageSemaphores[i].iUsedFrameResourceIndex = iFrameResourceIndex;

            // Switch to the next fence index.
            iFrameResourceIndex = (iFrameResourceIndex + 1) % FrameResourceManager::getFrameResourceCount();
        }

        // Update light culling resources.
        auto optionalError = recalculateLightTileFrustums();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return {};
    }

    std::optional<Error> VulkanRenderer::createCommandPool() {
        // Describe command pool.
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = physicalDeviceQueueFamilyIndices.iGraphicsFamilyIndex.value();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        // Create command pool.
        const auto result = vkCreateCommandPool(pLogicalDevice, &poolInfo, nullptr, &pCommandPool);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create command pool, error: {}", string_VkResult(result)));
        }

        return {};
    }

    std::variant<VkExtent2D, Error>
    VulkanRenderer::pickSwapChainExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) {
        if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            // `currentExtent` stores the current width and height of the surface.
            return surfaceCapabilities.currentExtent;
        } // else: window size will be determined by the extent of a swapchain

        // Get game manager.
        const auto pGameManager = getGameManager();
        if (pGameManager == nullptr) [[unlikely]] {
            return Error("game manager is nullptr");
        }

        // Get window.
        const auto pWindow = pGameManager->getWindow();
        if (pWindow == nullptr) [[unlikely]] {
            return Error("window is nullptr");
        }

        // Get GLFW window.
        const auto pGlfwWindow = pWindow->getGlfwWindow();
        if (pGlfwWindow == nullptr) [[unlikely]] {
            return Error("GLFW window is nullptr");
        }

        // Get window size and use it as extent.
        int iWidth = -1;
        int iHeight = -1;
        glfwGetFramebufferSize(pGlfwWindow, &iWidth, &iHeight);

        // Make extent.
        VkExtent2D extent = {static_cast<uint32_t>(iWidth), static_cast<uint32_t>(iHeight)};

        return extent;
    }

    std::optional<Error> VulkanRenderer::createRenderPasses(bool bIsRendererInitialization) {
        if (bIsRendererInitialization) {
            // Create shadow mapping render pass
            // (there's no need to re-create it when render settings change
            // moreover framebuffers in shadow maps will reference this render pass
            // and expect that it will not be re-created).
            auto optionalError = createShadowMappingRenderPasses();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        // Create depth only render pass.
        auto optionalError = createDepthOnlyRenderPass();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create main render pass.
        optionalError = createMainRenderPass();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> VulkanRenderer::createMainRenderPass() {
        std::vector<VkAttachmentDescription> vAttachments{};

        static_assert(
            iMainRenderPassColorAttachmentIndex != iMainRenderPassDepthAttachmentIndex &&
                iMainRenderPassColorAttachmentIndex != iMainRenderPassColorResolveTargetAttachmentIndex,
            "attachment indices should be unique");

        const auto bEnableMsaa = msaaSampleCount != VK_SAMPLE_COUNT_1_BIT;

        // Describe color buffer.
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = msaaSampleCount;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = bEnableMsaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                  : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // at end of the render
        vAttachments.push_back(colorAttachment);
        static_assert(iMainRenderPassColorAttachmentIndex == 0);
        if (vAttachments.size() != iMainRenderPassColorAttachmentIndex + 1) [[unlikely]] {
            return Error("unexpected attachment index");
        }

        // Describe depth buffer.
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = depthImageFormat;
        depthAttachment.samples = msaaSampleCount;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // loading depth from depth render pass
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = bEnableMsaa
                                            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // was resolved
                                            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // was used in shaders
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        vAttachments.push_back(depthAttachment);
        static_assert(iMainRenderPassDepthAttachmentIndex == 1);
        if (vAttachments.size() != iMainRenderPassDepthAttachmentIndex + 1) [[unlikely]] {
            return Error("unexpected attachment index");
        }

        if (bEnableMsaa) {
            // Describe color resolve target attachment to resolve color buffer (see above)
            // which uses MSAA to a regular image for presenting.
            VkAttachmentDescription colorResolveTargetAttachment{};
            colorResolveTargetAttachment.format = swapChainImageFormat;
            colorResolveTargetAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorResolveTargetAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorResolveTargetAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorResolveTargetAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorResolveTargetAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorResolveTargetAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorResolveTargetAttachment.finalLayout =
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // at end of the render
            vAttachments.push_back(colorResolveTargetAttachment);
            static_assert(iMainRenderPassColorResolveTargetAttachmentIndex == 2);
            if (vAttachments.size() != iMainRenderPassColorResolveTargetAttachmentIndex + 1) [[unlikely]] {
                return Error("unexpected attachment index");
            }
        }

        // Create color buffer reference for subpasses.
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = iMainRenderPassColorAttachmentIndex;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // layout during subpass

        // Create depth buffer reference in read layout for main subpass.
        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = iMainRenderPassDepthAttachmentIndex;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; // layout during subpass

        VkAttachmentReference colorAttachmentResolveRef{};
        if (bEnableMsaa) {
            // Create reference to resolve target for subpasses.
            colorAttachmentResolveRef.attachment = iMainRenderPassColorResolveTargetAttachmentIndex;
            colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        // Describe main subpass.
        VkSubpassDescription mainSubpass = {};
        mainSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        mainSubpass.colorAttachmentCount = 1;
        mainSubpass.pColorAttachments = &colorAttachmentRef;
        mainSubpass.pDepthStencilAttachment = &depthAttachmentRef;
        if (bEnableMsaa) {
            mainSubpass.pResolveAttachments = &colorAttachmentResolveRef;
        }

        // Describe main subpass dependency.
        VkSubpassDependency mainSubpassDependency = {};
        mainSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        mainSubpassDependency.dstSubpass = 0;
        mainSubpassDependency.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // make sure swap chain finished reading the image
        mainSubpassDependency.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // before commands in our subpass reach color
                                                           // attachment stage
        mainSubpassDependency.srcAccessMask = 0;           // 0 (no need) because we have semaphore
        mainSubpassDependency.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // because we will write to swap chain image

        // Describe main render pass.
        VkRenderPassCreateInfo mainRenderPassInfo{};
        mainRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        mainRenderPassInfo.attachmentCount = static_cast<uint32_t>(vAttachments.size());
        mainRenderPassInfo.pAttachments = vAttachments.data();
        mainRenderPassInfo.subpassCount = 1;
        mainRenderPassInfo.pSubpasses = &mainSubpass;
        mainRenderPassInfo.dependencyCount = 1;
        mainRenderPassInfo.pDependencies = &mainSubpassDependency;

        // Create main render pass.
        const auto result =
            vkCreateRenderPass(pLogicalDevice, &mainRenderPassInfo, nullptr, &pMainRenderPass);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create render pass, error: {}", string_VkResult(result)));
        }

        // Name this render pass.
        setObjectDebugOnlyName(this, pMainRenderPass, VK_OBJECT_TYPE_RENDER_PASS, "main render pass");

        return {};
    }

    std::optional<Error> VulkanRenderer::createDepthOnlyRenderPass() {
        const auto bEnableMsaa = msaaSampleCount != VK_SAMPLE_COUNT_1_BIT;

        // Describe depth image attachment.
        std::vector<VkAttachmentDescription2> vAttachments;
        VkAttachmentDescription2 depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
        depthAttachment.format = depthImageFormat;
        depthAttachment.samples = msaaSampleCount;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // save depth for main pass
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout =
            bEnableMsaa ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // will be resolved
                        : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;        // will be used by shaders
        vAttachments.push_back(depthAttachment);
        if (vAttachments.size() != iDepthOnlyRenderPassDepthImageAttachmentIndex + 1) [[unlikely]] {
            return Error("unexpected attachment index");
        }

        if (bEnableMsaa) {
            // Describe depth image attachment without multisampling.
            VkAttachmentDescription2 depthResolveAttachment{};
            depthResolveAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
            depthResolveAttachment.format = depthImageFormat;
            depthResolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthResolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthResolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // save depth for main pass
            depthResolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthResolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthResolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthResolveAttachment.finalLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // it will be only used by shaders
            vAttachments.push_back(depthResolveAttachment);
            if (vAttachments.size() != iDepthOnlyRenderPassDepthResolveTargetAttachmentIndex + 1)
                [[unlikely]] {
                return Error("unexpected attachment index");
            }
        }

        // Create depth image reference in write layout for depth subpass.
        VkAttachmentReference2 depthAttachmentRef{};
        depthAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
        depthAttachmentRef.attachment = iDepthOnlyRenderPassDepthImageAttachmentIndex;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // layout during subpass

        // Create non-multisampled depth image reference.
        VkAttachmentReference2 depthResolveAttachmentRef{};
        depthResolveAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
        depthResolveAttachmentRef.attachment = iDepthOnlyRenderPassDepthResolveTargetAttachmentIndex;
        depthResolveAttachmentRef.layout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // layout during subpass

        // Create a depth resolve description because we want to resolve multisampled depth image
        // to a non-multisampled depth image to be used in (compute) shaders.
        VkSubpassDescriptionDepthStencilResolve depthResolveDescription{};
        if (bEnableMsaa) {
            depthResolveDescription.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
            depthResolveDescription.depthResolveMode = depthResolveMode;
            depthResolveDescription.stencilResolveMode = stencilResolveMode;
            depthResolveDescription.pDepthStencilResolveAttachment = &depthResolveAttachmentRef;
        }

        // Describe depth only subpass.
        VkSubpassDescription2 depthSubpass{};
        depthSubpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
        depthSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        depthSubpass.colorAttachmentCount = 0;
        depthSubpass.pDepthStencilAttachment = &depthAttachmentRef;
        if (bEnableMsaa) {
            depthSubpass.pNext = &depthResolveDescription;
        }

        // Describe main subpass dependency.
        std::array<VkSubpassDependency2, 2> vSubpassDependencies{};

        // Specify dependency before subpass.
        auto& beforeSubpassDependency = vSubpassDependencies[0];
        beforeSubpassDependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        beforeSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        beforeSubpassDependency.dstSubpass = 0;
        beforeSubpassDependency.srcStageMask =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // make commands before our subpass reach fragment shader
                                                   // stage
        beforeSubpassDependency.dstStageMask =
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // before commands in our subpass reach depth load
                                                        // stage
        beforeSubpassDependency.srcAccessMask =
            VK_ACCESS_SHADER_READ_BIT; // because we want depth read commands to finish
        beforeSubpassDependency.dstAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // before we write to depth

        // Specify dependency after subpass.
        auto& afterSubpassDependency = vSubpassDependencies[1];
        afterSubpassDependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        afterSubpassDependency.srcSubpass = 0;
        afterSubpassDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        afterSubpassDependency.srcStageMask =
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; // make commands in our subpass to reach depth store
                                                       // stage
        afterSubpassDependency.dstStageMask =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // before commands after our subpass reach depth load
                                                   // stage (can't use EARLY_FRAGMENT because of dstAccess)
        afterSubpassDependency.srcAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // because we want to first write depth
        afterSubpassDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // and only then read it

        // Describe depth render pass.
        VkRenderPassCreateInfo2 depthRenderPassInfo{};
        depthRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
        depthRenderPassInfo.attachmentCount = static_cast<uint32_t>(vAttachments.size());
        depthRenderPassInfo.pAttachments = vAttachments.data();
        depthRenderPassInfo.subpassCount = 1;
        depthRenderPassInfo.pSubpasses = &depthSubpass;
        depthRenderPassInfo.dependencyCount = static_cast<uint32_t>(vSubpassDependencies.size());
        depthRenderPassInfo.pDependencies = vSubpassDependencies.data();

        // Create depth render pass.
        const auto result =
            vkCreateRenderPass2(pLogicalDevice, &depthRenderPassInfo, nullptr, &pDepthOnlyRenderPass);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create render pass, error: {}", string_VkResult(result)));
        }

        // Name this render pass.
        setObjectDebugOnlyName(
            this, pDepthOnlyRenderPass, VK_OBJECT_TYPE_RENDER_PASS, "depth only render pass");

        return {};
    }

    std::optional<Error> VulkanRenderer::createShadowMappingRenderPasses() {
        std::vector<VkAttachmentDescription> vAttachments;

        // Describe depth image attachment.
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = shadowMapFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // save depth for main pass
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vAttachments.push_back(depthAttachment);

        // Create depth image reference in write layout for depth subpass.
        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 0;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // layout during subpass

        // Describe subpass.
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 0;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        // Use subpass dependencies for layout transitions
        std::array<VkSubpassDependency, 2> vDependencies{};

        // Specify dependency before subpass.
        auto& beforeSubpassDependency = vDependencies[0];
        beforeSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        beforeSubpassDependency.dstSubpass = 0;
        beforeSubpassDependency.srcStageMask =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // make commands before our subpass reach fragment shader
                                                   // stage
        beforeSubpassDependency.dstStageMask =
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // before commands in our subpass reach depth
                                                        // load stage
        beforeSubpassDependency.srcAccessMask =
            VK_ACCESS_SHADER_READ_BIT; // because we want depth read commands to finish
        beforeSubpassDependency.dstAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // before we write to depth
        beforeSubpassDependency.dependencyFlags =
            VK_DEPENDENCY_BY_REGION_BIT; // we don't need random access to other fragments

        // Specify dependency after subpass.
        auto& afterSubpassDependency = vDependencies[1];
        afterSubpassDependency.srcSubpass = 0;
        afterSubpassDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        afterSubpassDependency.srcStageMask =
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; // make commands in our subpass to reach depth store
                                                       // stage
        afterSubpassDependency.dstStageMask =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // before commands after our subpass reach depth load stage
                                                   // (can't use EARLY_FRAGMENT because of dstAccess)
        afterSubpassDependency.srcAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // because we want to first write depth
        afterSubpassDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // and only then read
        afterSubpassDependency.dependencyFlags =
            VK_DEPENDENCY_BY_REGION_BIT; // we don't need random access to other fragments

        // Describe depth render pass.
        VkRenderPassCreateInfo shadowMappingRenderPassInfo{};
        shadowMappingRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        shadowMappingRenderPassInfo.attachmentCount = static_cast<uint32_t>(vAttachments.size());
        shadowMappingRenderPassInfo.pAttachments = vAttachments.data();
        shadowMappingRenderPassInfo.subpassCount = 1;
        shadowMappingRenderPassInfo.pSubpasses = &subpass;
        shadowMappingRenderPassInfo.dependencyCount = static_cast<uint32_t>(vDependencies.size());
        shadowMappingRenderPassInfo.pDependencies = vDependencies.data();

        // Create render pass.
        auto result = vkCreateRenderPass(
            pLogicalDevice, &shadowMappingRenderPassInfo, nullptr, &pShadowMappingDirectionalSpotRenderPass);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create render pass, error: {}", string_VkResult(result)));
        }

        // Name this render pass.
        setObjectDebugOnlyName(
            this,
            pShadowMappingDirectionalSpotRenderPass,
            VK_OBJECT_TYPE_RENDER_PASS,
            "shadow mapping (directional/spot) render pass");

        // Describe attachment for point light render pass.
        VkAttachmentDescription pointColorAttachment{};
        pointColorAttachment.format = shadowMappingPointLightColorTargetFormat;
        pointColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        pointColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        pointColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // save for main pass
        pointColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        pointColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        pointColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        pointColorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vAttachments.push_back(pointColorAttachment);

        // Create ref.
        VkAttachmentReference pointColorAttachmentRef{};
        pointColorAttachmentRef.attachment = static_cast<uint32_t>(vAttachments.size() - 1);
        pointColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // layout during subpass

        // Update subpass.
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &pointColorAttachmentRef;

        // Update render pass info.
        shadowMappingRenderPassInfo.attachmentCount = static_cast<uint32_t>(vAttachments.size());
        shadowMappingRenderPassInfo.pAttachments = vAttachments.data();

        // Create render pass.
        result = vkCreateRenderPass(
            pLogicalDevice, &shadowMappingRenderPassInfo, nullptr, &pShadowMappingPointRenderPass);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create render pass, error: {}", string_VkResult(result)));
        }

        // Name this render pass.
        setObjectDebugOnlyName(
            this,
            pShadowMappingPointRenderPass,
            VK_OBJECT_TYPE_RENDER_PASS,
            "shadow mapping (point) render pass");

        return {};
    }

    void VulkanRenderer::destroySwapChainAndDependentResources(bool bDestroyPipelineManager) {
        if (pLogicalDevice == nullptr || pSwapChain == nullptr) {
            return;
        }

        // Explicitly destroy MSAA resource before resource manager is destroyed.
        pMsaaImage = nullptr;

        // Explicitly destroy depth resource before resource manager is destroyed.
        pDepthImage = nullptr;
        pDepthImageNoMultisampling = nullptr;

        // Destroy swap chain framebuffers.
        for (size_t i = 0; i < vSwapChainFramebuffersMainRenderPass.size(); i++) {
            vkDestroyFramebuffer(pLogicalDevice, vSwapChainFramebuffersMainRenderPass[i], nullptr);
        }
        vSwapChainFramebuffersMainRenderPass.clear();
        for (size_t i = 0; i < vSwapChainFramebuffersDepthOnlyRenderPass.size(); i++) {
            vkDestroyFramebuffer(pLogicalDevice, vSwapChainFramebuffersDepthOnlyRenderPass[i], nullptr);
        }
        vSwapChainFramebuffersDepthOnlyRenderPass.clear();

        if (bDestroyPipelineManager) {
            // Make sure all pipelines were destroyed because they reference render pass.
            resetPipelineManager();
        } // else: the caller guarantees that all pipeline resources were released

        // Now when all pipelines were destroyed:
        // Destroy render pass.
        vkDestroyRenderPass(pLogicalDevice, pMainRenderPass, nullptr);
        pMainRenderPass = nullptr;
        vkDestroyRenderPass(pLogicalDevice, pDepthOnlyRenderPass, nullptr);
        pDepthOnlyRenderPass = nullptr;
        if (bDestroyPipelineManager) {
            // Renderer is being destroyed.
            vkDestroyRenderPass(pLogicalDevice, pShadowMappingDirectionalSpotRenderPass, nullptr);
            pShadowMappingDirectionalSpotRenderPass = nullptr;
            vkDestroyRenderPass(pLogicalDevice, pShadowMappingPointRenderPass, nullptr);
            pShadowMappingPointRenderPass = nullptr;
        }

        // Destroy swap chain image views.
        for (size_t i = 0; i < vSwapChainImageViews.size(); i++) {
            vkDestroyImageView(pLogicalDevice, vSwapChainImageViews[i], nullptr);
        }
        vSwapChainImageViews.clear();

        // Destroy swap chain.
        vkDestroySwapchainKHR(pLogicalDevice, pSwapChain, nullptr);
        pSwapChain = nullptr;
        vSwapChainImages.clear();
        vSwapChainImageFenceRefs.clear();

        // Destroy semaphores/fence.
        const auto iSemaphoreCount = vImageSemaphores.size();
        for (size_t i = 0; i < iSemaphoreCount; i++) {
            vkDestroySemaphore(pLogicalDevice, vImageSemaphores[i].pAcquireImageSemaphore, nullptr);
            vkDestroySemaphore(pLogicalDevice, vImageSemaphores[i].pQueueSubmitSemaphore, nullptr);
        }
        vImageSemaphores.clear();
        iCurrentImageSemaphore = 0;
    }

    std::optional<Error> VulkanRenderer::createTextureSamplers() {
        // Make sure logical device is valid.
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("expected logical device to be valid");
        }

        // Make sure samplers are not created yet
        if (pTextureSamplerPointFiltering != nullptr || pTextureSamplerLinearFiltering != nullptr ||
            pTextureSamplerAnisotropicFiltering != nullptr) [[unlikely]] {
            return Error("texture samplers are already created");
        }

        // Setup Vulkan parameters depending on the texture filtering mode.
        struct SamplerCreationInfo {
            VkFilter filter;
            VkSamplerMipmapMode mipmapMode;
            bool enableAnisotropy;
            VkSampler& pSampler;
        };
        std::array<SamplerCreationInfo, 3> vSamplerCreationInfos = {
            SamplerCreationInfo{
                VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, false, pTextureSamplerPointFiltering},
            SamplerCreationInfo{
                VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, false, pTextureSamplerLinearFiltering},
            SamplerCreationInfo{
                VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, true, pTextureSamplerAnisotropicFiltering}};

        // Create samplers.
        for (const auto& creationInfo : vSamplerCreationInfos) {
            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

            // Specify how oversampling and undersampling is handed.
            samplerInfo.magFilter = creationInfo.filter;
            samplerInfo.minFilter = creationInfo.filter;

            // Specify address mode.
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // used when `clamp` address mode

            // Specify whether to use anisotropic filtering or not.
            if (creationInfo.enableAnisotropy) {
                samplerInfo.anisotropyEnable = VK_TRUE;
                samplerInfo.maxAnisotropy = 16.0F; // NOLINT: magic number - max anisotropy
            } else {
                samplerInfo.anisotropyEnable = VK_FALSE;
                samplerInfo.maxAnisotropy = 1.0F;
            }

            // Specify whether to use [0; textureWidth] coordinates or not.
            samplerInfo.unnormalizedCoordinates = VK_FALSE;

            // Specify texel comparison options.
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

            // Specify mipmapping options.
            samplerInfo.mipmapMode = creationInfo.mipmapMode;
            samplerInfo.mipLodBias = 0.0F;
            samplerInfo.minLod = 0.0F;
            samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

            // Create sampler.
            const auto result =
                vkCreateSampler(pLogicalDevice, &samplerInfo, nullptr, &creationInfo.pSampler);
            if (result != VK_SUCCESS) [[unlikely]] {
                return Error(std::format("failed to create sampler, error: {}", string_VkResult(result)));
            }
        }

        return {};
    }

    std::optional<Error> VulkanRenderer::createComputeTextureSampler() {
        // Make sure logical device is valid.
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("expected logical device to be valid");
        }

        // Make sure sampler is not valid.
        if (pComputeTextureSampler != nullptr) [[unlikely]] {
            return Error("unexpected compute texture sampler to be re-created");
        }

        // Describe sampler.
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

        // Specify how oversampling and undersampling is handed.
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;

        // Specify address mode.
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // used when `clamp` address mode

        // Disable anisotropic filtering.
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0F;

        // Specify whether to use [0; textureWidth] coordinates or not.
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        // Specify texel comparison for texture filtering.
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        // Specify mipmapping options.
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.mipLodBias = 0.0F;
        samplerInfo.minLod = 0.0F;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

        // Create sampler.
        const auto result = vkCreateSampler(pLogicalDevice, &samplerInfo, nullptr, &pComputeTextureSampler);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create sampler, error: {}", string_VkResult(result)));
        }

        return {};
    }

    std::optional<Error> VulkanRenderer::createShadowTextureSampler() {
        // Make sure logical device is valid.
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("expected logical device to be valid");
        }

        // Make sure sampler is not valid.
        if (pShadowTextureSampler != nullptr) [[unlikely]] {
            return Error("unexpected shadow texture sampler to be re-created");
        }

        // Describe sampler.
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

        // Specify how oversampling and undersampling is handed.
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        // Specify address mode (clamp to edge because in shadow mapping we can get UVs outside [0; 1] range).
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK; // used when `clamp` address mode
        static_assert(
            shadowMapFormat == VK_FORMAT_D32_SFLOAT, "consider changing border color type (INT/FLOAT)");

        // Disable anisotropic filtering.
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0F;

        // Specify whether to use [0; textureWidth] coordinates or not.
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        // Specify texel comparison for texture filtering.
        samplerInfo.compareEnable = VK_TRUE; // for automatic PCF on shadow samplers
        samplerInfo.compareOp =
            VK_COMPARE_OP_LESS_OR_EQUAL; // same in DirectX, fragment depth <= shadow map value

        // Specify mipmapping options.
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.mipLodBias = 0.0F;
        samplerInfo.minLod = 0.0F;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

        // Create sampler.
        auto result = vkCreateSampler(pLogicalDevice, &samplerInfo, nullptr, &pShadowTextureSampler);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create sampler, error: {}", string_VkResult(result)));
        }

        return {};
    }

    bool VulkanRenderer::isUsedDepthImageFormatSupported() {
        // Get details of support of the depth image format.
        VkFormatProperties physicalDeviceFormatProperties;
        vkGetPhysicalDeviceFormatProperties(
            pPhysicalDevice, depthImageFormat, &physicalDeviceFormatProperties);

        constexpr auto usageBit = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

        if (depthImageTiling == VK_IMAGE_TILING_OPTIMAL) {
            return (physicalDeviceFormatProperties.optimalTilingFeatures & usageBit) != 0;
        }

        return (physicalDeviceFormatProperties.linearTilingFeatures & usageBit) != 0;
    }

    std::optional<Error> VulkanRenderer::createDepthImage() {
        // Make sure swap chain extent is set.
        if (!swapChainExtent.has_value()) {
            return Error("expected swap chain extent to have a value");
        }

        // Get resource manager.
        const auto pResourceManager = getResourceManager();
        if (pResourceManager == nullptr) [[unlikely]] {
            return Error("expected GPU resource manager to be valid");
        }

        // Convert manager.
        const auto pVulkanResourceManager = dynamic_cast<VulkanResourceManager*>(pResourceManager);
        if (pVulkanResourceManager == nullptr) [[unlikely]] {
            return Error("expected a Vulkan resource manager");
        }

        // Create depth image.
        auto result = pVulkanResourceManager->createImage(
            "renderer depth/stencil buffer",
            swapChainExtent->width,
            swapChainExtent->height,
            1,
            msaaSampleCount,
            depthImageFormat,
            depthImageTiling,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        pDepthImage = std::get<std::unique_ptr<VulkanResource>>(std::move(result));

        // Create depth image without multisampling.
        result = pVulkanResourceManager->createImage(
            "non-multisampled depth/stencil buffer",
            swapChainExtent->width,
            swapChainExtent->height,
            1,
            VK_SAMPLE_COUNT_1_BIT, // 1 sample
            depthImageFormat,
            depthImageTiling,
            VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        pDepthImageNoMultisampling = std::get<std::unique_ptr<VulkanResource>>(std::move(result));

        return {};
    }

    std::optional<Error> VulkanRenderer::createMsaaImage() {
        if (msaaSampleCount == VK_SAMPLE_COUNT_1_BIT) {
            // Do nothing.
            return {};
        }

        // Make sure swap chain extent is set.
        if (!swapChainExtent.has_value()) {
            return Error("expected swap chain extent to have a value");
        }

        // Get resource manager.
        const auto pResourceManager = getResourceManager();
        if (pResourceManager == nullptr) [[unlikely]] {
            return Error("expected GPU resource manager to be valid");
        }

        // Convert manager.
        const auto pVulkanResourceManager = dynamic_cast<VulkanResourceManager*>(pResourceManager);
        if (pVulkanResourceManager == nullptr) [[unlikely]] {
            return Error("expected a Vulkan resource manager");
        }

        // Create MSAA image.
        auto result = pVulkanResourceManager->createImage(
            "renderer MSAA render buffer",
            swapChainExtent->width,
            swapChainExtent->height,
            1,
            msaaSampleCount,
            swapChainImageFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        pMsaaImage = std::get<std::unique_ptr<VulkanResource>>(std::move(result));

        return {};
    }

    std::optional<Error> VulkanRenderer::recreateSwapChainAndDependentResources() {
        // Pause the rendering.
        std::scoped_lock guard(*getRenderResourcesMutex());
        waitForGpuToFinishWorkUpToThisPoint();

        // Make sure the swap chain exists.
        if (pSwapChain == nullptr) [[unlikely]] {
            return Error("expected the swap chain to exist in order to recreate it");
        }

        // Make sure the GPU is not using swap chain.
        const auto result = vkDeviceWaitIdle(pLogicalDevice);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(
                std::format("failed to wait for device to be idle, error: {}", string_VkResult(result)));
        }

        // Using a new scope to visually mark where pipelines reference old/deleted resources.
        {
            // Destroy swap chain and dependent resources.
            destroySwapChainAndDependentResources(false);

            // Recreate swap chain and dependent resources.
            auto optionalError = createSwapChain();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }

            // Recreate render passes as they depend on the format of the swap chain images.
            optionalError = createRenderPasses(false);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }

            // Recreate pipeline resources to reference the new one.
            optionalError = getPipelineManager()->recreateGraphicsPipelinesResources();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        // Now all pipeline resources were re-created.
        // Descriptor sets were also re-created and were notified but not everything is re-binded.

        // Create render target for MSAA because it depends on the swap chain image sizes.
        auto optionalError = createMsaaImage();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create depth image because it depends on the swap chain image sizes.
        optionalError = createDepthImage();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create framebuffers because they depend on the swap chain images.
        optionalError = createSwapChainFramebuffers();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Since the next acquired swap chain image index will be 0,
        // make sure the current frame resource index is also 0.
        const auto pMtxCurrentFrameResource = getFrameResourceManager()->getCurrentFrameResource();
        std::scoped_lock guardFrameResources(pMtxCurrentFrameResource->first);
        if (pMtxCurrentFrameResource->second.iIndex != 0) {
            do {
                getFrameResourceManager()->switchToNextFrameResource();
            } while (pMtxCurrentFrameResource->second.iIndex != 0);
        }

        return {};
    }

    std::optional<Error> VulkanRenderer::createSwapChainFramebuffers() {
        // Make sure swap chain extent is set.
        if (!swapChainExtent.has_value()) {
            return Error("expected swap chain extent to have a value");
        }

        // Make sure the swap chain is created.
        if (pSwapChain == nullptr) [[unlikely]] {
            return Error("expected the swap chain to be created at this point");
        }

        // Make sure the render pass is created.
        if (pMainRenderPass == nullptr) [[unlikely]] {
            return Error("expected the render pass to be created at this point");
        }

        // Make sure depth image is created.
        if (pDepthImage == nullptr) [[unlikely]] {
            return Error("expected the depth image to be created at this point");
        }

        const auto bEnableMsaa = msaaSampleCount != VK_SAMPLE_COUNT_1_BIT;

        if (bEnableMsaa) {
            // Make sure MSAA image is created.
            if (pMsaaImage == nullptr) [[unlikely]] {
                return Error("expected the MSAA image to be created at this point");
            }
        }

        // Allocate framebuffers data.
        vSwapChainFramebuffersMainRenderPass.resize(iSwapChainImageCount);
        vSwapChainFramebuffersDepthOnlyRenderPass.resize(iSwapChainImageCount);
        vSwapChainImageFenceRefs.resize(iSwapChainImageCount);

        // Make sure framebuffer array size is equal to image views array size.
        if (vSwapChainFramebuffersMainRenderPass.size() != vSwapChainImageViews.size()) [[unlikely]] {
            return Error(std::format(
                "swapchain framebuffer array size ({}) is not equal to swapchain image view array size ({}), "
                "swapchain framebuffers wrap swapchain images thus framebuffer count "
                "should be equal to swapchain image count",
                vSwapChainFramebuffersMainRenderPass.size(),
                vSwapChainImageViews.size()));
        }

        {
            auto mtxAllFrameResource = getFrameResourceManager()->getAllFrameResources();
            std::scoped_lock frameResourceGuard(*mtxAllFrameResource.first);

            // Make swap chain images reference frame resource fences.
            size_t iFrameResourceIndex = 0;
            for (auto& [pFenceRef, iFrameIndex] : vSwapChainImageFenceRefs) {
                // Convert resource.
                const auto pVulkanFrameResource =
                    reinterpret_cast<VulkanFrameResource*>(mtxAllFrameResource.second[iFrameResourceIndex]);

                // Save fence ref and index.
                pFenceRef = pVulkanFrameResource->pFence;
                iFrameIndex = iFrameResourceIndex;

                // Pick next frame resource.
                iFrameResourceIndex =
                    (iFrameResourceIndex + 1) % FrameResourceManager::getFrameResourceCount();
            }
        }

        for (size_t i = 0; i < vSwapChainImageViews.size(); i++) {
            // Prepare image views to render pass attachments that framebuffer will reference.
            std::vector<VkImageView> vAttachments;

            // Specify color attachment.
            static_assert(iMainRenderPassColorAttachmentIndex == 0);
            if (bEnableMsaa) {
                vAttachments.push_back(pMsaaImage->getInternalImageView());
            } else {
                vAttachments.push_back(vSwapChainImageViews[i]);
            }

            // Specify depth attachment.
            static_assert(iMainRenderPassDepthAttachmentIndex == 1);
            vAttachments.push_back(pDepthImage->getInternalImageView());

            // Specify color resolve target attachment.
            static_assert(iMainRenderPassColorResolveTargetAttachmentIndex == 2);
            if (bEnableMsaa) {
                vAttachments.push_back(vSwapChainImageViews[i]);
            }

            // Describe framebuffer.
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = pMainRenderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(vAttachments.size());
            framebufferInfo.pAttachments = vAttachments.data();
            framebufferInfo.width = swapChainExtent->width;
            framebufferInfo.height = swapChainExtent->height;
            framebufferInfo.layers = 1;

            // Create main render pass framebuffer.
            auto result = vkCreateFramebuffer(
                pLogicalDevice, &framebufferInfo, nullptr, &vSwapChainFramebuffersMainRenderPass[i]);
            if (result != VK_SUCCESS) [[unlikely]] {
                return Error(std::format(
                    "failed to create a framebuffer for a swapchain image view, error: {}",
                    string_VkResult(result)));
            }

            // Name this framebuffer.
            setObjectDebugOnlyName(
                this,
                vSwapChainFramebuffersMainRenderPass[i],
                VK_OBJECT_TYPE_FRAMEBUFFER,
                std::format("main render pass swapchain framebuffer for swapchain image #{}", i));

            // Change render pass and attachments.
            vAttachments.clear();

            static_assert(iDepthOnlyRenderPassDepthImageAttachmentIndex == 0);
            vAttachments.push_back(pDepthImage->getInternalImageView());

            if (bEnableMsaa) {
                static_assert(iDepthOnlyRenderPassDepthResolveTargetAttachmentIndex == 1);
                vAttachments.push_back(pDepthImageNoMultisampling->getInternalImageView());
            }

            framebufferInfo.renderPass = pDepthOnlyRenderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(vAttachments.size());
            framebufferInfo.pAttachments = vAttachments.data();

            // Create depth only render pass framebuffer.
            result = vkCreateFramebuffer(
                pLogicalDevice, &framebufferInfo, nullptr, &vSwapChainFramebuffersDepthOnlyRenderPass[i]);
            if (result != VK_SUCCESS) [[unlikely]] {
                return Error(std::format(
                    "failed to create a framebuffer for a swapchain image view, error: {}",
                    string_VkResult(result)));
            }

            // Name this framebuffer.
            setObjectDebugOnlyName(
                this,
                vSwapChainFramebuffersDepthOnlyRenderPass[i],
                VK_OBJECT_TYPE_FRAMEBUFFER,
                std::format("depth only render pass swapchain framebuffer for swapchain image #{}", i));
        }

        return {};
    }

    void VulkanRenderer::prepareRenderTargetForNextFrame() {
        // See if we received `VK_SUBOPTIMAL_KHR` previously.
        if (bNeedToRecreateSwapchain) {
            // Log this event.
            Logger::get().info("re-creating swapchain because received `VK_SUBOPTIMAL_KHR`");

            // Make sure no resources are in use by the GPU.
            waitForGpuToFinishWorkUpToThisPoint();

            // Recreate swap chain and other window-size dependent resources.
            auto optionalError = recreateSwapChainAndDependentResources();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                optionalError->showError();
                throw std::runtime_error(optionalError->getFullErrorMessage());
            }

            // Mark as re-created.
            bNeedToRecreateSwapchain = false;
        }
    }

    void VulkanRenderer::prepareForDrawingNextFrame(
        CameraProperties* pCameraProperties, FrameResource* pCurrentFrameResource) {
        PROFILE_FUNC;

        // Make sure swap chain extent is set.
        if (!swapChainExtent.has_value()) [[unlikely]] {
            Error error("expected swap chain extent to be set at this point");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        PROFILE_SCOPE_START(WaitForFrameResourceFence);

        // Get all frame resources.
        auto mtxAllFrameResources = getFrameResourceManager()->getAllFrameResources();
        std::scoped_lock allFrameResourcesGuard(*mtxAllFrameResources.first);

        // Get frame resource that was used with current semaphores.
        const auto pFenceFrameResource = reinterpret_cast<VulkanFrameResource*>(
            mtxAllFrameResources.second[vImageSemaphores[iCurrentImageSemaphore].iUsedFrameResourceIndex]);

        // Wait for fence to be signaled.
        auto result = vkWaitForFences(pLogicalDevice, 1, &pFenceFrameResource->pFence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(std::format("failed to wait for a fence, error: {}", string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        PROFILE_SCOPE_END;

        PROFILE_SCOPE_START(AcquireNextImage);

        // Acquire an image from the swapchain.
        result = vkAcquireNextImageKHR(
            pLogicalDevice,
            pSwapChain,
            UINT64_MAX,
            vImageSemaphores[iCurrentImageSemaphore].pAcquireImageSemaphore,
            VK_NULL_HANDLE,
            &iLastAcquiredImageIndex);
        if (result != VK_SUCCESS) {
            if (result != VK_SUBOPTIMAL_KHR) [[unlikely]] {
                Error error(std::format(
                    "failed to acquire next swap chain image, error: {}", string_VkResult(result)));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Mark swapchain as needs to be re-created.
            bNeedToRecreateSwapchain = true;
        }

        PROFILE_SCOPE_END;

        // Convert frame resource.
        const auto pCurrentVulkanFrameResource =
            reinterpret_cast<VulkanFrameResource*>(pCurrentFrameResource);

        PROFILE_SCOPE_START(ResetCommandBuffer);

        // Reset command buffer.
        result = vkResetCommandBuffer(pCurrentVulkanFrameResource->pCommandBuffer, 0);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(std::format("failed to reset command buffer, error: {}", string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        PROFILE_SCOPE_END;

        // Prepare to start recording commands.
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;

        // Mark start of command recording.
        result = vkBeginCommandBuffer(pCurrentVulkanFrameResource->pCommandBuffer, &beginInfo);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(std::format(
                "failed to start recording commands into a command buffer, error: {}",
                string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    void VulkanRenderer::startMainRenderPass(VkCommandBuffer pCommandBuffer, size_t iAcquiredImageIndex) {
        PROFILE_FUNC;

        // Prepare to begin render pass.
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = pMainRenderPass;
        renderPassInfo.framebuffer = vSwapChainFramebuffersMainRenderPass[iAcquiredImageIndex];

        // Specify render area.
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = *swapChainExtent;

        // Specify clear color for attachments.
        std::array<VkClearValue, 1> vClearValues;

        static_assert(iMainRenderPassColorAttachmentIndex == 0);
        vClearValues[iMainRenderPassColorAttachmentIndex].color = {{0.0F, 0.0F, 0.0F, 1.0F}};

        // no clear value for depth attachment because we load depth from depth prepass

        // no clear value for MSAA resolve target because it uses load don't care

        renderPassInfo.clearValueCount = static_cast<uint32_t>(vClearValues.size());
        renderPassInfo.pClearValues = vClearValues.data();

        // Mark render pass start.
        vkCmdBeginRenderPass(pCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void
    VulkanRenderer::startDepthOnlyRenderPass(VkCommandBuffer pCommandBuffer, size_t iAcquiredImageIndex) {
        PROFILE_FUNC;

        // Prepare to begin render pass.
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = pDepthOnlyRenderPass;
        renderPassInfo.framebuffer = vSwapChainFramebuffersDepthOnlyRenderPass[iAcquiredImageIndex];

        // Specify render area.
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = *swapChainExtent;

        // Specify clear color for attachments.
        std::array<VkClearValue, 1> vClearValues{};

        static_assert(iDepthOnlyRenderPassDepthImageAttachmentIndex == 0);
        vClearValues[iDepthOnlyRenderPassDepthImageAttachmentIndex].depthStencil = {getMaxDepth(), 0};

        // no clear value for MSAA depth resolve target because it uses load don't care

        renderPassInfo.clearValueCount = static_cast<uint32_t>(vClearValues.size());
        renderPassInfo.pClearValues = vClearValues.data();

        // Mark render pass start.
        vkCmdBeginRenderPass(pCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VulkanRenderer::startShadowMappingRenderPass(
        VkRenderPass pShadowMappingRenderPass,
        VkCommandBuffer pCommandBuffer,
        VkFramebuffer pFramebufferToUse,
        uint32_t iShadowMapSize) {
        PROFILE_FUNC;

        // Prepare to begin render pass.
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = pShadowMappingRenderPass;
        renderPassInfo.framebuffer = pFramebufferToUse;

        // Specify render area.
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent.width = iShadowMapSize;
        renderPassInfo.renderArea.extent.height = iShadowMapSize;

        // Specify clear color for attachments.
        std::array<VkClearValue, 2> vClearValues{};
        vClearValues[0].depthStencil = {getMaxDepth(), 0};
        vClearValues[1].color = {{0.0F, 0.0F, 0.0F, 0.0F}};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(vClearValues.size());
        renderPassInfo.pClearValues = vClearValues.data();

        // Mark render pass start.
        vkCmdBeginRenderPass(pCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    std::variant<std::unique_ptr<Renderer>, std::pair<Error, std::string>>
    VulkanRenderer::create(GameManager* pGameManager, const std::vector<std::string>& vBlacklistedGpuNames) {
        // Create an empty (uninitialized) Vulkan renderer.
        auto pRenderer = std::unique_ptr<VulkanRenderer>(new VulkanRenderer(pGameManager));

        // Initialize renderer.
        const auto optionalError = pRenderer->initialize(vBlacklistedGpuNames);
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return std::pair<Error, std::string>{error, pRenderer->sUsedGpuName};
        }

        return pRenderer;
    }

    VkSampler VulkanRenderer::getTextureSampler(TextureFilteringQuality filtering) {
        switch (filtering) {
        case (TextureFilteringQuality::LOW): {
            return pTextureSamplerPointFiltering;
            break;
        }
        case (TextureFilteringQuality::MEDIUM): {
            return pTextureSamplerLinearFiltering;
            break;
        }
        case (TextureFilteringQuality::HIGH): {
            return pTextureSamplerAnisotropicFiltering;
            break;
        }
        default:
            Error error("unhandled case");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    std::vector<std::string> VulkanRenderer::getSupportedGpuNames() const { return vSupportedGpuNames; }

    std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
    VulkanRenderer::getSupportedRenderResolutions() const {
        // TODO: implement window size independent resolution
        if (!swapChainExtent.has_value()) {
            return Error("swap chain extent is not set");
        }
        return std::set{
            std::pair<unsigned int, unsigned int>{swapChainExtent->width, swapChainExtent->height}};
    }

    std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
    VulkanRenderer::getSupportedRefreshRates() const {
        // TODO: properly query this info
        return std::set<std::pair<unsigned int, unsigned int>>{{0, 0}};
    }

    RendererType VulkanRenderer::getType() const { return RendererType::VULKAN; }

    std::string VulkanRenderer::getUsedApiVersion() const {
        static_assert(iUsedVulkanVersion == VK_API_VERSION_1_2, "update returned version string");
        return "1.2";
    }

    std::string VulkanRenderer::getCurrentlyUsedGpuName() const { return sUsedGpuName; }

    void VulkanRenderer::waitForGpuToFinishWorkUpToThisPoint() {
        if (bIsBeingDestroyed) {
            // Destructor will wait for the GPU to be idle.
            return;
        }

        // Make sure the logical device is valid.
        if (pLogicalDevice == nullptr) [[unlikely]] {
            Error error("expected logical device to be valid at this point");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get frame resources.
        const auto pFrameResourceManager = getFrameResourceManager();
        if (pFrameResourceManager == nullptr) [[unlikely]] {
            Error error("expected frame resource manager to be valid at this point");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get all frame resources.
        const auto mtxAllFrameResources = pFrameResourceManager->getAllFrameResources();

        // Make sure no new frames are queued (if we are calling this function from a non-main thread)
        // to avoid fences change their state to unsignaled due to a new frame being submitted.
        const auto pRenderResourcesMutex = getRenderResourcesMutex();

        // Lock both rendering and all frame resources.
        std::scoped_lock guard(*pRenderResourcesMutex, *mtxAllFrameResources.first);

        // Collect all fences into one array.
        std::vector<VkFence> vFences(mtxAllFrameResources.second.size());
        for (size_t i = 0; i < mtxAllFrameResources.second.size(); i++) {
            vFences[i] = reinterpret_cast<VulkanFrameResource*>(mtxAllFrameResources.second[i])->pFence;
        }

        // Wait for all fences to be signaled.
        const auto result = vkWaitForFences(
            pLogicalDevice, static_cast<uint32_t>(vFences.size()), vFences.data(), VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(std::format("failed to wait for a fence, error: {}", string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    VkDevice VulkanRenderer::getLogicalDevice() const { return pLogicalDevice; }

    VkPhysicalDevice VulkanRenderer::getPhysicalDevice() const { return pPhysicalDevice; }

    VkInstance VulkanRenderer::getInstance() const { return pInstance; }

    VkRenderPass VulkanRenderer::getMainRenderPass() const { return pMainRenderPass; }

    VkRenderPass VulkanRenderer::getDepthOnlyRenderPass() const { return pDepthOnlyRenderPass; }

    VkRenderPass VulkanRenderer::getShadowMappingRenderPass(bool bIsForPointLights) const {
        if (bIsForPointLights) {
            return pShadowMappingPointRenderPass;
        }
        return pShadowMappingDirectionalSpotRenderPass;
    }

    VkCommandPool VulkanRenderer::getCommandPool() const { return pCommandPool; }

    VkQueue VulkanRenderer::getGraphicsQueue() const { return pGraphicsQueue; }

    VkSampler VulkanRenderer::getComputeTextureSampler() const { return pComputeTextureSampler; }

    VkSampler VulkanRenderer::getShadowTextureSampler() const { return pShadowTextureSampler; }

    std::optional<VkExtent2D> VulkanRenderer::getSwapChainExtent() const { return swapChainExtent; }

    VkSampleCountFlagBits VulkanRenderer::getMsaaSampleCount() const { return msaaSampleCount; }

    GpuResource* VulkanRenderer::getDepthTextureNoMultisampling() {
        std::scoped_lock guard(
            *getRenderResourcesMutex()); // `bIsUsingMsaaRenderTarget` is only changed under this mutex

        if (bIsUsingMsaaRenderTarget) {
            // Depth image uses multisampling so return a pointer to the resolved depth resource.
            return pDepthImageNoMultisampling.get();
        }

        // Depth image does not use multisampling so just return it.
        return pDepthImage.get();
    }

    std::variant<VkCommandBuffer, Error> VulkanRenderer::createOneTimeSubmitCommandBuffer() {
        // Make sure command pool exists.
        if (pCommandPool == nullptr) [[unlikely]] {
            return Error("command pool is not created yet");
        }

        // Describe a one-time submit command buffer.
        VkCommandBufferAllocateInfo allocationInfo{};
        allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocationInfo.commandPool = pCommandPool;
        allocationInfo.commandBufferCount = 1;

        // Create a one-time submit command buffer.
        VkCommandBuffer pOneTimeSubmitCommandBuffer = nullptr;
        auto result = vkAllocateCommandBuffers(pLogicalDevice, &allocationInfo, &pOneTimeSubmitCommandBuffer);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to create a one-time submit command buffer, error: {}", string_VkResult(result)));
        }

        // Prepare to record command to the one-time submit command buffer.
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // we will submit once and destroy

        // Start recording commands.
        result = vkBeginCommandBuffer(pOneTimeSubmitCommandBuffer, &beginInfo);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to start recording commands into a one-time submit command buffer, error: {}",
                string_VkResult(result)));
        }

        return pOneTimeSubmitCommandBuffer;
    }

    std::optional<Error>
    VulkanRenderer::submitWaitDestroyOneTimeSubmitCommandBuffer(VkCommandBuffer pOneTimeSubmitCommandBuffer) {
        // Finish recording commands.
        auto result = vkEndCommandBuffer(pOneTimeSubmitCommandBuffer);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to finish recording commands into a one-time submit command buffer, error: {}",
                string_VkResult(result)));
        }

        // Describe fence.
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        // Create fence to wait for commands to be finished.
        VkFence pTemporaryFence = nullptr;
        result = vkCreateFence(pLogicalDevice, &fenceInfo, nullptr, &pTemporaryFence);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format("failed to create a fence, error: {}", string_VkResult(result)));
        }

        // Prepare to execute the commands.
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &pOneTimeSubmitCommandBuffer;

        // Execute the commands.
        result = vkQueueSubmit(pGraphicsQueue, 1, &submitInfo, pTemporaryFence);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to submit commands of a one-time submit command buffer, error: {}",
                string_VkResult(result)));
        }

        // Wait for the fence to be signaled.
        result = vkWaitForFences(pLogicalDevice, 1, &pTemporaryFence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(
                std::format("failed to wait for a temporary fence, error: {}", string_VkResult(result)));
        }

        // Destroy the fence.
        vkDestroyFence(pLogicalDevice, pTemporaryFence, nullptr);

        // Free temporary command buffer.
        vkFreeCommandBuffers(pLogicalDevice, pCommandPool, 1, &pOneTimeSubmitCommandBuffer);

        return {};
    }

    std::optional<Error> VulkanRenderer::transitionImageLayout(
        VkImage pImage,
        VkFormat imageFormat,
        VkImageAspectFlags aspect,
        uint32_t levelCount,
        uint32_t layerCount,
        VkImageLayout oldLayout,
        VkImageLayout newLayout) {
        // Create one-time submit command buffer.
        auto commandBufferResult = createOneTimeSubmitCommandBuffer();
        if (std::holds_alternative<Error>(commandBufferResult)) {
            auto error = std::get<Error>(std::move(commandBufferResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto pOneTimeSubmitCommandBuffer = std::get<VkCommandBuffer>(commandBufferResult);

        // Describe memory barrier.
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;

        // Specify whether or not we want to transfer queue family that owns this image.
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        // Specify which image and its parts will be affected.
        barrier.image = pImage;
        barrier.subresourceRange.aspectMask = aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = levelCount;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = layerCount;

        VkPipelineStageFlags pipelineStagesBeforeBarrier;
        VkPipelineStageFlags pipelineStagesToWaitOnBarrier;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            // Specify which image operations should occur before the barrier (src) and
            // which need to wait on the barrier (dst).
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            // Specify which pipeline stages should occur before the barrier and which should wait.
            pipelineStagesBeforeBarrier = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            pipelineStagesToWaitOnBarrier = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (
            oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
            newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            // Specify which image operations should occur before the barrier (src) and
            // which need to wait on the barrier (dst).
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            // Specify which pipeline stages should occur before the barrier and which should wait.
            pipelineStagesBeforeBarrier = VK_PIPELINE_STAGE_TRANSFER_BIT;
            pipelineStagesToWaitOnBarrier = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (
            oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            // Specify which image operations should occur before the barrier (src) and
            // which need to wait on the barrier (dst).
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask =
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            // Specify which pipeline stages should occur before the barrier and which should wait.
            pipelineStagesBeforeBarrier = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            pipelineStagesToWaitOnBarrier = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            // Specify which image operations should occur before the barrier (src) and
            // which need to wait on the barrier (dst).
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

            // Specify which pipeline stages should occur before the barrier and which should wait.
            pipelineStagesBeforeBarrier = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            pipelineStagesToWaitOnBarrier =
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } else {
            return Error("unsupported image layout transition");
        }

        // Record pipeline barrier command.
        vkCmdPipelineBarrier(
            pOneTimeSubmitCommandBuffer,
            pipelineStagesBeforeBarrier,
            pipelineStagesToWaitOnBarrier,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

        // Submit and wait of the command buffer.
        auto optionalError = submitWaitDestroyOneTimeSubmitCommandBuffer(pOneTimeSubmitCommandBuffer);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::pair<unsigned int, unsigned int> VulkanRenderer::getRenderTargetSize() const {
        if (!swapChainExtent.has_value()) {
            return {0, 0};
        }
        return {swapChainExtent->width, swapChainExtent->height};
    }

    void VulkanRenderer::drawShadowMappingPass(
        FrameResource* pCurrentFrameResource,
        size_t iCurrentFrameResourceIndex,
        GraphicsPipelineRegistry* pGraphicsPipelines) {
        PROFILE_FUNC;

        // Get command buffer of the current frame resource.
        const auto pCommandBuffer =
            reinterpret_cast<VulkanFrameResource*>(pCurrentFrameResource)->pCommandBuffer;

        GPU_MARK_FUNC(pCommandBuffer);

        // Prepare draw call counter to be used later.
        const auto pDrawCallCounter = getDrawCallCounter();

        // Prepare vertex buffer.
        static constexpr size_t iVertexBufferCount = 1;
        std::array<VkBuffer, iVertexBufferCount> vVertexBuffers{};
        const std::array<VkDeviceSize, iVertexBufferCount> vOffsets = {0};

        // Get pipelines to iterate over.
        const auto& shadowMappingDirectionalSpotPipelines = pGraphicsPipelines->vPipelineTypes.at(
            static_cast<size_t>(GraphicsPipelineType::PT_SHADOW_MAPPING_DIRECTIONAL_SPOT));
        const auto& shadowMappingPointPipelines = pGraphicsPipelines->vPipelineTypes.at(
            static_cast<size_t>(GraphicsPipelineType::PT_SHADOW_MAPPING_POINT));

        // Prepare lambda to set viewport size according to shadow map size.
        const auto setViewportSizeToShadowMap = [&](ShadowMapHandle* pShadowMapHandle) {
            const auto iShadowMapSize = pShadowMapHandle->getShadowMapSize();

            // Describe viewport.
            VkViewport viewport{};
            viewport.x = 0.0F;
            viewport.y = static_cast<float>(iShadowMapSize); // flip view space Y to behave as in DirectX
            viewport.width = static_cast<float>(iShadowMapSize);
            viewport.height =
                -static_cast<float>(iShadowMapSize); // flip view space Y to behave as in DirectX
            viewport.minDepth = Renderer::getMinDepth();
            viewport.maxDepth = Renderer::getMaxDepth();

            // Describe scissor.
            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent.width = static_cast<uint32_t>(iShadowMapSize);
            scissor.extent.height = static_cast<uint32_t>(iShadowMapSize);

            // Set viewport/scissor.
            vkCmdSetViewport(pCommandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(pCommandBuffer, 0, 1, &scissor);
        };

        // Prepare lambda to draw scene to shadow map.
        const auto drawSceneToShadowMap =
            [&](VkRenderPass pRenderPass,
                const std::unordered_map<std::string, ShaderPipelines>& pipelinesToRender,
                ShadowMapHandle* pShadowMapHandle,
                unsigned int iIndexIntoShadowPassInfoArray,
                size_t iCubemapFaceIndex = 0) {
                // Get shadow map texture.
                const auto pMtxShadowResources = pShadowMapHandle->getResources();
                std::scoped_lock shadowMapGuard(pMtxShadowResources->first);

                // Start shadow mapping render pass with light's framebuffer.
                startShadowMappingRenderPass(
                    pRenderPass,
                    pCommandBuffer,
                    pMtxShadowResources->second.vShadowMappingFramebuffers[iCubemapFaceIndex],
                    static_cast<uint32_t>(pShadowMapHandle->getShadowMapSize()));

                // Set viewport size.
                setViewportSizeToShadowMap(pShadowMapHandle);

                // Iterate over all shadow mapping pipelines that have different vertex shaders.
                for (const auto& [sShaderNames, pipelines] : pipelinesToRender) {
                    // Iterate over all shadow mapping pipelines that use the same vertex shader but different
                    // shader macros (if there are different macro variations).
                    for (const auto& [macros, pPipeline] : pipelines.shaderPipelines) {
                        // Convert pipeline type.
                        const auto pVulkanPipeline = reinterpret_cast<VulkanPipeline*>(pPipeline.get());

                        // Get push constants and pipeline's resources.
                        const auto pMtxPushConstants = pVulkanPipeline->getShaderConstants();
                        auto pMtxPipelineResources = pVulkanPipeline->getInternalResources();

                        // Lock both.
                        std::scoped_lock guardPipelineResources(
                            pMtxPipelineResources->first, pMtxPushConstants->first);

                        // Bind pipeline.
                        vkCmdBindPipeline(
                            pCommandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pMtxPipelineResources->second.pPipeline);

                        // Bind descriptor sets.
                        vkCmdBindDescriptorSets(
                            pCommandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pMtxPipelineResources->second.pPipelineLayout,
                            0,
                            1,
                            &pMtxPipelineResources->second.vDescriptorSets[iCurrentFrameResourceIndex],
                            0,
                            nullptr);

#if defined(DEBUG)
                        // Self check: make sure push constants are used.
                        if (!pMtxPushConstants->second.has_value()) [[unlikely]] {
                            Error error(std::format(
                                "expected push constants to be used on the pipeline \"{}\"",
                                pVulkanPipeline->getPipelineIdentifier()));
                            error.showError();
                            throw std::runtime_error(error.getFullErrorMessage());
                        }
#endif

                        // Copy shadow pass info index to constants.
                        pMtxPushConstants->second->findOffsetAndCopySpecialValueToConstant(
                            pPipeline.get(),
                            PipelineShaderConstantsManager::SpecialConstantsNames::pShadowPassLightInfoIndex,
                            iIndexIntoShadowPassInfoArray);

                        // Get push constants manager to be used later.
                        const auto pPushConstantsManager = pMtxPushConstants->second->pConstantsManager.get();

                        // Get materials.
                        const auto pMtxMaterials = pPipeline->getMaterialsThatUseThisPipeline();
                        std::scoped_lock materialsGuard(pMtxMaterials->first);

                        for (const auto& pMaterial : pMtxMaterials->second) {
                            // No need to bind material's shader resources since they are not used in vertex
                            // shader (since we are in shadow mapping pass).

                            // Get meshes.
                            const auto pMtxMeshNodes = pMaterial->getSpawnedMeshNodesThatUseThisMaterial();
                            std::scoped_lock meshNodesGuard(pMtxMeshNodes->first);

                            // Iterate over all visible mesh nodes that use this material.
                            for (const auto& [pMeshNode, vIndexBuffers] :
                                 pMtxMeshNodes->second.visibleMeshNodes) {
                                // Get mesh data.
                                auto pMtxMeshGpuResources = pMeshNode->getMeshGpuResources();

                                // Note: if you will ever need it - don't lock mesh node's spawning/despawning
                                // mutex here as it might cause a deadlock (see MeshNode::setMaterial for
                                // example).
                                std::scoped_lock geometryGuard(pMtxMeshGpuResources->first);

                                // Find and bind mesh data resource since only it is used in vertex shader.
                                const auto& meshDataIt = pMtxMeshGpuResources->second.shaderResources
                                                             .shaderCpuWriteResourceBindings.find(
                                                                 MeshNode::getMeshShaderConstantBufferName());
#if defined(DEBUG)
                                if (meshDataIt == pMtxMeshGpuResources->second.shaderResources
                                                      .shaderCpuWriteResourceBindings.end()) [[unlikely]] {
                                    Error error(std::format(
                                        "expected to find \"{}\" shader resource",
                                        MeshNode::getMeshShaderConstantBufferName()));
                                    error.showError();
                                    throw std::runtime_error(error.getFullErrorMessage());
                                }
#endif
                                meshDataIt->second.getResource()->copyResourceIndexToShaderConstants(
                                    pPushConstantsManager, pVulkanPipeline, iCurrentFrameResourceIndex);

                                // Bind vertex buffer.
                                vVertexBuffers[0] = {
                                    reinterpret_cast<VulkanResource*>(
                                        pMtxMeshGpuResources->second.mesh.pVertexBuffer.get())
                                        ->getInternalBufferResource()};
                                vkCmdBindVertexBuffers(
                                    pCommandBuffer,
                                    0,
                                    static_cast<uint32_t>(vVertexBuffers.size()),
                                    vVertexBuffers.data(),
                                    vOffsets.data());

                                // Set push constants.
                                vkCmdPushConstants(
                                    pCommandBuffer,
                                    pMtxPipelineResources->second.pPipelineLayout,
                                    VK_SHADER_STAGE_ALL_GRAPHICS,
                                    0,
                                    pPushConstantsManager->getTotalSizeInBytes(),
                                    pPushConstantsManager->getData());

                                // Iterate over all index buffers of a specific mesh node that use this
                                // material.
                                for (const auto& indexBufferInfo : vIndexBuffers) {
                                    // Bind index buffer.
                                    static_assert(
                                        sizeof(MeshData::meshindex_t) == sizeof(unsigned int),
                                        "change `indexTypeFormat`");
                                    vkCmdBindIndexBuffer(
                                        pCommandBuffer,
                                        reinterpret_cast<VulkanResource*>(indexBufferInfo.pIndexBuffer)
                                            ->getInternalBufferResource(),
                                        0,
                                        indexTypeFormat);

                                    // Add a draw command.
                                    vkCmdDrawIndexed(pCommandBuffer, indexBufferInfo.iIndexCount, 1, 0, 0, 0);

                                    // Increment draw call counter.
                                    pDrawCallCounter->fetch_add(1);
                                }
                            }
                        }
                    }
                }

                // Finish shadow mapping render pass with light's framebuffer.
                vkCmdEndRenderPass(pCommandBuffer);
            };

        {
            // Get directional lights.
            const auto pMtxDirectionalLights =
                getLightingShaderResourceManager()->getDirectionalLightDataArray()->getInternalResources();
            std::scoped_lock directionalLightsGuard(pMtxDirectionalLights->first);

            // Iterate over all directional lights (no culling).
            for (const auto& pLightNode :
                 pMtxDirectionalLights->second.lightsInFrustum.vShaderLightNodeArray) {
                // Convert node type.
                const auto pDirectionalLightNode = reinterpret_cast<DirectionalLightNode*>(pLightNode);

                // Get light info.
                ShadowMapHandle* pShadowMapHandle = nullptr;
                unsigned int iIndexIntoShadowPassInfoArray = 0;
                Renderer::getDirectionalLightNodeShadowMappingInfo(
                    pDirectionalLightNode, pShadowMapHandle, iIndexIntoShadowPassInfoArray);

                // Draw to shadow map.
                drawSceneToShadowMap(
                    getShadowMappingRenderPass(false),
                    shadowMappingDirectionalSpotPipelines,
                    pShadowMapHandle,
                    iIndexIntoShadowPassInfoArray);
            }
        }

        {
            // Get spotlights.
            const auto pMtxSpotlights =
                getLightingShaderResourceManager()->getSpotlightDataArray()->getInternalResources();
            std::scoped_lock spotlightsGuard(pMtxSpotlights->first);

            // Iterate only over spotlights in camera's frustum.
            for (const auto& iSpotlightIndex :
                 pMtxSpotlights->second.lightsInFrustum.vLightIndicesInFrustum) {
                // Convert node type.
                const auto pSpotlightNode = reinterpret_cast<SpotlightNode*>(
                    pMtxSpotlights->second.lightsInFrustum.vShaderLightNodeArray[iSpotlightIndex]);

                // Get light info.
                ShadowMapHandle* pShadowMapHandle = nullptr;
                unsigned int iIndexIntoShadowPassInfoArray = 0;
                Renderer::getSpotlightNodeShadowMappingInfo(
                    pSpotlightNode, pShadowMapHandle, iIndexIntoShadowPassInfoArray);

                // Draw to shadow map.
                drawSceneToShadowMap(
                    getShadowMappingRenderPass(false),
                    shadowMappingDirectionalSpotPipelines,
                    pShadowMapHandle,
                    iIndexIntoShadowPassInfoArray);
            }
        }

        {
            // Get point lights.
            const auto pMtxPointLights =
                getLightingShaderResourceManager()->getPointLightDataArray()->getInternalResources();
            std::scoped_lock pointLightsGuard(pMtxPointLights->first);

            // Iterate only over point lights in frustum.
            for (const auto& iPointLightIndex :
                 pMtxPointLights->second.lightsInFrustum.vLightIndicesInFrustum) {
                // Convert node type.
                const auto pPointLightNode = reinterpret_cast<PointLightNode*>(
                    pMtxPointLights->second.lightsInFrustum.vShaderLightNodeArray[iPointLightIndex]);

                // Get shadow handle.
                const auto pShadowMapHandle = Renderer::getPointLightNodeShadowMapHandle(pPointLightNode);

                // Draw to each cube shadow map face.
                for (size_t i = 0; i < 6; i++) { // NOLINT: cubemap has 6 faces
                    // Get index into shadow pass info array.
                    const auto iIndexIntoShadowPassInfoArray =
                        Renderer::getPointLightShadowPassLightInfoArrayIndex(pPointLightNode, i);

                    // Draw to cubemap's face.
                    drawSceneToShadowMap(
                        getShadowMappingRenderPass(true),
                        shadowMappingPointPipelines,
                        pShadowMapHandle,
                        iIndexIntoShadowPassInfoArray,
                        i);
                }
            }
        }
    }

    void VulkanRenderer::setObjectDebugOnlyName(
        Renderer* pRenderer, void* pObject, VkObjectType objectType, const std::string& sResourceName) {
#if defined(DEBUG)
        // Convert renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pRenderer);
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            Error error("expected a Vulkan renderer");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get Vulkan instance.
        const auto pInstance = pVulkanRenderer->getInstance();
        if (pInstance == nullptr) [[unlikely]] {
            Error error("expected an instance to be valid");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get Vulkan device.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            Error error("expected a logical device to be valid");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Prepare debug name info.
        VkDebugUtilsObjectNameInfoEXT debugNameInfo{};
        debugNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        debugNameInfo.pNext = nullptr;
        debugNameInfo.objectType = objectType;
        debugNameInfo.objectHandle = reinterpret_cast<uint64_t>(pObject);
        debugNameInfo.pObjectName = sResourceName.c_str();

        // Get pointer to the function.
        const auto pSetDebugUtilsObjectName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetInstanceProcAddr(pInstance, "vkSetDebugUtilsObjectNameEXT"));
        if (pSetDebugUtilsObjectName == nullptr) [[unlikely]] {
            Error error("expected to get extension function pointer");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Set debug name.
        const auto result = pSetDebugUtilsObjectName(pLogicalDevice, &debugNameInfo);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(std::format("failed to set debug object name, error: {}", string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
#endif
    }

    void VulkanRenderer::drawMeshesDepthPrepass(
        FrameResource* pCurrentFrameResource,
        size_t iCurrentFrameResourceIndex,
        const std::vector<Renderer::MeshesInFrustum::PipelineInFrustumInfo>& vOpaquePipelines) {
        PROFILE_FUNC;

        // Get command buffer of the current frame resource.
        const auto pCommandBuffer =
            reinterpret_cast<VulkanFrameResource*>(pCurrentFrameResource)->pCommandBuffer;

        GPU_MARK_FUNC(pCommandBuffer);

        // Start depth only render pass.
        startDepthOnlyRenderPass(pCommandBuffer, iLastAcquiredImageIndex);

        // Prepare draw call counter to be used later.
        const auto pDrawCallCounter = getDrawCallCounter();

        // Prepare vertex buffer.
        static constexpr size_t iVertexBufferCount = 1;
        std::array<VkBuffer, iVertexBufferCount> vVertexBuffers{};
        const std::array<VkDeviceSize, iVertexBufferCount> vOffsets = {0};

        for (const auto& pipelineInfo : vOpaquePipelines) {
            // Get depth only (vertex shader only) pipeline
            // (all materials that use the same opaque pipeline will use the same depth only pipeline).
            const auto pVulkanPipeline = reinterpret_cast<VulkanPipeline*>(
                pipelineInfo.vMaterials[0].pMaterial->getDepthOnlyPipeline());

            // Get push constants and pipeline's resources.
            const auto pMtxPushConstants = pVulkanPipeline->getShaderConstants();
            auto pMtxPipelineResources = pVulkanPipeline->getInternalResources();

            // Lock both.
            std::scoped_lock guardPipelineResources(pMtxPipelineResources->first, pMtxPushConstants->first);

#if defined(DEBUG)
            // Self check: make sure push constants are used.
            if (!pMtxPushConstants->second.has_value()) [[unlikely]] {
                Error error(std::format(
                    "expected push constants to be used on the pipeline \"{}\"",
                    pVulkanPipeline->getPipelineIdentifier()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
#endif

            // Get push constants manager.
            const auto pPushConstantsManager = pMtxPushConstants->second->pConstantsManager.get();

            // Bind pipeline.
            vkCmdBindPipeline(
                pCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pMtxPipelineResources->second.pPipeline);

            // Bind descriptor sets.
            vkCmdBindDescriptorSets(
                pCommandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pMtxPipelineResources->second.pPipelineLayout,
                0,
                1,
                &pMtxPipelineResources->second.vDescriptorSets[iCurrentFrameResourceIndex],
                0,
                nullptr);

            for (const auto& materialInfo : pipelineInfo.vMaterials) {
                // No need to bind material's shader resources since they are not used in vertex shader
                // (since we are in depth prepass).

                for (const auto& meshInfo : materialInfo.vMeshes) {
                    // Get mesh data.
                    auto pMtxMeshGpuResources = meshInfo.pMeshNode->getMeshGpuResources();

                    // Note: if you will ever need it - don't lock mesh node's spawning/despawning mutex here
                    // as it might cause a deadlock (see MeshNode::setMaterial for example).
                    std::scoped_lock geometryGuard(pMtxMeshGpuResources->first);

                    // Find and bind mesh data resource since only it is used in vertex shader.
                    const auto& meshDataIt =
                        pMtxMeshGpuResources->second.shaderResources.shaderCpuWriteResourceBindings.find(
                            MeshNode::getMeshShaderConstantBufferName());
#if defined(DEBUG)
                    if (meshDataIt ==
                        pMtxMeshGpuResources->second.shaderResources.shaderCpuWriteResourceBindings.end())
                        [[unlikely]] {
                        Error error(std::format(
                            "expected to find \"{}\" shader resource",
                            MeshNode::getMeshShaderConstantBufferName()));
                        error.showError();
                        throw std::runtime_error(error.getFullErrorMessage());
                    }
#endif
                    meshDataIt->second.getResource()->copyResourceIndexToShaderConstants(
                        pPushConstantsManager, pVulkanPipeline, iCurrentFrameResourceIndex);

                    // Bind vertex buffer.
                    vVertexBuffers[0] = {reinterpret_cast<VulkanResource*>(
                                             pMtxMeshGpuResources->second.mesh.pVertexBuffer.get())
                                             ->getInternalBufferResource()};
                    vkCmdBindVertexBuffers(
                        pCommandBuffer,
                        0,
                        static_cast<uint32_t>(vVertexBuffers.size()),
                        vVertexBuffers.data(),
                        vOffsets.data());

                    // Set push constants.
                    vkCmdPushConstants(
                        pCommandBuffer,
                        pMtxPipelineResources->second.pPipelineLayout,
                        VK_SHADER_STAGE_ALL_GRAPHICS,
                        0,
                        pPushConstantsManager->getTotalSizeInBytes(),
                        pPushConstantsManager->getData());

                    // Iterate over all index buffers of a specific mesh node that use this material.
                    for (const auto& indexBufferInfo : meshInfo.vIndexBuffers) {
                        // Bind index buffer.
                        static_assert(
                            sizeof(MeshData::meshindex_t) == sizeof(unsigned int),
                            "change `indexTypeFormat`");
                        vkCmdBindIndexBuffer(
                            pCommandBuffer,
                            reinterpret_cast<VulkanResource*>(indexBufferInfo.pIndexBuffer)
                                ->getInternalBufferResource(),
                            0,
                            indexTypeFormat);

                        // Add a draw command.
                        vkCmdDrawIndexed(pCommandBuffer, indexBufferInfo.iIndexCount, 1, 0, 0, 0);

                        // Increment draw call counter.
                        pDrawCallCounter->fetch_add(1);
                    }
                }
            }
        }

        // Finish depth only render pass.
        vkCmdEndRenderPass(pCommandBuffer);
    }

    void VulkanRenderer::executeComputeShadersOnGraphicsQueue(
        FrameResource* pCurrentFrameResource,
        size_t iCurrentFrameResourceIndex,
        ComputeExecutionStage stage) {
        PROFILE_FUNC;

        // Get shader groups.
        auto& computeShaderGroups = getPipelineManager()
                                        ->getComputeShadersForGraphicsQueueExecution()
                                        .second->vGraphicsQueueStagesGroups[static_cast<size_t>(stage)];

        // Check if we have compute shaders to dispatch.
        bool bHaveComputeWorkToDispatch = false;
        for (auto& group : computeShaderGroups) {
            if (group.empty()) {
                continue;
            }

            bHaveComputeWorkToDispatch = true;
            break;
        }

        if (!bHaveComputeWorkToDispatch) {
            // Nothing to do.
            return;
        }

        // Convert frame resource.
        const auto pVulkanFrameResource = reinterpret_cast<VulkanFrameResource*>(pCurrentFrameResource);

        GPU_MARK_FUNC(pVulkanFrameResource->pCommandBuffer);

        // Insert a synchronization barrier.
        vkCmdPipelineBarrier(
            pVulkanFrameResource->pCommandBuffer,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,   // run all graphics commands before this barrier
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // make all next compute commands to wait for this
                                                  // barrier
            0,
            0,
            nullptr,
            0,
            nullptr,
            0,
            nullptr);

        // Dispatch compute shaders.
        for (auto& group : computeShaderGroups) {
            if (dispatchComputeShadersOnGraphicsQueue(
                    pVulkanFrameResource, iCurrentFrameResourceIndex, group)) {
                // Insert a synchronization barrier.
                vkCmdPipelineBarrier(
                    pVulkanFrameResource->pCommandBuffer,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   // run all compute commands before this barrier
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |  // make all next compute/graphics commands to
                        VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, // wait for this barrier
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    0,
                    nullptr);
            }
        }
    }

    void VulkanRenderer::drawMeshesMainPass(
        FrameResource* pCurrentFrameResource,
        size_t iCurrentFrameResourceIndex,
        const std::vector<MeshesInFrustum::PipelineInFrustumInfo>& vOpaquePipelines,
        const std::vector<MeshesInFrustum::PipelineInFrustumInfo>& vTransparentPipelines) {
        // Convert frame resource.
        const auto pVulkanFrameResource = reinterpret_cast<VulkanFrameResource*>(pCurrentFrameResource);

        GPU_MARK_FUNC(pVulkanFrameResource->pCommandBuffer);

        // Start main render pass.
        startMainRenderPass(pVulkanFrameResource->pCommandBuffer, iLastAcquiredImageIndex);

        // Draw opaque meshes.
        drawMeshesMainPassSpecificPipelines(
            vOpaquePipelines, pVulkanFrameResource->pCommandBuffer, iCurrentFrameResourceIndex);

        // Draw transparent meshes.
        drawMeshesMainPassSpecificPipelines(
            vTransparentPipelines, pVulkanFrameResource->pCommandBuffer, iCurrentFrameResourceIndex);

        // Mark render pass end.
        vkCmdEndRenderPass(pVulkanFrameResource->pCommandBuffer);

        // Mark end of command recording.
        auto result = vkEndCommandBuffer(pVulkanFrameResource->pCommandBuffer);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(std::format(
                "failed to finish recording commands into a command buffer, error: {}",
                string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    void VulkanRenderer::present(FrameResource* pCurrentFrameResource, size_t iCurrentFrameResourceIndex) {
        PROFILE_FUNC;

        // Convert current frame resource.
        const auto pVulkanCurrentFrameResource =
            reinterpret_cast<VulkanFrameResource*>(pCurrentFrameResource);

        PROFILE_SCOPE_START(WaitForAcquiredImageToBeUnused);

        // Since the next acquired image might be not in the order we expect (i.e. it may be used
        // by other frame resource - not the one we are using right now) make sure this image is not used
        // by the GPU.
        auto result = vkWaitForFences(
            pLogicalDevice, 1, &vSwapChainImageFenceRefs[iLastAcquiredImageIndex].first, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(
                std::format("failed to wait for acquired image fence, error: {}", string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        PROFILE_SCOPE_END;

        // Mark the image as being used by this frame resource.
        vSwapChainImageFenceRefs[iLastAcquiredImageIndex].first = pVulkanCurrentFrameResource->pFence;
        vSwapChainImageFenceRefs[iLastAcquiredImageIndex].second = iCurrentFrameResourceIndex;

        // Prepare to submit command buffer for execution.
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // Specify semaphores to wait for before starting execution.
        const std::array<VkSemaphore, 1> semaphoresToWaitFor = {
            vImageSemaphores[iCurrentImageSemaphore].pAcquireImageSemaphore};
        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(semaphoresToWaitFor.size());
        submitInfo.pWaitSemaphores = semaphoresToWaitFor.data();

        // Specify which stages will wait.
        const std::array<VkPipelineStageFlags, semaphoresToWaitFor.size()> waitStages = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.pWaitDstStageMask = waitStages.data();

        // Specify which command buffers to execute.
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &pVulkanCurrentFrameResource->pCommandBuffer;

        // Specify which semaphores will be signaled once the command buffer(s) have finished execution.
        std::array<VkSemaphore, 1> vSemaphoresAfterCommandBufferFinished = {
            vImageSemaphores[iCurrentImageSemaphore].pQueueSubmitSemaphore};
        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(vSemaphoresAfterCommandBufferFinished.size());
        submitInfo.pSignalSemaphores = vSemaphoresAfterCommandBufferFinished.data();

        // Make fence to be in "unsignaled" state.
        result = vkResetFences(pLogicalDevice, 1, &pVulkanCurrentFrameResource->pFence);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(std::format("failed to reset a fence, error: {}", string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        PROFILE_SCOPE_START(QueueSubmit);

        // Submit command buffer(s) to the queue for execution.
        vImageSemaphores[iCurrentImageSemaphore].iUsedFrameResourceIndex = iCurrentFrameResourceIndex;
        result = vkQueueSubmit(pGraphicsQueue, 1, &submitInfo, pVulkanCurrentFrameResource->pFence);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(std::format(
                "failed to submit command buffer(s) for execution, error: {}", string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        PROFILE_SCOPE_END;

        // Prepare for presenting.
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = static_cast<uint32_t>(vSemaphoresAfterCommandBufferFinished.size());
        presentInfo.pWaitSemaphores = vSemaphoresAfterCommandBufferFinished.data();

        // Specify which swapchain/image to present.
        std::array<VkSwapchainKHR, 1> vSwapChains = {pSwapChain};
        presentInfo.swapchainCount = static_cast<uint32_t>(vSwapChains.size());
        presentInfo.pSwapchains = vSwapChains.data();
        presentInfo.pImageIndices = &iLastAcquiredImageIndex;

        // No using multiple swapchains so leave `pResults` empty as we will get result for our single
        // swapchain from `vkQueuePresentKHR`.
        presentInfo.pResults = nullptr;

        PROFILE_SCOPE_START(Present);

        // Present.
        result = vkQueuePresentKHR(pPresentQueue, &presentInfo);
        if (result != VK_SUCCESS) {
            if (result != VK_SUBOPTIMAL_KHR) [[unlikely]] {
                Error error(
                    std::format("failed to present a swapchain image, error: {}", string_VkResult(result)));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Mark swapchain as needs to be re-created.
            bNeedToRecreateSwapchain = true;
        }

        PROFILE_SCOPE_END;

        // Switch to the next semaphores index.
        iCurrentImageSemaphore = (iCurrentImageSemaphore + 1) % vImageSemaphores.size();
    }

    void VulkanRenderer::drawMeshesMainPassSpecificPipelines(
        const std::vector<Renderer::MeshesInFrustum::PipelineInFrustumInfo>& pipelinesOfSpecificType,
        VkCommandBuffer pCommandBuffer,
        size_t iCurrentFrameResourceIndex) {
        PROFILE_FUNC;

        // Prepare draw call counter to be used later.
        const auto pDrawCallCounter = getDrawCallCounter();

        // Prepare vertex buffer.
        static constexpr size_t iVertexBufferCount = 1;
        std::array<VkBuffer, iVertexBufferCount> vVertexBuffers{};
        const std::array<VkDeviceSize, iVertexBufferCount> vOffsets = {0};

        for (const auto& pipelineInfo : pipelinesOfSpecificType) {
            // Get internal resources of this pipeline.
            const auto pVulkanPipeline = reinterpret_cast<VulkanPipeline*>(pipelineInfo.pPipeline);

            // Get push constants and pipeline's resources.
            const auto pMtxPushConstants = pVulkanPipeline->getShaderConstants();
            auto pMtxPipelineResources = pVulkanPipeline->getInternalResources();

            // Lock both.
            std::scoped_lock guardPipelineResources(pMtxPipelineResources->first, pMtxPushConstants->first);

#if defined(DEBUG)
            // Self check: make sure push constants are used.
            if (!pMtxPushConstants->second.has_value()) [[unlikely]] {
                Error error(std::format(
                    "expected push constants to be used on the pipeline \"{}\"",
                    pVulkanPipeline->getPipelineIdentifier()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
#endif

            // Get push constants manager.
            const auto pPushConstantsManager = pMtxPushConstants->second->pConstantsManager.get();

            // Bind pipeline.
            vkCmdBindPipeline(
                pCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pMtxPipelineResources->second.pPipeline);

            // Bind descriptor sets.
            vkCmdBindDescriptorSets(
                pCommandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pMtxPipelineResources->second.pPipelineLayout,
                0,
                1,
                &pMtxPipelineResources->second.vDescriptorSets[iCurrentFrameResourceIndex],
                0,
                nullptr);

            for (const auto& materialInfo : pipelineInfo.vMaterials) {
                // Set material's GPU resources.
                const auto pMtxMaterialGpuResources = materialInfo.pMaterial->getMaterialGpuResources();

                // Note: if you will ever need it - don't lock material's internal resources mutex
                // here as it might cause a deadlock (see Material::setDiffuseTexture for example).
                std::scoped_lock materialGpuResourcesGuard(pMtxMaterialGpuResources->first);
                auto& materialShaderResources = pMtxMaterialGpuResources->second.shaderResources;

                // Set material's CPU-write buffers.
                for (const auto& [sResourceName, pShaderCpuWriteResource] :
                     materialShaderResources.shaderCpuWriteResources) {
                    pShaderCpuWriteResource.getResource()->copyResourceIndexToShaderConstants(
                        pPushConstantsManager, pVulkanPipeline, iCurrentFrameResourceIndex);
                }

                // Set material's textures.
                for (const auto& [sResourceName, pShaderTextureResource] :
                     materialShaderResources.shaderTextureResources) {
                    reinterpret_cast<GlslShaderTextureResourceBinding*>(pShaderTextureResource.getResource())
                        ->copyResourceIndexToPushConstants(pPushConstantsManager, pVulkanPipeline);
                }

                for (const auto& meshInfo : materialInfo.vMeshes) {
                    // Get mesh data.
                    auto pMtxMeshGpuResources = meshInfo.pMeshNode->getMeshGpuResources();

                    // Note: if you will ever need it - don't lock mesh node's spawning/despawning mutex here
                    // as it might cause a deadlock (see MeshNode::setMaterial for example).
                    std::scoped_lock geometryGuard(pMtxMeshGpuResources->first);

                    // Set mesh's CPU-write buffers.
                    for (const auto& [sResourceName, pShaderCpuWriteResource] :
                         pMtxMeshGpuResources->second.shaderResources.shaderCpuWriteResourceBindings) {
                        pShaderCpuWriteResource.getResource()->copyResourceIndexToShaderConstants(
                            pPushConstantsManager, pVulkanPipeline, iCurrentFrameResourceIndex);
                    }

                    // Set mesh's textures.
                    for (const auto& [sResourceName, pShaderTextureResource] :
                         pMtxMeshGpuResources->second.shaderResources.shaderTextureResources) {
                        reinterpret_cast<GlslShaderTextureResourceBinding*>(
                            pShaderTextureResource.getResource())
                            ->copyResourceIndexToPushConstants(pPushConstantsManager, pVulkanPipeline);
                    }

                    // Bind vertex buffer.
                    vVertexBuffers[0] = {reinterpret_cast<VulkanResource*>(
                                             pMtxMeshGpuResources->second.mesh.pVertexBuffer.get())
                                             ->getInternalBufferResource()};
                    vkCmdBindVertexBuffers(
                        pCommandBuffer,
                        0,
                        static_cast<uint32_t>(vVertexBuffers.size()),
                        vVertexBuffers.data(),
                        vOffsets.data());

                    // Set push constants.
                    vkCmdPushConstants(
                        pCommandBuffer,
                        pMtxPipelineResources->second.pPipelineLayout,
                        VK_SHADER_STAGE_ALL_GRAPHICS,
                        0,
                        pPushConstantsManager->getTotalSizeInBytes(),
                        pPushConstantsManager->getData());

                    // Iterate over all index buffers of a specific mesh node that use this material.
                    for (const auto& indexBufferInfo : meshInfo.vIndexBuffers) {
                        // Bind index buffer.
                        static_assert(
                            sizeof(MeshData::meshindex_t) == sizeof(unsigned int),
                            "change `indexTypeFormat`");
                        vkCmdBindIndexBuffer(
                            pCommandBuffer,
                            reinterpret_cast<VulkanResource*>(indexBufferInfo.pIndexBuffer)
                                ->getInternalBufferResource(),
                            0,
                            indexTypeFormat);

                        // Add a draw command.
                        vkCmdDrawIndexed(pCommandBuffer, indexBufferInfo.iIndexCount, 1, 0, 0, 0);

                        // Increment draw call counter.
                        pDrawCallCounter->fetch_add(1);
                    }
                }
            }
        }
    }

    void VulkanRenderer::onFramebufferSizeChangedDerived(int iWidth, int iHeight) {
        // Make sure the logical device is valid.
        if (pLogicalDevice == nullptr) {
            return;
        }

        if (iWidth == 0 && iHeight == 0) {
            // Don't draw anything as frame buffer size is zero.
            return;
        }

        if (pSwapChain == nullptr) {
            // Nothing to do.
            return;
        }

        // Recreate swap chain and other window-size dependent resources.
        auto optionalError = recreateSwapChainAndDependentResources();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }
    }

    std::optional<Error> VulkanRenderer::updateMsaaSampleCount() {
        // Make sure the physical device is valid.
        if (pPhysicalDevice == nullptr) [[unlikely]] {
            return Error("expected physical device to be valid at this point");
        }

        // Get maximum supported sample count.
        auto result = getMaxSupportedAntialiasingQuality();
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto maxSampleCount = std::get<std::optional<AntialiasingQuality>>(result);

        // First check if AA is supported at all.
        if (!maxSampleCount.has_value()) {
            // AA is not supported.
            msaaSampleCount = VK_SAMPLE_COUNT_1_BIT;
            return {};
        }
        const auto iMaxSampleCount = static_cast<unsigned int>(*maxSampleCount);

        // Get render setting.
        const auto mtxRenderSettings = getRenderSettings();
        std::scoped_lock guard(*mtxRenderSettings.first);

        // Get current AA sample count.
        const auto sampleQuality = mtxRenderSettings.second->getAntialiasingQuality();
        if (!sampleQuality.has_value()) [[unlikely]] {
            return Error("expected antialiasing to be supported");
        }
        const auto sampleCount = sampleQuality.value();
        const auto iSampleCount = static_cast<unsigned int>(sampleCount);

        // Make sure this sample count is supported.
        if (iSampleCount > iMaxSampleCount) [[unlikely]] {
            // This should never happen as render settings guarantee that returned AA quality is supported.
            return Error(std::format("expected the current AA quality {} to be supported", iSampleCount));
        }

        // Save sample count.
        bIsUsingMsaaRenderTarget = true;
        switch (sampleCount) {
        case (AntialiasingQuality::DISABLED): {
            msaaSampleCount = VK_SAMPLE_COUNT_1_BIT;
            bIsUsingMsaaRenderTarget = false;
            break;
        }
        case (AntialiasingQuality::MEDIUM): {
            msaaSampleCount = VK_SAMPLE_COUNT_2_BIT;
            break;
        }
        case (AntialiasingQuality::HIGH): {
            msaaSampleCount = VK_SAMPLE_COUNT_4_BIT;
            break;
        }
        }

        return {};
    }

    bool VulkanRenderer::dispatchComputeShadersOnGraphicsQueue(
        VulkanFrameResource* pCurrentFrameResource,
        size_t iCurrentFrameResourceIndex,
        std::unordered_map<Pipeline*, std::unordered_set<ComputeShaderInterface*>>&
            computePipelinesToSubmit) {
        // Make sure we have shaders to dispatch.
        if (computePipelinesToSubmit.empty()) {
            return false;
        }

        // Execution order synchronization is done outside of this function.

        for (const auto& [pPipeline, computeShaders] : computePipelinesToSubmit) {
            // Get internal resources of this pipeline.
            const auto pVulkanPipeline = reinterpret_cast<VulkanPipeline*>(pPipeline);
            auto pMtxPipelineResources = pVulkanPipeline->getInternalResources();
            std::scoped_lock guardPipelineResources(pMtxPipelineResources->first);

            // Bind pipeline.
            vkCmdBindPipeline(
                pCurrentFrameResource->pCommandBuffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                pMtxPipelineResources->second.pPipeline);

            // Bind descriptor sets.
            vkCmdBindDescriptorSets(
                pCurrentFrameResource->pCommandBuffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                pMtxPipelineResources->second.pPipelineLayout,
                0,
                1,
                &pMtxPipelineResources->second.vDescriptorSets[iCurrentFrameResourceIndex],
                0,
                nullptr);

            // Dispatch each compute shader.
            for (const auto& pComputeInterface : computeShaders) {
                reinterpret_cast<GlslComputeShaderInterface*>(pComputeInterface)
                    ->dispatchOnGraphicsQueue(pCurrentFrameResource->pCommandBuffer);
            }
        }

        // Clear map because we submitted all shaders.
        computePipelinesToSubmit.clear();

        return true;
    }

    std::optional<Error> VulkanRenderer::onRenderSettingsChangedDerived() {
        // Make sure no rendering is happening.
        std::scoped_lock guard(*getRenderResourcesMutex());
        waitForGpuToFinishWorkUpToThisPoint();

        // Update MSAA sample count using render settings.
        auto optionalError = updateMsaaSampleCount();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Re-create some resources.
        optionalError = recreateSwapChainAndDependentResources();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    void VulkanRenderer::waitForGpuToFinishUsingFrameResource(FrameResource* pFrameResource) {
        // Convert frame resource.
        const auto pVulkanFrameResource = reinterpret_cast<VulkanFrameResource*>(pFrameResource);

        // Wait for fence to be signaled.
        const auto result =
            vkWaitForFences(pLogicalDevice, 1, &pVulkanFrameResource->pFence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(std::format("failed to wait for a fence, error: {}", string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    bool VulkanRenderer::isInitialized() const { return bIsVulkanInitialized; }

    std::variant<std::vector<const char*>, Error> VulkanRenderer::getRequiredVulkanInstanceExtensions() {
        // Get extensions for window surface.
        uint32_t iGlfwExtensionCount = 0;
        const char** pGlfwExtensions = nullptr;
        pGlfwExtensions = glfwGetRequiredInstanceExtensions(&iGlfwExtensionCount);
        if (pGlfwExtensions == nullptr) {
            return Error("failed to get Vulkan instance window extensions from GLFW");
        }

        // Prepare array to return.
        std::vector<const char*> vRequiredExtensions;
        vRequiredExtensions.reserve(iGlfwExtensionCount);

        // Add GLFW extensions.
        for (uint32_t i = 0; i < iGlfwExtensionCount; i++) {
            vRequiredExtensions.push_back(pGlfwExtensions[i]);
        }

#if defined(DEBUG)
        // Add extension to use custom message callback for validation layers.
        vRequiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

        return vRequiredExtensions;
    }

    std::variant<std::string, Error>
    VulkanRenderer::isGpuSupportsUsedDeviceExtensions(VkPhysicalDevice pGpuDevice) {
        // Get the number of available device extensions.
        uint32_t iAvailableDeviceExtensionCount;
        auto result = vkEnumerateDeviceExtensionProperties(
            pGpuDevice, nullptr, &iAvailableDeviceExtensionCount, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to enumerate the number of available device extensions, error: {}",
                string_VkResult(result)));
        }

        // Get available device extensions.
        std::vector<VkExtensionProperties> vAvailableDeviceExtensions(iAvailableDeviceExtensionCount);
        result = vkEnumerateDeviceExtensionProperties(
            pGpuDevice, nullptr, &iAvailableDeviceExtensionCount, vAvailableDeviceExtensions.data());
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to enumerate available device extensions, error: {}", string_VkResult(result)));
        }

        // Make sure all required device extensions are available on this GPU.
        for (const auto& pRequiredExtensionName : vUsedDeviceExtensionNames) {
            bool bFound = false;
            auto sRequiredExtensionName = std::string(pRequiredExtensionName);

            for (const auto& availableExtensionInfo : vAvailableDeviceExtensions) {
                if (sRequiredExtensionName == availableExtensionInfo.extensionName) {
                    bFound = true;
                    break;
                }
            }

            if (!bFound) {
                return sRequiredExtensionName;
            }
        }

        return "";
    }

#if defined(DEBUG)
    std::optional<Error> VulkanRenderer::makeSureUsedValidationLayersSupported() {
        // Get total number of available Vulkan instance validation layers.
        uint32_t iAvailableValidationLayerCount = 0;
        auto result = vkEnumerateInstanceLayerProperties(&iAvailableValidationLayerCount, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to enumerate available Vulkan instance validation layer count, error: {}",
                string_VkResult(result)));
        }

        // Get available Vulkan instance validation layers.
        std::vector<VkLayerProperties> vAvailableValidationLayers(iAvailableValidationLayerCount);
        result = vkEnumerateInstanceLayerProperties(
            &iAvailableValidationLayerCount, vAvailableValidationLayers.data());
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(std::format(
                "failed to enumerate available Vulkan instance validation layers, error: {}",
                string_VkResult(result)));
        }

        // Make sure that all used validation layers are available.
        for (const auto& sUsedValidationLayerName : vUsedValidationLayerNames) {
            bool bIsLayerAvailable = false;

            // See if this layer is available.
            for (const auto& availableValidationLayerInfo : vAvailableValidationLayers) {
                if (std::string(availableValidationLayerInfo.layerName) == sUsedValidationLayerName) {
                    bIsLayerAvailable = true;
                    break;
                }
            }

            if (!bIsLayerAvailable) {
                return Error(std::format(
                    "Vulkan instance validation layer \"{}\" was requested but is not available",
                    sUsedValidationLayerName));
            }
        }

        return {};
    }

    VkResult VulkanRenderer::createDebugUtilsMessengerEXT(
        VkInstance pInstance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger) {
        auto pFunction = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(pInstance, "vkCreateDebugUtilsMessengerEXT"));
        if (pFunction != nullptr) {
            return pFunction(pInstance, pCreateInfo, pAllocator, pDebugMessenger);
        }

        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    void VulkanRenderer::destroyDebugUtilsMessengerEXT(
        VkInstance pInstance,
        VkDebugUtilsMessengerEXT pDebugMessenger,
        const VkAllocationCallbacks* pAllocator) {
        auto pFunction = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            pInstance, "vkDestroyDebugUtilsMessengerEXT");
        if (pFunction != nullptr) {
            return pFunction(pInstance, pDebugMessenger, pAllocator);
        }

        Logger::get().error(std::format("unable to load \"vkDestroyDebugUtilsMessengerEXT\" function"));
    }

    PFN_vkCmdBeginDebugUtilsLabelEXT
    VulkanRenderer::requestVkCmdBeginDebugUtilsLabelEXT(VkInstance pInstance) {
        auto pFunction = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(pInstance, "vkCmdBeginDebugUtilsLabelEXT"));
        if (pFunction == nullptr) [[unlikely]] {
            Error error("failed to get function address");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        return pFunction;
    }

    PFN_vkCmdEndDebugUtilsLabelEXT VulkanRenderer::requestVkCmdEndDebugUtilsLabelEXT(VkInstance pInstance) {
        auto pFunction = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(pInstance, "vkCmdEndDebugUtilsLabelEXT"));
        if (pFunction == nullptr) [[unlikely]] {
            Error error("failed to get function address");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        return pFunction;
    }
#endif
} // namespace ne
