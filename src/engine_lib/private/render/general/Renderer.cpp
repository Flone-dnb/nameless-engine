#include "render/Renderer.h"

// Standard.
#include <array>

// Custom.
#include "game/GameManager.h"
#include "game/Window.h"
#include "io/Logger.h"
#include "materials/ShaderFilesystemPaths.hpp"
#include "misc/MessageBox.h"
#include "materials/ShaderMacro.h"
#include "render/general/pipeline/PipelineManager.h"
#include "render/vulkan/VulkanRenderer.h"
#include "game/camera/CameraProperties.h"
#include "misc/Profiler.hpp"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif

// External.
#include "fmt/core.h"

namespace ne {
    Renderer::Renderer(GameManager* pGameManager) {
        // There should be at least 2 swap chain images.
        static_assert(iSwapChainBufferCount >= 2);

        // Make sure there are N swap chain images and N frame resources (frames in flight).
        // Frame resources expect that the number of swap chain images is equal to the number
        // of frame resources because frame resources store synchronization objects such as
        // fences and semaphores that expect one swap chain image per frame resource.
        static_assert(iSwapChainBufferCount == FrameResourcesManager::getFrameResourcesCount());

        this->pGameManager = pGameManager;

        // Create some objects.
        pShaderManager = std::make_unique<ShaderManager>(this);
        pPipelineManager = std::make_unique<PipelineManager>(this);
        mtxShaderConfiguration.second = std::make_unique<ShaderConfiguration>(this);
    }

    std::optional<Error> Renderer::compileEngineShaders() const {
        // Prepare shaders to compile.
        auto vEngineShaders = getEngineShadersToCompile();

        // Prepare a promise/future to synchronously wait for the compilation to finish.
        auto pPromiseFinish = std::make_shared<std::promise<bool>>();
        auto future = pPromiseFinish->get_future();

        // Prepare callbacks.
        auto onProgress = [](size_t iCompiledShaderCount, size_t iTotalShadersToCompile) {};
        auto onError = [](ShaderDescription shaderDescription, std::variant<std::string, Error> error) {
            if (std::holds_alternative<std::string>(error)) {
                const auto sErrorMessage = fmt::format(
                    "failed to compile shader \"{}\" due to the following compilation error:\n{}",
                    shaderDescription.sShaderName,
                    std::get<std::string>(std::move(error)));
                const Error err(sErrorMessage);
                err.showError();
                throw std::runtime_error(err.getFullErrorMessage());
            }

            const auto sErrorMessage = fmt::format(
                "failed to compile shader \"{}\" due to the following internal error:\n{}",
                shaderDescription.sShaderName,
                std::get<Error>(std::move(error)).getFullErrorMessage());
            const Error err(sErrorMessage);
            err.showError();
            MessageBox::info(
                "Info",
                fmt::format(
                    "Try restarting the application or deleting the directory \"{}\", if this "
                    "does not help contact the developers.",
                    ShaderFilesystemPaths::getPathToShaderCacheDirectory().string()));
            throw std::runtime_error(err.getFullErrorMessage());
        };
        auto onCompleted = [pPromiseFinish]() { pPromiseFinish->set_value(false); };

        // Mark start time.
        const auto startTime = std::chrono::steady_clock::now();

        // Compile shaders.
        auto error =
            getShaderManager()->compileShaders(std::move(vEngineShaders), onProgress, onError, onCompleted);
        if (error.has_value()) {
            error->addCurrentLocationToErrorStack();
            error->showError();
            throw std::runtime_error(error->getFullErrorMessage());
        }

        // Wait synchronously (before user adds his shaders).
        try {
            Logger::get().info("waiting for engine shaders to be compiled...");
            Logger::get().flushToDisk();
            future.get();
        } catch (const std::exception& ex) {
            const Error err(ex.what());
            err.showError();
            throw std::runtime_error(err.getInitialMessage());
        }

        // Mark end time.
        const auto endTime = std::chrono::steady_clock::now();

        // Calculate duration.
        const auto timeTookInSec =
            std::chrono::duration<float, std::chrono::seconds::period>(endTime - startTime).count();

        // Log time.
        Logger::get().info(fmt::format("took {:.1f} sec. to compile engine shaders", timeTookInSec));

        return {};
    }

