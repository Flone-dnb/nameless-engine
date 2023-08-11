#include "render/vulkan/VulkanRenderer.h"

// Custom.
#include "game/nodes/CameraNode.h"
#include "render/general/pipeline/PipelineManager.h"
#include "window/GLFW.hpp"
#include "game/GameManager.h"
#include "game/Window.h"
#include "materials/glsl/GlslEngineShaders.hpp"
#include "render/vulkan/resources/VulkanResourceManager.h"
#include "render/vulkan/resources/VulkanFrameResource.h"
#include "render/general/resources/FrameResourcesManager.h"
#include "render/vulkan/pipeline/VulkanPushConstantsManager.hpp"
#include "game/camera/CameraManager.h"
#include "game/camera/CameraProperties.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"
#include "materials/Material.h"
#include "game/nodes/MeshNode.h"
#include "materials/glsl/GlslShaderResource.h"
#include "game/camera/TransientCamera.h"

// External.
#include "vulkan/vk_enum_string_helper.h"

namespace ne {
    VulkanRenderer::VulkanRenderer(GameManager* pGameManager) : Renderer(pGameManager) {}

    std::vector<ShaderDescription> VulkanRenderer::getEngineShadersToCompile() const {
        return {GlslEngineShaders::meshNodeVertexShader, GlslEngineShaders::meshNodeFragmentShader};
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
                    fmt::format("failed to wait for device to be idle, error: {}", string_VkResult(result)));
                return;
            }
        }

        destroySwapChainAndDependentResources();

        if (pLogicalDevice != nullptr) {
            // Explicitly delete frame resources manager before command pool because command buffers
            // in frame resources use command pool to be freed.
            // Also delete frame resources before GPU resource manager because they use memory allocator
            // for destruction.
            resetFrameResourcesManager();

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

    std::optional<Error> VulkanRenderer::initialize() {
        std::scoped_lock frameGuard(*getRenderResourcesMutex());

        // Initialize essential entities.
        auto optionalError = initializeRenderer();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Initialize Vulkan.
        optionalError = initializeVulkan();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> VulkanRenderer::initializeVulkan() {
        // Create Vulkan instance.
        auto optionalError = createVulkanInstance();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create window surface.
        optionalError = createWindowSurface();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Pick physical device.
        optionalError = pickPhysicalDevice();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create logical device.
        optionalError = createLogicalDevice();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create swap chain.
        optionalError = createSwapChain();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create render pass.
        optionalError = createRenderPass();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create command pool.
        optionalError = createCommandPool();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Initialize resource managers.
        optionalError = initializeResourceManagers();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        {
            auto mtxAllFrameResource = getFrameResourcesManager()->getAllFrameResources();
            std::scoped_lock frameResourceGuard(*mtxAllFrameResource.first);

            for (size_t i = 0; i < mtxAllFrameResource.second.size(); i++) {
                // Self check: make sure allocated frame resource is of expected type
                // so we may just `reinterpret_cast` later because they won't change.
                const auto pVulkanFrameResource =
                    dynamic_cast<VulkanFrameResource*>(mtxAllFrameResource.second[i]);
                if (pVulkanFrameResource == nullptr) [[unlikely]] {
                    return Error("expected a Vulkan frame resource");
                }

                // Save refs to frame resource fences.
                vSwapChainImageFenceRefs[i] = pVulkanFrameResource->pFence;
            }
        }

        // Now that GPU resource manager is created:
        // Create depth image.
        optionalError = createDepthImage();
        if (optionalError.has_value()) {
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
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // ... TODO ...

        bIsVulkanInitialized = true;

        return {};
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL validationLayerMessageCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
        Logger::get().error(fmt::format("[validation layer] {}", pCallbackData->pMessage));
        return VK_FALSE;
    }

    std::optional<Error> VulkanRenderer::createVulkanInstance() {
        // Check which extensions are available.
        uint32_t iExtensionCount = 0;
        auto result = vkEnumerateInstanceExtensionProperties(nullptr, &iExtensionCount, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
                "failed to enumerate available Vulkan instance extension count, error: {}",
                string_VkResult(result)));
        }
        std::vector<VkExtensionProperties> vExtensions(iExtensionCount);
        result = vkEnumerateInstanceExtensionProperties(nullptr, &iExtensionCount, vExtensions.data());
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
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
        Logger::get().info(fmt::format("{} validation layer(s) enabled", createInfo.enabledLayerCount));

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
            return Error(fmt::format(
                "failed to create Vulkan instance, make sure your GPU drivers are updated, error: {}",
                string_VkResult(result)));
        }

#if defined(DEBUG)
        // Make validation layers use our custom message callback.
        result = createDebugUtilsMessengerEXT(
            pInstance, &debugMessengerCreateInfo, nullptr, &pValidationLayerDebugMessenger);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
                "failed to create validation layer debug messenger, error: {}", string_VkResult(result)));
        }
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
            return Error(fmt::format("failed to create window surface, error: {}", string_VkResult(result)));
        }

        return {};
    }

    size_t VulkanRenderer::rateGpuSuitability(VkPhysicalDevice pGpuDevice) {
        // Get device properties.
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(pGpuDevice, &deviceProperties);

        // Make sure this device is suitable for this renderer.
        std::string sErrorDescription;
        auto result = isDeviceSuitable(pGpuDevice);
        if (std::holds_alternative<Error>(result)) {
            const auto error = std::get<Error>(std::move(result));
            Logger::get().info(fmt::format(
                "failed to test if the GPU \"{}\" is suitable due to the following error: {}",
                deviceProperties.deviceName,
                error.getFullErrorMessage()));
            return 0;
        }
        const auto sMissingSupportMessage = std::get<std::string>(std::move(result));
        if (!sMissingSupportMessage.empty()) {
            Logger::get().info(fmt::format("{} and thus cannon be used", sErrorDescription));
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

        return iFinalScore;
    }

    std::variant<std::string, Error> VulkanRenderer::isDeviceSuitable(VkPhysicalDevice pGpu) {
        // Get device properties.
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(pGpu, &deviceProperties);

        // Make sure this GPU supports used Vulkan version.
        if (deviceProperties.apiVersion < iUsedVulkanVersion) {
            return fmt::format(
                "GPU \"{}\" does not support used Vulkan version", deviceProperties.deviceName);
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
            return fmt::format(
                "GPU \"{}\" does not support all required queue families", deviceProperties.deviceName);
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
            return fmt::format(
                "GPU \"{}\" does not support required device extension \"{}\"",
                deviceProperties.deviceName,
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
        const auto sMissingSwapChainDetailDescription = std::get<std::string>(std::move(result));
        if (!sMissingSwapChainDetailDescription.empty()) {
            return sMissingSwapChainDetailDescription;
        }

        // Get supported device features.
        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(pGpu, &supportedFeatures);

        // Make sure anisotropic filtering is supported.
        if (supportedFeatures.samplerAnisotropy == VK_FALSE) {
            return fmt::format(
                "GPU \"{}\" does not support anisotropic filtering", deviceProperties.deviceName);
        }

        // Make sure that maximum push constants size that we use is supported.
        if (VulkanPushConstantsManager::getMaxPushConstantsSizeInBytes() >
            deviceProperties.limits.maxPushConstantsSize) {
            return fmt::format(
                "GPU \"{}\" max push constants size is only {} while we expect {}",
                deviceProperties.deviceName,
                deviceProperties.limits.maxPushConstantsSize,
                VulkanPushConstantsManager::getMaxPushConstantsSizeInBytes());
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
            return Error(fmt::format(
                "failed to query physical device capabilities, error: {}", string_VkResult(result)));
        }

        // Query supported surface format count.
        uint32_t iSupportedFormatCount = 0;
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(pGpu, pWindowSurface, &iSupportedFormatCount, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
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
                return Error(fmt::format(
                    "failed to query supported physical device surface formats, error: {}",
                    string_VkResult(result)));
            }
        }

        // Query supported presentation mode count.
        uint32_t iSupportedPresentationModeCount = 0;
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(
            pGpu, pWindowSurface, &iSupportedPresentationModeCount, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
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
                return Error(fmt::format(
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
                return Error(fmt::format(
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
            return fmt::format(
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
            return fmt::format(
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
            return fmt::format(
                "GPU \"{}\" swap chain does not support immediate present mode", deviceProperties.deviceName);
        }
        if (!bFoundDefaultFifoPresentMode) {
            return fmt::format(
                "GPU \"{}\" swap chain does not support default FIFO present mode",
                deviceProperties.deviceName);
        }

        // Make sure it supports used number of images in the swap chain.
        constexpr auto iSwapChainImageCount = getSwapChainBufferCount();
        if (iSwapChainImageCount < swapChainSupportDetails.capabilities.minImageCount) {
            return fmt::format(
                "GPU \"{}\" swap chain does not support used swap chain image count (used: {}, "
                "supported min: {})",
                deviceProperties.deviceName,
                iSwapChainImageCount,
                swapChainSupportDetails.capabilities.minImageCount);
        }
        // 0 max image count means "no limit" so we only check if it's not 0
        if (swapChainSupportDetails.capabilities.maxImageCount > 0 &&
            iSwapChainImageCount > swapChainSupportDetails.capabilities.maxImageCount) {
            return fmt::format(
                "GPU \"{}\" swap chain does not support used swap chain image count (used: {}, "
                "supported max: {})",
                deviceProperties.deviceName,
                iSwapChainImageCount,
                swapChainSupportDetails.capabilities.maxImageCount);
        }

        return "";
    }

    std::optional<Error> VulkanRenderer::pickPhysicalDevice() {
        // Get total GPU count.
        uint32_t iSupportedGpuCount = 0;
        auto result = vkEnumeratePhysicalDevices(pInstance, &iSupportedGpuCount, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(
                fmt::format("failed to enumerate physical device count, error: {}", string_VkResult(result)));
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
                fmt::format("failed to enumerate physical devices, error: {}", string_VkResult(result)));
        }

        // Pick a GPU with the highest suitability score.
        struct GpuScore {
            size_t iScore = 0;
            VkPhysicalDevice pGpu = nullptr;
            std::string sGpuName;
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

        // Log rated GPUs by score.
        std::string sRating = fmt::format("found and rated {} suitable GPU(s):", vScores.size());
        for (size_t i = 0; i < vScores.size(); i++) {
            sRating +=
                fmt::format("\n{}. {}, suitability score: {}", i + 1, vScores[i].sGpuName, vScores[i].iScore);
        }
        Logger::get().info(sRating);

        // Get render settings.
        const auto pMtxRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(pMtxRenderSettings->first);

        // Check if the GPU to use is set.
        auto sGpuNameToUse = pMtxRenderSettings->second->getGpuToUse();
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
                Logger::get().info(fmt::format(
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

            // Save (cache) queue family indices of this device.
            auto queueFamilyIndicesResult = queryQueueFamilyIndices(currentGpuInfo.pGpu);
            if (std::holds_alternative<Error>(queueFamilyIndicesResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(queueFamilyIndicesResult));
                error.addCurrentLocationToErrorStack();
                Logger::get().error(fmt::format(
                    "failed to query queue family indices for the rated GPU \"{}\"",
                    currentGpuInfo.sGpuName));
                continue;
            }
            physicalDeviceQueueFamilyIndices =
                std::get<QueueFamilyIndices>(std::move(queueFamilyIndicesResult));

            // Log used GPU.
            if (sGpuNameToUse == currentGpuInfo.sGpuName) {
                Logger::get().info(fmt::format(
                    "using the following GPU: \"{}\" (was specified as preferred in the renderer's "
                    "config file)",
                    currentGpuInfo.sGpuName));
            } else {
                Logger::get().info(fmt::format("using the following GPU: \"{}\"", currentGpuInfo.sGpuName));
            }

            pPhysicalDevice = currentGpuInfo.pGpu;

            // Save GPU name in the settings.
            pMtxRenderSettings->second->setGpuToUse(currentGpuInfo.sGpuName);

            break;
        }

        if (pPhysicalDevice == nullptr) [[unlikely]] {
            return Error(fmt::format(
                "found {} suitable GPU(s) but failed to query queue family indices", vScores.size()));
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

        // Specify features that we need.
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        // Fill info to create a logical device.
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(vQueueCreateInfo.size());
        createInfo.pQueueCreateInfos = vQueueCreateInfo.data();
        createInfo.pEnabledFeatures = &deviceFeatures;

        // Specify needed device extensions.
        createInfo.enabledExtensionCount = static_cast<uint32_t>(vUsedDeviceExtensionNames.size());
        createInfo.ppEnabledExtensionNames = vUsedDeviceExtensionNames.data();

#if defined(DEBUG)
        // Setup validation layers (for compatibility with older implementations).
        createInfo.enabledLayerCount = static_cast<uint32_t>(vUsedValidationLayerNames.size());
        createInfo.ppEnabledLayerNames = vUsedValidationLayerNames.data();
#endif

        // Create device.
        const auto result = vkCreateDevice(pPhysicalDevice, &createInfo, nullptr, &pLogicalDevice);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format("failed to create logical device, error: {}", string_VkResult(result)));
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

        // Prepare swap chain creation info.
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = pWindowSurface;

        const auto iSwapChainImageCount = getSwapChainBufferCount();

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

        // Specify other parameters.
        createInfo.preTransform = swapChainSupportDetails.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        // Create swap chain.
        auto result = vkCreateSwapchainKHR(pLogicalDevice, &createInfo, nullptr, &pSwapChain);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format("failed to create swap chain, error: {}", string_VkResult(result)));
        }

        // Query image count in the created swap chain.
        uint32_t iActualSwapChainImageCount = 0;
        result = vkGetSwapchainImagesKHR(pLogicalDevice, pSwapChain, &iActualSwapChainImageCount, nullptr);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
                "failed to save references to swap chain image count, error: {}", string_VkResult(result)));
        }

        // Make sure the requested number of images was created.
        if (iSwapChainImageCount != iActualSwapChainImageCount) [[unlikely]] {
            return Error(fmt::format(
                "failed to created swap chain images, requested: {}, created: {}",
                iSwapChainImageCount,
                iActualSwapChainImageCount));
        }

        // Save references to swap chain images.
        result = vkGetSwapchainImagesKHR(
            pLogicalDevice, pSwapChain, &iActualSwapChainImageCount, vSwapChainImages.data());
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
                "failed to save references to swap chain images, error: {}", string_VkResult(result)));
        }

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
                return Error(fmt::format("failed to create image view, error: {}", string_VkResult(result)));
            }
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
            return Error(fmt::format("failed to create command pool, error: {}", string_VkResult(result)));
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

    std::optional<Error> VulkanRenderer::createRenderPass() {
        std::vector<VkAttachmentDescription> vAttachments{};

        static_assert(
            iRenderPassColorAttachmentIndex != iRenderPassDepthAttachmentIndex &&
                iRenderPassColorAttachmentIndex != iRenderPassColorResolveTargetAttachmentIndex,
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
        colorAttachment.finalLayout =
            bEnableMsaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vAttachments.push_back(colorAttachment);
        static_assert(iRenderPassColorAttachmentIndex == 0);
        if (vAttachments.size() != iRenderPassColorAttachmentIndex + 1) [[unlikely]] {
            return Error("unexpected attachment index");
        }

        // Describe depth buffer.
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = depthImageFormat;
        depthAttachment.samples = msaaSampleCount;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        vAttachments.push_back(depthAttachment);
        static_assert(iRenderPassDepthAttachmentIndex == 1);
        if (vAttachments.size() != iRenderPassDepthAttachmentIndex + 1) [[unlikely]] {
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
            colorResolveTargetAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            vAttachments.push_back(colorResolveTargetAttachment);
            static_assert(iRenderPassColorResolveTargetAttachmentIndex == 2);
            if (vAttachments.size() != iRenderPassColorResolveTargetAttachmentIndex + 1) [[unlikely]] {
                return Error("unexpected attachment index");
            }
        }

        // Create color buffer reference for subpasses.
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = iRenderPassColorAttachmentIndex;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Create depth buffer reference for subpasses.
        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = iRenderPassDepthAttachmentIndex;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentResolveRef{};
        if (bEnableMsaa) {
            // Create reference to resolve target for subpasses.
            colorAttachmentResolveRef.attachment = iRenderPassColorResolveTargetAttachmentIndex;
            colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        // Describe subpass.
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        if (bEnableMsaa) {
            subpass.pResolveAttachments = &colorAttachmentResolveRef;
        }

        // Describe subpass dependency.
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // Describe render pass.
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(vAttachments.size());
        renderPassInfo.pAttachments = vAttachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        // Create render pass.
        const auto result = vkCreateRenderPass(pLogicalDevice, &renderPassInfo, nullptr, &pRenderPass);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format("failed to create render pass, error: {}", string_VkResult(result)));
        }

        return {};
    }

    void VulkanRenderer::destroySwapChainAndDependentResources() {
        if (pLogicalDevice == nullptr || pSwapChain == nullptr) {
            return;
        }

        // Explicitly destroy MSAA resource before resource manager is destroyed.
        pMsaaImage = nullptr;

        // Explicitly destroy depth resource before resource manager is destroyed.
        pDepthImage = nullptr;

        // Destroy swap chain framebuffers.
        for (size_t i = 0; i < vSwapChainFramebuffers.size(); i++) {
            vkDestroyFramebuffer(pLogicalDevice, vSwapChainFramebuffers[i], nullptr);
            vSwapChainFramebuffers[i] = nullptr;
        }

        // Make sure all pipelines were destroyed because they reference render pass.
        resetPipelineManager();

        // Now when all pipelines were destroyed:
        // Destroy render pass.
        vkDestroyRenderPass(pLogicalDevice, pRenderPass, nullptr);
        pRenderPass = nullptr;

        // Destroy swap chain image views.
        for (size_t i = 0; i < vSwapChainImageViews.size(); i++) {
            vkDestroyImageView(pLogicalDevice, vSwapChainImageViews[i], nullptr);
            vSwapChainImageViews[i] = nullptr;
        }

        // Destroy swap chain.
        vkDestroySwapchainKHR(pLogicalDevice, pSwapChain, nullptr);
        pSwapChain = nullptr;
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
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        pDepthImage = std::get<std::unique_ptr<VulkanResource>>(std::move(result));

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
        if (pRenderPass == nullptr) [[unlikely]] {
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

        // Make sure framebuffer array size is equal to image views array size.
        if (vSwapChainFramebuffers.size() != vSwapChainImageViews.size()) [[unlikely]] {
            return Error(std::format(
                "swapchain framebuffer array size ({}) is not equal to swapchain image view array size ({}), "
                "swapchain framebuffers wrap swapchain images thus framebuffer count "
                "should be equal to swapchain image count",
                vSwapChainFramebuffers.size(),
                vSwapChainImageViews.size()));
        }

        for (size_t i = 0; i < vSwapChainImageViews.size(); i++) {
            // Prepare image views to render pass attachments that framebuffer will reference.
            std::vector<VkImageView> vAttachments;

            // Specify color attachment.
            static_assert(iRenderPassColorAttachmentIndex == 0);
            if (bEnableMsaa) {
                vAttachments.push_back(pMsaaImage->getInternalImageView());
            } else {
                vAttachments.push_back(vSwapChainImageViews[i]);
            }

            // Specify depth attachment.
            static_assert(iRenderPassDepthAttachmentIndex == 1);
            vAttachments.push_back(pDepthImage->getInternalImageView());

            // Specify color resolve target attachment.
            static_assert(iRenderPassColorResolveTargetAttachmentIndex == 2);
            if (bEnableMsaa) {
                vAttachments.push_back(vSwapChainImageViews[i]);
            }

            // Describe framebuffer.
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = pRenderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(vAttachments.size());
            framebufferInfo.pAttachments = vAttachments.data();
            framebufferInfo.width = swapChainExtent->width;
            framebufferInfo.height = swapChainExtent->height;
            framebufferInfo.layers = 1;

            // Create framebuffer.
            const auto result =
                vkCreateFramebuffer(pLogicalDevice, &framebufferInfo, nullptr, &vSwapChainFramebuffers[i]);
            if (result != VK_SUCCESS) [[unlikely]] {
                return Error(fmt::format(
                    "failed to create a framebuffer for a swapchain image view, error: {}",
                    string_VkResult(result)));
            }
        }

        return {};
    }

    std::optional<Error> VulkanRenderer::prepareForDrawingNextFrame(CameraProperties* pCameraProperties) {
        std::scoped_lock frameGuard(*getRenderResourcesMutex());

        // Make sure swap chain extent is set.
        if (!swapChainExtent.has_value()) [[unlikely]] {
            return Error("expected swap chain extent to be set at this point");
        }

        // Waits for frame resource to be no longer used by the GPU.
        updateResourcesForNextFrame(swapChainExtent->width, swapChainExtent->height, pCameraProperties);

        // Get command buffer to reset it.
        auto pMtxCurrentFrameResource = getFrameResourcesManager()->getCurrentFrameResource();
        std::scoped_lock frameResourceGuard(pMtxCurrentFrameResource->first);

        // Convert frame resource.
        const auto pVulkanFrameResource =
            reinterpret_cast<VulkanFrameResource*>(pMtxCurrentFrameResource->second.pResource);

        // Reset command buffer.
        auto result = vkResetCommandBuffer(pVulkanFrameResource->pCommandBuffer, 0);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format("failed to reset command buffer, error: {}", string_VkResult(result)));
        }

        // Prepare to start recording commands.
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;

        // Mark start of command recording.
        result = vkBeginCommandBuffer(pVulkanFrameResource->pCommandBuffer, &beginInfo);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
                "failed to start recording commands into a command buffer, error: {}",
                string_VkResult(result)));
        }

        // Prepare to begin render pass.
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = pRenderPass;
        renderPassInfo.framebuffer =
            vSwapChainFramebuffers[pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex];

        // Specify render area.
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = *swapChainExtent;

        // Specify clear color for attachments.
        std::array<VkClearValue, 2> clearValues{};
        clearValues[iRenderPassColorAttachmentIndex].color = {0.0F, 0.0F, 0.0F, 1.0F};
        clearValues[iRenderPassDepthAttachmentIndex].depthStencil = {getMaxDepth(), 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        // Mark render pass start.
        vkCmdBeginRenderPass(
            pVulkanFrameResource->pCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        return {};
    }

    std::variant<std::unique_ptr<Renderer>, Error> VulkanRenderer::create(GameManager* pGameManager) {
        // Create an empty (uninitialized) Vulkan renderer.
        auto pRenderer = std::unique_ptr<VulkanRenderer>(new VulkanRenderer(pGameManager));

        // Initialize renderer.
        const auto optionalError = pRenderer->initialize();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        return pRenderer;
    }

    std::variant<std::vector<std::string>, Error> VulkanRenderer::getSupportedGpuNames() const {
        return vSupportedGpuNames;
    }

    std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
    VulkanRenderer::getSupportedRenderResolutions() const {
        throw std::runtime_error("not implemented");
    }

    std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
    VulkanRenderer::getSupportedRefreshRates() const {
        throw std::runtime_error("not implemented");
    }

    RendererType VulkanRenderer::getType() const { return RendererType::VULKAN; }

    std::string VulkanRenderer::getUsedApiVersion() const {
        static_assert(iUsedVulkanVersion == VK_API_VERSION_1_0, "update returned version string");
        return "1.0";
    }

    std::string VulkanRenderer::getCurrentlyUsedGpuName() const {
        throw std::runtime_error("not implemented");
    }

    size_t VulkanRenderer::getTotalVideoMemoryInMb() const { throw std::runtime_error("not implemented"); }

    size_t VulkanRenderer::getUsedVideoMemoryInMb() const { throw std::runtime_error("not implemented"); }

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
        const auto pFrameResourcesManager = getFrameResourcesManager();
        if (pFrameResourcesManager == nullptr) [[unlikely]] {
            Error error("expected frame resource manager to be valid at this point");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get all frame resources.
        const auto mtxAllFrameResources = pFrameResourcesManager->getAllFrameResources();

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
            Error error(fmt::format("failed to wait for a fence, error: {}", string_VkResult(result)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    VkDevice VulkanRenderer::getLogicalDevice() const { return pLogicalDevice; }

    VkPhysicalDevice VulkanRenderer::getPhysicalDevice() const { return pPhysicalDevice; }

    VkInstance VulkanRenderer::getInstance() const { return pInstance; }

    VkRenderPass VulkanRenderer::getRenderPass() const { return pRenderPass; }

    VkCommandPool VulkanRenderer::getCommandPool() const { return pCommandPool; }

    std::optional<VkExtent2D> VulkanRenderer::getSwapChainExtent() const { return swapChainExtent; }

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
            return Error(fmt::format(
                "failed to create a one-time submit command buffer, error: {}", string_VkResult(result)));
        }

        // Prepare to record command to the one-time submit command buffer.
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // we will submit once and destroy

        // Start recording commands.
        result = vkBeginCommandBuffer(pOneTimeSubmitCommandBuffer, &beginInfo);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
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
            return Error(fmt::format(
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
            return Error(fmt::format("failed to create a fence, error: {}", string_VkResult(result)));
        }

        // Prepare to execute the commands.
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &pOneTimeSubmitCommandBuffer;

        // Execute the commands.
        result = vkQueueSubmit(pGraphicsQueue, 1, &submitInfo, pTemporaryFence);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
                "failed to submit commands of a one-time submit command buffer, error: {}",
                string_VkResult(result)));
        }

        // Wait for the fence to be signaled.
        result = vkWaitForFences(pLogicalDevice, 1, &pTemporaryFence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(
                fmt::format("failed to wait for a temporary fence, error: {}", string_VkResult(result)));
        }

        // Destroy the fence.
        vkDestroyFence(pLogicalDevice, pTemporaryFence, nullptr);

        // Free temporary command buffer.
        vkFreeCommandBuffers(pLogicalDevice, pCommandPool, 1, &pOneTimeSubmitCommandBuffer);

        return {};
    }

    void VulkanRenderer::drawNextFrame() {
        if (bIsWindowMinimized) {
            // Framebuffer size is zero and swap chain is invalid, wait until the window is
            // restored/maximized.
            return;
        }

        // Get active camera.
        const auto pMtxActiveCamera = getGameManager()->getCameraManager()->getActiveCamera();

        // Lock both camera and draw mutex.
        std::scoped_lock renderGuard(pMtxActiveCamera->first, *getRenderResourcesMutex());

        // Get camera properties of the active camera.
        CameraProperties* pActiveCameraProperties = nullptr;
        if (pMtxActiveCamera->second.pCameraNode != nullptr) {
            pActiveCameraProperties = pMtxActiveCamera->second.pCameraNode->getCameraProperties();
        } else if (pMtxActiveCamera->second.pTransientCamera != nullptr) {
            pActiveCameraProperties = pMtxActiveCamera->second.pTransientCamera->getCameraProperties();
        } else {
            // No active camera.
            return;
        }

        // don't unlock active camera mutex until finished submitting the next frame for drawing

        // Setup.
        auto optionalError = prepareForDrawingNextFrame(pActiveCameraProperties);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Lock frame resources to use them (see below).
        auto pMtxCurrentFrameResource = getFrameResourcesManager()->getCurrentFrameResource();
        std::scoped_lock frameResourceGuard(pMtxCurrentFrameResource->first);
        const auto pVulkanCurrentFrameResource =
            reinterpret_cast<VulkanFrameResource*>(pMtxCurrentFrameResource->second.pResource);

        // Iterate over all pipelines.
        const auto pCreatedGraphicsPipelines = getPipelineManager()->getGraphicsPipelines();
        for (auto& [mtx, map] : *pCreatedGraphicsPipelines) {
            std::scoped_lock pipelineGuard(mtx);

            for (const auto& [sPipelineId, pPipeline] : map) {
                auto pMtxPipelineResources =
                    reinterpret_cast<VulkanPipeline*>(pPipeline.get())->getInternalResources();

                std::scoped_lock guardPipelineResources(pMtxPipelineResources->first);

                // Bind pipeline.
                vkCmdBindPipeline(
                    pVulkanCurrentFrameResource->pCommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pMtxPipelineResources->second.pPipeline);

                // Bind descriptor sets.
                vkCmdBindDescriptorSets(
                    pVulkanCurrentFrameResource->pCommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pMtxPipelineResources->second.pPipelineLayout,
                    0,
                    1,
                    &pMtxPipelineResources->second
                         .vDescriptorSets[pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex],
                    0,
                    nullptr);

                // Iterate over all materials that use this pipeline.
                const auto pMtxMaterials = pPipeline->getMaterialsThatUseThisPipeline();
                std::scoped_lock materialsGuard(pMtxMaterials->first);

                for (const auto& pMaterial : pMtxMaterials->second) {
                    // Set material's GPU resources.
                    const auto pMtxMaterialGpuResources = pMaterial->getMaterialGpuResources();
                    std::scoped_lock materialGpuResourcesGuard(pMtxMaterialGpuResources->first);

                    // Set material's CPU write shader resources.
                    for (const auto& [sResourceName, pShaderCpuWriteResource] :
                         pMtxMaterialGpuResources->second.shaderResources.shaderCpuWriteResources) {
                        reinterpret_cast<GlslShaderCpuWriteResource*>(pShaderCpuWriteResource.getResource())
                            ->copyResourceIndexToPushConstants(
                                pMtxPipelineResources->second.pPushConstantsManager.get(),
                                pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex);
                    }

                    // Draw mesh nodes.
                    drawMeshNodes(
                        pMaterial,
                        pVulkanCurrentFrameResource->pCommandBuffer,
                        pMtxPipelineResources->second.pPushConstantsManager.get(),
                        pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex);
                }
            }
        }

        // Do finish logic.
        optionalError = finishDrawingNextFrame(pVulkanCurrentFrameResource);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    void VulkanRenderer::drawMeshNodes(
        Material* pMaterial,
        VkCommandBuffer pCommandBuffer,
        VulkanPushConstantsManager* pPushConstantsManager,
        size_t iCurrentFrameResourceIndex) {
        const auto pMtxMeshNodes = pMaterial->getSpawnedMeshNodesThatUseThisMaterial();

        // Prepare vertex buffers.
        static constexpr size_t iVertexBufferCount = 1;
        std::array<VkBuffer, iVertexBufferCount> vVertexBuffers{};
        std::array<VkDeviceSize, iVertexBufferCount> vOffsets = {0};

        // Iterate over all visible mesh nodes that use this material.
        std::scoped_lock meshNodesGuard(pMtxMeshNodes->first);
        for (const auto& pMeshNode : pMtxMeshNodes->second.visibleMeshNodes) {
            // Get mesh data.
            auto pMtxMeshGpuResources = pMeshNode->getMeshGpuResources();
            auto mtxMeshData = pMeshNode->getMeshData();

            std::scoped_lock geometryGuard(pMtxMeshGpuResources->first, *mtxMeshData.first);

            // Bind vertex buffers.
            vVertexBuffers[0] = {
                reinterpret_cast<VulkanResource*>(pMtxMeshGpuResources->second.mesh.pVertexBuffer.get())
                    ->getInternalBufferResource()};
            vkCmdBindVertexBuffers(
                pCommandBuffer,
                0,
                static_cast<uint32_t>(vVertexBuffers.size()),
                vVertexBuffers.data(),
                vOffsets.data());

            // Bind index buffer.
            static_assert(sizeof(MeshData::meshindex_t) == sizeof(unsigned int), "change `indexTypeFormat`");
            vkCmdBindIndexBuffer(
                pCommandBuffer,
                reinterpret_cast<VulkanResource*>(pMtxMeshGpuResources->second.mesh.pIndexBuffer.get())
                    ->getInternalBufferResource(),
                0,
                indexTypeFormat);

            // Set CPU write shader resources.
            for (const auto& [sResourceName, pShaderCpuWriteResource] :
                 pMtxMeshGpuResources->second.shaderResources.shaderCpuWriteResources) {
                reinterpret_cast<GlslShaderCpuWriteResource*>(pShaderCpuWriteResource.getResource())
                    ->copyResourceIndexToPushConstants(pPushConstantsManager, iCurrentFrameResourceIndex);
            }

            // Queue a draw command.
            vkCmdDrawIndexed(
                pCommandBuffer, static_cast<uint32_t>(mtxMeshData.second->getIndices()->size()), 1, 0, 0, 0);
        }
    }

    std::optional<Error> VulkanRenderer::finishDrawingNextFrame(VulkanFrameResource* pCurrentFrameResource) {
        // Convert current frame resource.
        const auto pVulkanCurrentFrameResource =
            reinterpret_cast<VulkanFrameResource*>(pCurrentFrameResource);

        // Mark render pass end.
        vkCmdEndRenderPass(pVulkanCurrentFrameResource->pCommandBuffer);

        // Mark end of command recording.
        auto result = vkEndCommandBuffer(pVulkanCurrentFrameResource->pCommandBuffer);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
                "failed to finish recording commands into a command buffer, error: {}",
                string_VkResult(result)));
        }

        // Acquire an image from the swapchain.
        uint32_t iImageIndex = 0;
        result = vkAcquireNextImageKHR(
            pLogicalDevice,
            pSwapChain,
            UINT64_MAX,
            pVulkanCurrentFrameResource->pSemaphoreSwapChainImageAcquired,
            VK_NULL_HANDLE,
            &iImageIndex);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(
                fmt::format("failed to acquire next swap chain image, error: {}", string_VkResult(result)));
        }

        // Since the next acquired image might be not in the order we expect:
        // Make sure this image is not used by the GPU.
        result =
            vkWaitForFences(pLogicalDevice, 1, &vSwapChainImageFenceRefs[iImageIndex], VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(
                fmt::format("failed to wait for acquired image fence, error: {}", string_VkResult(result)));
        }

        // Mark the image as being used by this frame.
        vSwapChainImageFenceRefs[iImageIndex] = pVulkanCurrentFrameResource->pFence;

        // Prepare to submit command buffer for execution.
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // Specify semaphores to wait for before starting execution.
        const std::array<VkSemaphore, 1> semaphoresToWaitFor = {
            pVulkanCurrentFrameResource->pSemaphoreSwapChainImageAcquired};
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
            pVulkanCurrentFrameResource->pSemaphoreSwapChainImageDrawingFinished};
        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(vSemaphoresAfterCommandBufferFinished.size());
        submitInfo.pSignalSemaphores = vSemaphoresAfterCommandBufferFinished.data();

        // Make fence to be in "unsignaled" state.
        result = vkResetFences(pLogicalDevice, 1, &pVulkanCurrentFrameResource->pFence);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format("failed to reset a fence, error: {}", string_VkResult(result)));
        }

        // Submit command buffer(s) to the queue for execution.
        result = vkQueueSubmit(pGraphicsQueue, 1, &submitInfo, pVulkanCurrentFrameResource->pFence);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
                "failed to submit command buffer(s) for execution, error: {}", string_VkResult(result)));
        }

        // Prepare for presenting.
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = static_cast<uint32_t>(vSemaphoresAfterCommandBufferFinished.size());
        presentInfo.pWaitSemaphores = vSemaphoresAfterCommandBufferFinished.data();

        // Specify which swapchain/image to present.
        std::array<VkSwapchainKHR, 1> vSwapChains = {pSwapChain};
        presentInfo.swapchainCount = static_cast<uint32_t>(vSwapChains.size());
        presentInfo.pSwapchains = vSwapChains.data();
        presentInfo.pImageIndices = &iImageIndex;

        // No using multiple swapchains so leave `pResults` empty as we will get result for our single
        // swapchain from `vkQueuePresentKHR`.
        presentInfo.pResults = nullptr;

        // Present.
        result = vkQueuePresentKHR(pPresentQueue, &presentInfo);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(
                fmt::format("failed to present a swapchain image, error: {}", string_VkResult(result)));
        }

        return {};
    }

    std::optional<Error> VulkanRenderer::updateRenderBuffers() {
        throw std::runtime_error("not implemented");
    }

    void VulkanRenderer::waitForGpuToFinishUsingFrameResource(FrameResource* pFrameResource) {
        // Convert frame resource.
        const auto pVulkanFrameResource = reinterpret_cast<VulkanFrameResource*>(pFrameResource);

        // Wait for all fences to be signaled.
        const auto result =
            vkWaitForFences(pLogicalDevice, 1, &pVulkanFrameResource->pFence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) [[unlikely]] {
            Error error(fmt::format("failed to wait for a fence, error: {}", string_VkResult(result)));
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
            return Error(fmt::format(
                "failed to enumerate the number of available device extensions, error: {}",
                string_VkResult(result)));
        }

        // Get available device extensions.
        std::vector<VkExtensionProperties> vAvailableDeviceExtensions(iAvailableDeviceExtensionCount);
        result = vkEnumerateDeviceExtensionProperties(
            pGpuDevice, nullptr, &iAvailableDeviceExtensionCount, vAvailableDeviceExtensions.data());
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
                "failed to enumerate available device extensions, error: {}", string_VkResult(result)));
        }

        // Make sure all required device extensions are available on this GPU.
        for (const auto& pRequiredExtensionName : vUsedDeviceExtensionNames) {
            bool bFound = false;
            const auto sRequiredExtensionName = std::string(pRequiredExtensionName);

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
            return Error(fmt::format(
                "failed to enumerate available Vulkan instance validation layer count, error: {}",
                string_VkResult(result)));
        }

        // Get available Vulkan instance validation layers.
        std::vector<VkLayerProperties> vAvailableValidationLayers(iAvailableValidationLayerCount);
        result = vkEnumerateInstanceLayerProperties(
            &iAvailableValidationLayerCount, vAvailableValidationLayers.data());
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(fmt::format(
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
                return Error(fmt::format(
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

        Logger::get().error(fmt::format("unable to load \"vkDestroyDebugUtilsMessengerEXT\" function"));
    }
#endif
} // namespace ne