    void Renderer::updateFrameConstantsBuffer(
        FrameResource* pCurrentFrameResource, CameraProperties* pCameraProperties) {
        std::scoped_lock guard(mtxFrameConstants.first);

        // Set camera properties.
        mtxFrameConstants.second.cameraPosition = pCameraProperties->getWorldLocation();
        mtxFrameConstants.second.viewProjectionMatrix =
            pCameraProperties->getProjectionMatrix() * pCameraProperties->getViewMatrix();

        // Set time parameters.
        mtxFrameConstants.second.timeSincePrevFrameInSec = getGameManager()->getTimeSincePrevFrameInSec();
        mtxFrameConstants.second.totalTimeInSec = GameInstance::getTotalApplicationTimeInSec();

        // Copy to GPU.
        pCurrentFrameResource->pFrameConstantBuffer->copyDataToElement(
            0, &mtxFrameConstants.second, sizeof(mtxFrameConstants.second));
    }

    void Renderer::resetGpuResourceManager() {
        if (pResourceManager == nullptr) {
            return;
        }

        Logger::get().info("explicitly resetting GPU resource manager");
        Logger::get().flushToDisk();
        pResourceManager = nullptr;
    }

    void Renderer::resetPipelineManager() {
        if (pPipelineManager == nullptr) {
            return;
        }

        Logger::get().info("explicitly resetting pipeline manager");
        Logger::get().flushToDisk();
        pPipelineManager = nullptr;
    }

    void Renderer::resetFrameResourcesManager() {
        if (pFrameResourcesManager == nullptr) {
            return;
        }

        Logger::get().info("explicitly resetting frame resources manager");
        Logger::get().flushToDisk();
        pFrameResourcesManager = nullptr;
    }

    std::unique_ptr<Renderer>
    Renderer::createRenderer(GameManager* pGameManager, std::optional<RendererType> preferredRenderer) {
        // Describe default renderer preference.
        constexpr size_t iRendererTypeCount = 2;
        std::array<RendererType, iRendererTypeCount> vRendererPreferenceQueue = {
            RendererType::DIRECTX, RendererType::VULKAN};

        if (!preferredRenderer.has_value()) {
            // See if config file has a special preference.
            // Construct path to config file.
            const auto pathToConfigFile = ProjectPaths::getPathToEngineConfigsDirectory() /
                                          RenderSettings::getConfigurationFileName(true);

            // See if config file exists.
            if (std::filesystem::exists(pathToConfigFile)) {
                // Try to deserialize.
                auto result = Serializable::deserialize<std::shared_ptr, RenderSettings>(pathToConfigFile);
                if (std::holds_alternative<Error>(result)) {
                    auto error = std::get<Error>(std::move(result));
                    error.addCurrentLocationToErrorStack();
                    Logger::get().error(fmt::format(
                        "failed to deserialize render settings from the file \"{}\", using default "
                        "settings instead, error: \"{}\"",
                        pathToConfigFile.string(),
                        error.getFullErrorMessage()));
                } else {
                    // Use the deserialized object.
                    const auto pSettings = std::get<std::shared_ptr<RenderSettings>>(std::move(result));

                    if (pSettings->iRendererType != static_cast<unsigned int>(RendererType::DIRECTX)) {
                        // Change preference queue.
                        vRendererPreferenceQueue = {RendererType::VULKAN, RendererType::DIRECTX};
                    }
                }
            }
        } else if (preferredRenderer.value() == RendererType::VULKAN) {
            // Change preference queue.
            vRendererPreferenceQueue = {RendererType::VULKAN, RendererType::DIRECTX};
        }

        // Create renderer using preference queue.
        std::array<std::vector<std::string>, iRendererTypeCount> vBlacklistedGpuNames;
        bool bLastGpuBlacklisted = false;
        do {
            for (const auto& rendererType : vRendererPreferenceQueue) {
                // Prepare some variables.
                const auto pRendererName = rendererType == RendererType::DIRECTX ? "DirectX" : "Vulkan";
                bLastGpuBlacklisted = false;

                // Log test.
                Logger::get().info(fmt::format(
                    "attempting to initialize {} renderer to test if the hardware/OS supports it...",
                    pRendererName));

                // Attempt to create a renderer.
                auto result = createRenderer(
                    rendererType, pGameManager, vBlacklistedGpuNames[static_cast<size_t>(rendererType)]);
                if (std::holds_alternative<std::pair<Error, std::string>>(result)) {
                    auto [error, sUsedGpuName] = std::get<std::pair<Error, std::string>>(std::move(result));

                    if (sUsedGpuName.empty()) {
                        // Log failure (not an error).
                        Logger::get().info(fmt::format(
                            "failed to initialize {} renderer, error: {}",
                            pRendererName,
                            error.getFullErrorMessage()));

                        // Try the next renderer.
                        Logger::get().info(
                            "either no information about used GPU is available or all supported GPUs are "
                            "blacklisted, attempting to use another renderer");
                        continue;
                    }

                    // Log failure (not an error).
                    Logger::get().info(fmt::format(
                        "failed to initialize {} renderer using the GPU \"{}\", error: {}",
                        pRendererName,
                        sUsedGpuName,
                        error.getFullErrorMessage()));

                    // Mark this GPU as blacklisted for this renderer.
                    vBlacklistedGpuNames[static_cast<size_t>(rendererType)].push_back(sUsedGpuName);
                    bLastGpuBlacklisted = true;
                    Logger::get().info(
                        fmt::format("blacklisting the GPU \"{}\" for this renderer", sUsedGpuName));

                    // Try the next renderer type, maybe it will be able to use this most suitable GPU
                    // (instead of switching to a less powerful GPU and trying to use it on this renderer).
                    continue;
                }
                auto pRenderer = std::get<std::unique_ptr<Renderer>>(std::move(result));

                // Log success.
                Logger::get().info(fmt::format(
                    "successfully initialized {} renderer, using {} renderer (used API version: {})",
                    pRendererName,
                    pRendererName,
                    pRenderer->getUsedApiVersion()));

                return pRenderer;
            }
        } while (bLastGpuBlacklisted);

        return nullptr;
    }

    std::variant<std::unique_ptr<Renderer>, std::pair<Error, std::string>> Renderer::createRenderer(
        RendererType type, GameManager* pGameManager, const std::vector<std::string>& vBlacklistedGpuNames) {
        if (type == RendererType::DIRECTX) {
#if defined(WIN32)
            return DirectXRenderer::create(pGameManager, vBlacklistedGpuNames);
#else
            return Error("DirectX renderer is not supported on this OS");
#endif
        }

        return VulkanRenderer::create(pGameManager, vBlacklistedGpuNames);
    }

    std::variant<std::unique_ptr<Renderer>, Error>
    Renderer::create(GameManager* pGameManager, std::optional<RendererType> preferredRenderer) {
        // Create a renderer.
        auto pCreatedRenderer = createRenderer(pGameManager, preferredRenderer);
        if (pCreatedRenderer == nullptr) [[unlikely]] {
            return Error(fmt::format(
                "unable to create a renderer because the hardware or the operating "
                "system does not meet the engine requirements, make sure your "
                "operating system and graphics drivers are updated and try again, "
                "you can find more information about the error in the most recent log file at \"{}\"",
                ProjectPaths::getPathToLogsDirectory().string()));
        }

        // Log amount of shader variants per shader pack.
        Logger::get().info(fmt::format(
            "using {} shader(s) per vertex shader pack",
            ShaderMacroConfigurations::validVertexShaderMacroConfigurations.size()));
        Logger::get().info(fmt::format(
            "using {} shader(s) per pixel shader pack",
            ShaderMacroConfigurations::validPixelShaderMacroConfigurations.size()));

        // Update render settings (maybe they were fixed/clamped during the renderer initialization).
        const auto pMtxRenderSettings = pCreatedRenderer->getRenderSettings();
        {
            std::scoped_lock guard(pMtxRenderSettings->first);

            // Set picked renderer type.
            pMtxRenderSettings->second->iRendererType =
                static_cast<unsigned int>(pCreatedRenderer->getType());

            // Enable saving configuration on disk.
            pMtxRenderSettings->second->bAllowSavingConfigurationToDisk = true;

            // Save settings.
            auto optionalError = pMtxRenderSettings->second->saveConfigurationToDisk();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }
        }

        // Update shader cache (clears if the old cache is no longer valid).
        auto optionalError = pCreatedRenderer->getShaderManager()->refreshShaderCache();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        // Compile/verify engine shaders.
        optionalError = pCreatedRenderer->compileEngineShaders();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return pCreatedRenderer;
    }

    std::pair<std::recursive_mutex, std::shared_ptr<RenderSettings>>* Renderer::getRenderSettings() {
        return &mtxRenderSettings;
    }

    size_t Renderer::getTotalVideoMemoryInMb() const {
        return getResourceManager()->getTotalVideoMemoryInMb();
    }

    size_t Renderer::getUsedVideoMemoryInMb() const { return getResourceManager()->getUsedVideoMemoryInMb(); }

    std::pair<std::recursive_mutex, std::unique_ptr<ShaderConfiguration>>*
    Renderer::getShaderConfiguration() {
        return &mtxShaderConfiguration;
    }

    Window* Renderer::getWindow() const { return pGameManager->getWindow(); }

    GameManager* Renderer::getGameManager() const { return pGameManager; }

    ShaderManager* Renderer::getShaderManager() const { return pShaderManager.get(); }

    PipelineManager* Renderer::getPipelineManager() const { return pPipelineManager.get(); }

    GpuResourceManager* Renderer::getResourceManager() const { return pResourceManager.get(); }

    FrameResourcesManager* Renderer::getFrameResourcesManager() const { return pFrameResourcesManager.get(); }

    ShaderCpuWriteResourceManager* Renderer::getShaderCpuWriteResourceManager() const {
        return pShaderCpuWriteResourceManager.get();
    }

    std::recursive_mutex* Renderer::getRenderResourcesMutex() { return &mtxRwRenderResources; }

    void Renderer::updateShaderConfiguration() {
        if (isInitialized()) {
            const auto pipelineGuard =
                pPipelineManager->clearGraphicsPipelinesInternalResourcesAndDelayRestoring();

            {
                std::scoped_lock shaderParametersGuard(mtxShaderConfiguration.first);

                // Update shaders.
                pShaderManager->setRendererConfigurationForShaders(
                    mtxShaderConfiguration.second->currentVertexShaderConfiguration,
                    ShaderType::VERTEX_SHADER);
                pShaderManager->setRendererConfigurationForShaders(
                    mtxShaderConfiguration.second->currentPixelShaderConfiguration, ShaderType::PIXEL_SHADER);
            }
        } else {
            std::scoped_lock shaderParametersGuard(mtxShaderConfiguration.first);

            // Update shaders.
            pShaderManager->setRendererConfigurationForShaders(
                mtxShaderConfiguration.second->currentVertexShaderConfiguration, ShaderType::VERTEX_SHADER);
            pShaderManager->setRendererConfigurationForShaders(
                mtxShaderConfiguration.second->currentPixelShaderConfiguration, ShaderType::PIXEL_SHADER);
        }
    }

    std::optional<Error> Renderer::initializeRenderSettings() {
        // Construct path to config file.
        const auto pathToConfigFile =
            ProjectPaths::getPathToEngineConfigsDirectory() / RenderSettings::getConfigurationFileName(true);

        // See if config file exists.
        if (std::filesystem::exists(pathToConfigFile)) {
            // Try to deserialize.
            auto result = Serializable::deserialize<std::shared_ptr, RenderSettings>(pathToConfigFile);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                Logger::get().error(fmt::format(
                    "failed to deserialize render settings from the file \"{}\", using default "
                    "settings instead, error: \"{}\"",
                    pathToConfigFile.string(),
                    error.getFullErrorMessage()));

                // Just create a new object with default settings.
                mtxRenderSettings.second = std::make_shared<RenderSettings>();
            } else {
                // Use the deserialized object.
                mtxRenderSettings.second = std::get<std::shared_ptr<RenderSettings>>(std::move(result));
            }
        } else {
            // Just create a new object with default settings.
            mtxRenderSettings.second = std::make_shared<RenderSettings>();
        }

        // Initialize the setting.
        mtxRenderSettings.second->setRenderer(this);

        // Apply the configuration.
        mtxRenderSettings.second->notifyRendererAboutChangedSettings();

        return {};
    }

    std::optional<Error> Renderer::initializeRenderer() {
        auto optionalError = initializeRenderSettings();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> Renderer::initializeResourceManagers() {
        // Create GPU resource manager.
        auto gpuResourceManagerResult = GpuResourceManager::create(this);
        if (std::holds_alternative<Error>(gpuResourceManagerResult)) {
            auto error = std::get<Error>(std::move(gpuResourceManagerResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        pResourceManager = std::get<std::unique_ptr<GpuResourceManager>>(std::move(gpuResourceManagerResult));

        // Create frame resources manager.
        auto frameResourceManagerResult = FrameResourcesManager::create(this);
        if (std::holds_alternative<Error>(gpuResourceManagerResult)) {
            auto error = std::get<Error>(std::move(gpuResourceManagerResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        pFrameResourcesManager =
            std::get<std::unique_ptr<FrameResourcesManager>>(std::move(frameResourceManagerResult));

        // Create shader CPU write resource manager.
        pShaderCpuWriteResourceManager =
            std::unique_ptr<ShaderCpuWriteResourceManager>(new ShaderCpuWriteResourceManager(this));

        return {};
    }

    std::optional<Error> Renderer::clampSettingsToMaxSupported() {
        std::scoped_lock guard(mtxRenderSettings.first);

        auto optionalError = mtxRenderSettings.second->clampSettingsToMaxSupported();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    void Renderer::updateResourcesForNextFrame(
        unsigned int iRenderTargetWidth,
        unsigned int iRenderTargetHeight,
        CameraProperties* pCameraProperties) {
        PROFILE_FUNC;

        std::scoped_lock frameGuard(*getRenderResourcesMutex());

        // Set camera's aspect ratio.
        pCameraProperties->setAspectRatio(iRenderTargetWidth, iRenderTargetHeight);

        // Get current frame resource.
        auto pMtxCurrentFrameResource = getFrameResourcesManager()->getCurrentFrameResource();
        std::scoped_lock frameResource(pMtxCurrentFrameResource->first);

        {
            PROFILE_SCOPE(WaitForGpuToFinishUsingFrameResource);

            // Wait for this frame resource to no longer be used by the GPU.
            waitForGpuToFinishUsingFrameResource(pMtxCurrentFrameResource->second.pResource);
        }

        // Copy new (up to date) data to frame data cbuffer to be used by the shaders.
        updateFrameConstantsBuffer(pMtxCurrentFrameResource->second.pResource, pCameraProperties);

        // Update shader CPU write resources marked as "needs update".
        getShaderCpuWriteResourceManager()->updateResources(
            pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex);
    }

} // namespace ne
