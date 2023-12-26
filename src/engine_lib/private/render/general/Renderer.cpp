#include "render/Renderer.h"

// Standard.
#include <array>
#include <format>

// Custom.
#include "game/GameManager.h"
#include "game/Window.h"
#include "io/Logger.h"
#include "shader/general/ShaderFilesystemPaths.hpp"
#include "misc/MessageBox.h"
#include "shader/general/ShaderMacro.h"
#include "render/vulkan/VulkanRenderer.h"
#include "misc/Profiler.hpp"
#include "game/nodes/EnvironmentNode.h"
#include "render/general/pipeline/PipelineManager.h"
#include "game/nodes/MeshNode.h"
#include "game/camera/CameraManager.h"
#include "game/camera/CameraProperties.h"
#include "shader/general/EngineShaders.hpp"
#include "game/nodes/light/PointLightNode.h"
#include "game/nodes/light/SpotlightNode.h"
#if defined(WIN32)
#pragma comment(lib, "Winmm.lib")
#include "render/directx/DirectXRenderer.h"
#elif __linux__
#include <unistd.h>
#include <time.h>
#endif

namespace ne {
    Renderer::Renderer(GameManager* pGameManager) {
        // There should be at least 2 swap chain images.
        static_assert(iRecommendedSwapChainBufferCount >= 2);

        // Make sure there are N swap chain images and N frame resources (frames in flight).
        // Frame resources expect that the number of swap chain images is equal to the number
        // of frame resources because frame resources store synchronization objects such as
        // fences and semaphores that expect one swap chain image per frame resource.
        static_assert(iRecommendedSwapChainBufferCount == FrameResourcesManager::getFrameResourcesCount());

        // Save game manager.
        this->pGameManager = pGameManager;

        // Initialize some objects.
        mtxSpawnedEnvironmentNode.second = nullptr;

        // Create some objects.
        pShaderManager = std::make_unique<ShaderManager>(this);
        pPipelineManager = std::make_unique<PipelineManager>(this);
        mtxShaderConfiguration.second = std::make_unique<ShaderConfiguration>(this);
    }

    std::optional<Error> Renderer::compileEngineShaders() const {
        // Determine renderer type.
        bool bIsHlsl = true;
        if (dynamic_cast<const VulkanRenderer*>(this) != nullptr) {
            bIsHlsl = false;
        }

        // Prepare shaders to compile.
        std::vector vEngineShaders = {
            EngineShaders::MeshNode::getVertexShader(bIsHlsl),
            EngineShaders::MeshNode::getFragmentShader(bIsHlsl),
            EngineShaders::ForwardPlus::getCalculateGridFrustumComputeShader(bIsHlsl),
            EngineShaders::ForwardPlus::getPrepareLightCullingComputeShader(bIsHlsl),
            EngineShaders::ForwardPlus::getLightCullingComputeShader(bIsHlsl),
        };

        // Prepare a promise/future to synchronously wait for the compilation to finish.
        auto pPromiseFinish = std::make_shared<std::promise<bool>>();
        auto future = pPromiseFinish->get_future();

        // Prepare callbacks.
        auto onProgress = [](size_t iCompiledShaderCount, size_t iTotalShadersToCompile) {};
        auto onError = [](ShaderDescription shaderDescription, std::variant<std::string, Error> error) {
            if (std::holds_alternative<std::string>(error)) {
                const auto sErrorMessage = std::format(
                    "failed to compile shader \"{}\" due to the following compilation error:\n{}",
                    shaderDescription.sShaderName,
                    std::get<std::string>(std::move(error)));
                const Error err(sErrorMessage);
                err.showError();
                throw std::runtime_error(err.getFullErrorMessage());
            }

            const auto sErrorMessage = std::format(
                "failed to compile shader \"{}\" due to the following internal error:\n{}",
                shaderDescription.sShaderName,
                std::get<Error>(std::move(error)).getFullErrorMessage());
            const Error err(sErrorMessage);
            err.showError();
            MessageBox::info(
                "Info",
                std::format(
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
            Logger::get().flushToDisk(); // flush to disk to see if we crashed while compiling
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
        Logger::get().info(std::format("took {:.1f} sec. to compile engine shaders", timeTookInSec));
        Logger::get().flushToDisk(); // flush to disk to see that we successfully compiled shaders

        return {};
    }

    void Renderer::updateFrameConstantsBuffer(
        FrameResource* pCurrentFrameResource, CameraProperties* pCameraProperties) {
        // Lock frame constants and environment.
        std::scoped_lock guard(mtxFrameConstants.first, mtxSpawnedEnvironmentNode.first);

        // Get camera's view matrix.
        const auto cameraViewMatrix = pCameraProperties->getViewMatrix();

        // Set camera properties.
        mtxFrameConstants.second.cameraPosition = glm::vec4(pCameraProperties->getWorldLocation(), 1.0F);
        mtxFrameConstants.second.viewMatrix = cameraViewMatrix;
        mtxFrameConstants.second.viewProjectionMatrix =
            pCameraProperties->getProjectionMatrix() * cameraViewMatrix;

        // Set time parameters.
        mtxFrameConstants.second.timeSincePrevFrameInSec = getGameManager()->getTimeSincePrevFrameInSec();
        mtxFrameConstants.second.totalTimeInSec = GameInstance::getTotalApplicationTimeInSec();

        // Copy to GPU.
        pCurrentFrameResource->pFrameConstantBuffer->copyDataToElement(
            0, &mtxFrameConstants.second, sizeof(mtxFrameConstants.second));
    }

    void Renderer::calculateFrameStatistics() {
        PROFILE_FUNC;

        using namespace std::chrono;

        // Save time spent on frustum culling.
        frameStats.timeSpentLastFrameOnFrustumCullingInMs = accumulatedTimeSpentLastFrameOnFrustumCullingInMs;
        accumulatedTimeSpentLastFrameOnFrustumCullingInMs = 0.0F; // reset accumulated time

        // Save culled object count.
        frameStats.iLastFrameCulledObjectCount = iLastFrameCulledObjectCount;
        iLastFrameCulledObjectCount = 0;

        // Save draw call count.
        frameStats.iLastFrameDrawCallCount = iLastFrameDrawCallCount;
        iLastFrameDrawCallCount = 0;

        // Get elapsed time.
        const auto iTimeSinceFpsUpdateInSec =
            duration_cast<seconds>(steady_clock::now() - frameStats.timeAtLastFpsUpdate).count();

        // Count the new present call.
        frameStats.iPresentCountSinceFpsUpdate += 1;

        // See if 1 second has passed.
        if (iTimeSinceFpsUpdateInSec >= 1) {
            // Save FPS.
            frameStats.iFramesPerSecond = frameStats.iPresentCountSinceFpsUpdate;

            // Reset present count.
            frameStats.iPresentCountSinceFpsUpdate = 0;

            // Restart time.
            frameStats.timeAtLastFpsUpdate = steady_clock::now();
        }

        // Check if FPS limit is set.
        if (frameStats.timeToRenderFrameInNs.has_value()) {
            // Get frame time.
            const auto frameTimeInNs =
                duration<double, nanoseconds::period>(steady_clock::now() - frameStats.frameStartTime)
                    .count();

            // Check if the last frame was rendered too fast.
            if (*frameStats.timeToRenderFrameInNs > frameTimeInNs) {
                // Calculate time to wait.
                const auto timeToWaitInNs = *frameStats.timeToRenderFrameInNs - frameTimeInNs;

#if defined(WIN32)
                timeBeginPeriod(1);
                nanosleep(static_cast<long long>(std::floor(timeToWaitInNs * 0.98))); // NOLINT
                timeEndPeriod(1);
#else
                struct timespec tim;
                struct timespec tim2;
                tim.tv_sec = 0;
                tim.tv_nsec = static_cast<long>(timeToWaitInNs);
                nanosleep(&tim, &tim2);
#endif
            }
        }

        // Update frame start/end time.
        frameStats.frameStartTime = steady_clock::now();
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

    void Renderer::resetLightingShaderResourceManager() {
        if (pLightingShaderResourceManager == nullptr) {
            return;
        }

        Logger::get().info("explicitly resetting lighting shader resource manager");
        Logger::get().flushToDisk();
        pLightingShaderResourceManager = nullptr;
    }

    void Renderer::onFramebufferSizeChanged(int iWidth, int iHeight) {
        if (iWidth == 0 && iHeight == 0) {
            // Don't draw anything as frame buffer size is zero.
            bIsWindowMinimized = true;
            waitForGpuToFinishWorkUpToThisPoint();
            return;
        }

        bIsWindowMinimized = false;

        onFramebufferSizeChangedDerived(iWidth, iHeight);
    }

    void Renderer::drawNextFrame() {
        PROFILE_FUNC;

        if (bIsWindowMinimized) {
            // Framebuffer size is zero, don't draw anything.
            return;
        }

        // Get pipeline manager and compute shaders to dispatch.
        const auto pPipelineManager = getPipelineManager();
        auto mtxQueuedComputeShader = pPipelineManager->getComputeShadersForGraphicsQueueExecution();

        // Get active camera.
        const auto pMtxActiveCamera = getGameManager()->getCameraManager()->getActiveCamera();

        // Get current frame resource.
        auto pMtxCurrentFrameResource = getFrameResourcesManager()->getCurrentFrameResource();

        // Lock mutexes together to minimize deadlocks.
        std::scoped_lock renderGuard(
            pMtxActiveCamera->first,
            *getRenderResourcesMutex(),
            pMtxCurrentFrameResource->first,
            *mtxQueuedComputeShader.first);

        // Get camera properties of the active camera.
        const auto pActiveCameraProperties = pMtxActiveCamera->second.getCameraProperties();
        if (pActiveCameraProperties == nullptr) {
            // No active camera.
            return;
        }

        // don't unlock active camera mutex until finished submitting the next frame for drawing

        // Prepare render target because we will need its size now.
        prepareRenderTargetForNextFrame();

        // Wait for the next frame resource to be no longer used by the GPU.
        const auto renderTargetSize = getRenderTargetSize();
        updateResourcesForNextFrame(renderTargetSize.first, renderTargetSize.second, pActiveCameraProperties);

        // Prepare for drawing a new frame.
        prepareForDrawingNextFrame(pActiveCameraProperties, pMtxCurrentFrameResource->second.pResource);

        // Get graphics pipelines.
        const auto pMtxGraphicsPipelines = pPipelineManager->getGraphicsPipelines();
        std::scoped_lock pipelinesGuard(pMtxGraphicsPipelines->first);

        // Cull meshes.
        const auto pMeshPipelinesInFrustum =
            getMeshesInCameraFrustum(pActiveCameraProperties, &pMtxGraphicsPipelines->second);

        // Draw depth prepass.
        drawMeshesDepthPrepass(
            pMtxCurrentFrameResource->second.pResource,
            pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex,
            pMeshPipelinesInFrustum->vOpaquePipelines);

        PROFILE_SCOPE_START(DispatchComputeShadersAfterDepthPrepass);

        // Run compute shaders after depth prepass.
        executeComputeShadersOnGraphicsQueue(
            pMtxCurrentFrameResource->second.pResource,
            pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex,
            ComputeExecutionStage::AFTER_DEPTH_PREPASS);

        PROFILE_SCOPE_END;

        // Draw main pass.
        drawMeshesMainPass(
            pMtxCurrentFrameResource->second.pResource,
            pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex,
            pMeshPipelinesInFrustum->vOpaquePipelines,
            pMeshPipelinesInFrustum->vTransparentPipelines);

        // Present the frame on the screen, flip swapchain images, etc.
        present(
            pMtxCurrentFrameResource->second.pResource,
            pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex);

        // Update frame stats.
        calculateFrameStatistics();

        // Switch to the next frame resource.
        getFrameResourcesManager()->switchToNextFrameResource();
    }

    std::optional<Error> Renderer::onRenderSettingsChanged(bool bShadowMapSizeChanged) {
        Logger::get().info(
            "waiting for GPU to finish work up to this point in order to apply changed render settings...");

        // Make sure no rendering is happening.
        std::scoped_lock guard(*getRenderResourcesMutex());
        waitForGpuToFinishWorkUpToThisPoint();

        // Update FPS cap.
        updateFpsLimitSetting();

        if (bShadowMapSizeChanged) {
            // Notify shadow map manager.
            auto optionalError = pResourceManager->getShadowMapManager()->recreateShadowMaps();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        // Call derived logic.
        auto optionalError = onRenderSettingsChangedDerived();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        Logger::get().info("successfully finished applying changed render settings");

        return {};
    }

    std::optional<Error> Renderer::recalculateLightTileFrustums() {
        // Get camera manager.
        const auto pCameraManager = getGameManager()->getCameraManager();
        if (pCameraManager == nullptr) {
            // No active camera, no need to notify lighting manager.
            return {};
        }

        // Get render settings and active camera.
        const auto pMtxActiveCamera = pCameraManager->getActiveCamera();

        // Lock camera.
        std::scoped_lock guard(pMtxActiveCamera->first);

        // Get camera properties of the active camera.
        const auto pActiveCameraProperties = pMtxActiveCamera->second.getCameraProperties();
        if (pActiveCameraProperties == nullptr) {
            // No active camera, no need to notify lighting manager.
            return {};
        }

        // Get inverse projection matrix.
        const glm::mat4 inverseProjectionMatrix =
            glm::inverse(pActiveCameraProperties->getProjectionMatrix());

        // Recalculate grid of frustums for light culling.
        auto optionalError = pLightingShaderResourceManager->recalculateLightTileFrustums(
            getRenderTargetSize(), inverseProjectionMatrix);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    void Renderer::onActiveCameraChanged() {
        // Recalculate grid of frustums for light culling
        // because projection matrix of the new camera might be different.
        auto optionalError = recalculateLightTileFrustums();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }
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
                    Logger::get().error(std::format(
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
                Logger::get().info(std::format(
                    "attempting to initialize {} renderer to test if the hardware/OS supports it...",
                    pRendererName));

                // Attempt to create a renderer.
                auto result = createRenderer(
                    rendererType, pGameManager, vBlacklistedGpuNames[static_cast<size_t>(rendererType)]);
                if (std::holds_alternative<std::pair<Error, std::string>>(result)) {
                    auto [error, sUsedGpuName] = std::get<std::pair<Error, std::string>>(std::move(result));

                    if (sUsedGpuName.empty()) {
                        // Log failure (not an error).
                        Logger::get().info(std::format(
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
                    Logger::get().info(std::format(
                        "failed to initialize {} renderer using the GPU \"{}\", error: {}",
                        pRendererName,
                        sUsedGpuName,
                        error.getFullErrorMessage()));

                    // Mark this GPU as blacklisted for this renderer.
                    vBlacklistedGpuNames[static_cast<size_t>(rendererType)].push_back(sUsedGpuName);
                    bLastGpuBlacklisted = true;
                    Logger::get().info(
                        std::format("blacklisting the GPU \"{}\" for this renderer", sUsedGpuName));

                    // Try the next renderer type, maybe it will be able to use this most suitable GPU
                    // (instead of switching to a less powerful GPU and trying to use it on this renderer).
                    continue;
                }
                auto pRenderer = std::get<std::unique_ptr<Renderer>>(std::move(result));

                // Log success.
                Logger::get().info(std::format(
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
            return std::pair<Error, std::string>{Error("DirectX renderer is not supported on this OS"), ""};
#endif
        }

        return VulkanRenderer::create(pGameManager, vBlacklistedGpuNames);
    }

    void Renderer::updateFpsLimitSetting() {
        // Get render setting.
        const auto pMtxRenderSettings = getRenderSettings();
        std::scoped_lock guard(pMtxRenderSettings->first);

        // Update time to render a frame.
        const auto iFpsLimit = pMtxRenderSettings->second->getFpsLimit();
        if (iFpsLimit == 0) {
            frameStats.timeToRenderFrameInNs = {};
        } else {
            frameStats.timeToRenderFrameInNs = 1000000000.0 / static_cast<double>(iFpsLimit); // NOLINT
        }
    }

    std::variant<std::unique_ptr<Renderer>, Error>
    Renderer::create(GameManager* pGameManager, std::optional<RendererType> preferredRenderer) {
        // Create a renderer.
        auto pCreatedRenderer = createRenderer(pGameManager, preferredRenderer);
        if (pCreatedRenderer == nullptr) [[unlikely]] {
            return Error(std::format(
                "unable to create a renderer because the hardware or the operating "
                "system does not meet the engine requirements, make sure your "
                "operating system and graphics drivers are updated and try again, "
                "you can find more information about the error in the most recent log file at \"{}\"",
                ProjectPaths::getPathToLogsDirectory().string()));
        }

        // Log amount of shader variants per shader pack.
        Logger::get().info(std::format(
            "using {} shader(s) per vertex shader pack",
            ShaderMacroConfigurations::validVertexShaderMacroConfigurations.size()));
        Logger::get().info(std::format(
            "using {} shader(s) per pixel shader pack",
            ShaderMacroConfigurations::validPixelShaderMacroConfigurations.size()));
        Logger::get().info(std::format(
            "using {} shader(s) per compute shader pack",
            ShaderMacroConfigurations::validComputeShaderMacroConfigurations.size()));

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

        // Notify lighting manager that we compiled compute shaders it needs.
        pCreatedRenderer->pLightingShaderResourceManager->onEngineShadersCompiled();
        optionalError = pCreatedRenderer->recalculateLightTileFrustums();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        // Setup frame statistics.
        pCreatedRenderer->setupFrameStats();

        return pCreatedRenderer;
    }

    size_t Renderer::getFramesPerSecond() const { return frameStats.iFramesPerSecond; }

    size_t Renderer::getLastFrameDrawCallCount() const { return frameStats.iLastFrameDrawCallCount; }

    float Renderer::getTimeSpentLastFrameWaitingForGpu() const {
        return frameStats.timeSpentLastFrameWaitingForGpuInMs;
    }

    float Renderer::getTimeSpentLastFrameOnFrustumCulling() const {
        return frameStats.timeSpentLastFrameOnFrustumCullingInMs;
    }

    size_t Renderer::getLastFrameCulledObjectCount() const { return frameStats.iLastFrameCulledObjectCount; }

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

    ShaderTextureResourceManager* Renderer::getShaderTextureResourceManager() const {
        return pShaderTextureResourceManager.get();
    }

    LightingShaderResourceManager* Renderer::getLightingShaderResourceManager() const {
        return pLightingShaderResourceManager.get();
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
                    mtxShaderConfiguration.second->currentPixelShaderConfiguration,
                    ShaderType::FRAGMENT_SHADER);
            }
        } else {
            std::scoped_lock shaderParametersGuard(mtxShaderConfiguration.first);

            // Update shaders.
            pShaderManager->setRendererConfigurationForShaders(
                mtxShaderConfiguration.second->currentVertexShaderConfiguration, ShaderType::VERTEX_SHADER);
            pShaderManager->setRendererConfigurationForShaders(
                mtxShaderConfiguration.second->currentPixelShaderConfiguration, ShaderType::FRAGMENT_SHADER);
        }
    }

    void Renderer::setupFrameStats() {
        frameStats.timeAtLastFpsUpdate = std::chrono::steady_clock::now();
        frameStats.frameStartTime = std::chrono::steady_clock::now();
    }

#if defined(WIN32)
    void Renderer::nanosleep(long long iNanoseconds) {
        iNanoseconds /= 100; // NOLINT: The time after which the state of the timer is to be set to signaled,
                             // in 100 nanosecond intervals.

        // Prepare some variables to use.
        HANDLE pTimer;
        LARGE_INTEGER interval;

        // Create timer.
        pTimer = CreateWaitableTimer(NULL, TRUE, NULL);
        if (pTimer == NULL) [[unlikely]] {
            Logger::get().error(std::format(
                "failed to wait create a waitable timer for {} nanoseconds (error code: {})",
                iNanoseconds,
                GetLastError()));
        }

        // Set timer.
        interval.QuadPart = -iNanoseconds;
        if (SetWaitableTimer(pTimer, &interval, 0, NULL, NULL, FALSE) == 0) [[unlikely]] {
            CloseHandle(pTimer);
            Logger::get().error(std::format(
                "failed to set a waitable timer for {} nanoseconds (error code: {})",
                iNanoseconds,
                GetLastError()));
        }

        // Wait for it to be signaled.
        WaitForSingleObject(pTimer, INFINITE);

        // Delete timer.
        CloseHandle(pTimer);
    }
#endif

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
                Logger::get().error(std::format(
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

        // Apply initial FPS limit setting.
        updateFpsLimitSetting();

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

        // Create shader texture resource manager.
        pShaderTextureResourceManager =
            std::unique_ptr<ShaderTextureResourceManager>(new ShaderTextureResourceManager(this));

        // Create lighting shader resource manager.
        pLightingShaderResourceManager = LightingShaderResourceManager::create(this);

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

        // Don't allow new frames to be submitted.
        std::scoped_lock frameGuard(*getRenderResourcesMutex());

        // Get current frame resource.
        auto pMtxCurrentFrameResource = getFrameResourcesManager()->getCurrentFrameResource();
        std::scoped_lock frameResource(pMtxCurrentFrameResource->first);

        {
            PROFILE_SCOPE(WaitForGpuToFinishUsingFrameResource);

            // Mark start time.
            const auto startTime = std::chrono::steady_clock::now();

            // Wait for this frame resource to no longer be used by the GPU.
            waitForGpuToFinishUsingFrameResource(pMtxCurrentFrameResource->second.pResource);

            // Measure the time it took to wait.
            const auto endTime = std::chrono::steady_clock::now();
            frameStats.timeSpentLastFrameWaitingForGpuInMs =
                std::chrono::duration<float, std::chrono::milliseconds::period>(endTime - startTime).count();
        }

        // Update camera's aspect ratio (if it was changed).
        pCameraProperties->setRenderTargetSize(iRenderTargetWidth, iRenderTargetHeight);

        {
            // See if camera's projection matrix was changed.
            std::scoped_lock cameraGuard(pCameraProperties->mtxData.first);

            if (pCameraProperties->mtxData.second.projectionData.bLightGridFrustumsNeedUpdate) {
                // Queue compute shader to recalculate frustums for light culling.
                auto optionalError = recalculateLightTileFrustums();
                if (optionalError.has_value()) [[unlikely]] {
                    optionalError->addCurrentLocationToErrorStack();
                    optionalError->showError();
                    throw std::runtime_error(optionalError->getFullErrorMessage());
                }

                // Mark as updated.
                pCameraProperties->mtxData.second.projectionData.bLightGridFrustumsNeedUpdate = false;
            }
        }

        // Copy new (up to date) data to frame data GPU resource to be used by shaders.
        updateFrameConstantsBuffer(pMtxCurrentFrameResource->second.pResource, pCameraProperties);

        // Update shader CPU write resources marked as "needs update".
        getShaderCpuWriteResourceManager()->updateResources(
            pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex);

        // Before updating lighting shader resources update general lighting parameters.
        {
            std::scoped_lock environmentGuard(mtxSpawnedEnvironmentNode.first);

            if (mtxSpawnedEnvironmentNode.second != nullptr) {
                pLightingShaderResourceManager->setAmbientLight(
                    mtxSpawnedEnvironmentNode.second->getAmbientLight());
            } else {
                pLightingShaderResourceManager->setAmbientLight(glm::vec3(0.0F, 0.0F, 0.0F));
            }
        }

        // Update lighting shader resources marked as "needs update".
        pLightingShaderResourceManager->updateResources(
            pMtxCurrentFrameResource->second.pResource,
            pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex);
    }

    Renderer::MeshesInFrustum* Renderer::getMeshesInCameraFrustum(
        CameraProperties* pActiveCameraProperties,
        PipelineManager::GraphicsPipelineRegistry* pGraphicsPipelines) {
        PROFILE_FUNC;

        // Record start time.
        using namespace std::chrono;
        const auto startFrustumCullingTime = steady_clock::now();

        // Clear information from the last frame.
        meshesInFrustumLastFrame.vOpaquePipelines.clear();
        meshesInFrustumLastFrame.vTransparentPipelines.clear();
#if defined(DEBUG) && defined(WIN32)
        static_assert(sizeof(MeshesInFrustum::PipelineInFrustumInfo) == 40, "clear new arrays"); // NOLINT
#elif defined(DEBUG)
        static_assert(sizeof(MeshesInFrustum::PipelineInFrustumInfo) == 32, "clear new arrays"); // NOLINT
#endif

        // Get camera frustum (camera should be updated at this point).
        const auto pCameraFrustum = pActiveCameraProperties->getCameraFrustum();

        // Prepare lambda to cull pipelines.
        const auto frustumCullPipelines =
            [this, pCameraFrustum](
                const std::unordered_map<std::string, PipelineManager::ShaderPipelines>& pipelinesToScan,
                std::vector<MeshesInFrustum::PipelineInFrustumInfo>& vPipelinesInFrustum) {
                // Iterate over all specified pipelines.
                for (const auto& [sShaderNames, pipelines] : pipelinesToScan) {
                    for (const auto& [macros, pPipeline] : pipelines.shaderPipelines) {
                        // Get materials.
                        const auto pMtxMaterials = pPipeline->getMaterialsThatUseThisPipeline();
                        std::scoped_lock materialsGuard(pMtxMaterials->first);

                        // Prepare pipeline info to fill.
                        MeshesInFrustum::PipelineInFrustumInfo pipelineInFrustumInfo;
                        pipelineInFrustumInfo.pPipeline = pPipeline.get();
                        pipelineInFrustumInfo.vMaterials.reserve(pMtxMaterials->second.size());

                        for (const auto& pMaterial : pMtxMaterials->second) {
                            // Get meshes.
                            const auto pMtxMeshNodes = pMaterial->getSpawnedMeshNodesThatUseThisMaterial();

                            // Prepare material info to fill.
                            MeshesInFrustum::MaterialInFrustumInfo materialInFrustumInfo;
                            materialInFrustumInfo.pMaterial = pMaterial;
                            materialInFrustumInfo.vMeshes.reserve(
                                pMtxMeshNodes->second.visibleMeshNodes.size());

                            // Iterate over all visible mesh nodes that use this material.
                            std::scoped_lock meshNodesGuard(pMtxMeshNodes->first);
                            for (const auto& [pMeshNode, vIndexBuffers] :
                                 pMtxMeshNodes->second.visibleMeshNodes) {
                                // Do frustum culling.
                                const auto pMtxMeshShaderConstants = pMeshNode->getMeshShaderConstants();
                                std::scoped_lock meshConstantsGuard(pMtxMeshShaderConstants->first);

                                // (camera's frustum should be updated at this point)
                                const auto bIsVisible = pCameraFrustum->isAabbInFrustum(
                                    *pMeshNode->getAABB(), pMtxMeshShaderConstants->second.world);

                                // Increment culled object count.
                                iLastFrameCulledObjectCount += static_cast<size_t>(!bIsVisible);

                                if (!bIsVisible) {
                                    // This mesh is outside of camera's frustum.
                                    continue;
                                }

                                // This mesh is inside frustum.
                                MeshesInFrustum::MeshInFrustumInfo meshInFrustumInfo;
                                meshInFrustumInfo.pMeshNode = pMeshNode;
                                meshInFrustumInfo.vIndexBuffers = vIndexBuffers;

                                materialInFrustumInfo.vMeshes.push_back(std::move(meshInFrustumInfo));
                            }

                            // Add to pipelines if some meshes in frustum.
                            if (!materialInFrustumInfo.vMeshes.empty()) {
                                // Add material info.
                                pipelineInFrustumInfo.vMaterials.push_back(std::move(materialInFrustumInfo));
                            }
                        }

                        // Add to pipelines if some materials in frustum.
                        if (!pipelineInFrustumInfo.vMaterials.empty()) {
                            vPipelinesInFrustum.push_back(std::move(pipelineInFrustumInfo));
                        }
                    }
                }
            };

        // Get pipelines to iterate over.
        const auto& opaquePipelines =
            pGraphicsPipelines->vPipelineTypes.at(static_cast<size_t>(PipelineType::PT_OPAQUE));
        const auto& transparentPipelines =
            pGraphicsPipelines->vPipelineTypes.at(static_cast<size_t>(PipelineType::PT_TRANSPARENT));

        // Attempt to minimize allocations in the code below.
        meshesInFrustumLastFrame.vOpaquePipelines.reserve(opaquePipelines.size());
        meshesInFrustumLastFrame.vTransparentPipelines.reserve(transparentPipelines.size());

        // Iterate only over opaque and transparent pipelines since opaque materials will reference two
        // pipelines at the same time (opaque pipeline and depth only pipeline) so don't iterate over depth
        // only pipelines to avoid doing frustum culling twice on the same meshes.
        frustumCullPipelines(opaquePipelines, meshesInFrustumLastFrame.vOpaquePipelines);
        frustumCullPipelines(transparentPipelines, meshesInFrustumLastFrame.vTransparentPipelines);

        // Increment total time spent in frustum culling.
        accumulatedTimeSpentLastFrameOnFrustumCullingInMs +=
            duration<float, milliseconds::period>(steady_clock::now() - startFrustumCullingTime).count();

        return &meshesInFrustumLastFrame;
    }

    void Renderer::cullLightsOutsideCameraFrustum(
        CameraProperties* pActiveCameraProperties, size_t iCurrentFrameResourceIndex) {
        // Get camera frustum.
        const auto pCameraFrustum = pActiveCameraProperties->getCameraFrustum();

        // Prepare a short reference to light arrays.
        auto& lightArrays = pLightingShaderResourceManager->lightArrays;

        {
            // Get lights in frustum.
            std::scoped_lock guardLights(lightArrays.pPointLightDataArray->mtxResources.first);

            // Create a short reference.
            auto& pointLightsInFrustum =
                lightArrays.pPointLightDataArray->mtxResources.second.lightsInFrustum;

            // Make sure there is at least one light.
            if (!pointLightsInFrustum.vShaderLightNodeArray.empty()) {
#if defined(DEBUG)
                // Make sure it indeed stores point lights.
                const auto pFirstNode = pointLightsInFrustum.vShaderLightNodeArray[0];
                if (dynamic_cast<PointLightNode*>(pFirstNode) == nullptr) [[unlikely]] {
                    Error error(std::format(
                        "expected an array of point lights, got node of different type with name \"{}\"",
                        pFirstNode->getNodeName()));
                    error.showError();
                    throw std::runtime_error(error.getFullErrorMessage());
                }
#endif

                // Clear indices to lights in frustum because we will rebuilt this array now.
                pointLightsInFrustum.vLightIndicesInFrustum.clear();

                for (size_t i = 0; i < pointLightsInFrustum.vShaderLightNodeArray.size(); i++) {
                    // Convert type.
                    const auto pPointLightNode =
                        reinterpret_cast<PointLightNode*>(pointLightsInFrustum.vShaderLightNodeArray[i]);

                    // Get light source shape.
                    const auto pMtxShape = pPointLightNode->getShape();
                    std::scoped_lock shapeGuard(pMtxShape->first);

                    // Make sure shape is is frustum.
                    if (!pCameraFrustum->isSphereInFrustum(pMtxShape->second)) {
                        continue;
                    }

                    // Add light index.
                    pointLightsInFrustum.vLightIndicesInFrustum.push_back(static_cast<unsigned int>(i));
                }

                // Notify array.
                lightArrays.pPointLightDataArray->onLightsInCameraFrustumCulled(iCurrentFrameResourceIndex);
            }
        }

        {
            // Get lights in frustum.
            std::scoped_lock guardLights(lightArrays.pSpotlightDataArray->mtxResources.first);

            // Create a short reference.
            auto& spotlightsInFrustum = lightArrays.pSpotlightDataArray->mtxResources.second.lightsInFrustum;

            // Make sure there is at least one light.
            if (!spotlightsInFrustum.vShaderLightNodeArray.empty()) {
#if defined(DEBUG)
                // Make sure it indeed stores spotlights.
                const auto pFirstNode = spotlightsInFrustum.vShaderLightNodeArray[0];
                if (dynamic_cast<SpotlightNode*>(pFirstNode) == nullptr) [[unlikely]] {
                    Error error(std::format(
                        "expected an array of spotlights, got node of different type with name \"{}\"",
                        pFirstNode->getNodeName()));
                    error.showError();
                    throw std::runtime_error(error.getFullErrorMessage());
                }
#endif

                // Clear indices to lights in frustum because we will rebuilt this array now.
                spotlightsInFrustum.vLightIndicesInFrustum.clear();

                for (size_t i = 0; i < spotlightsInFrustum.vShaderLightNodeArray.size(); i++) {
                    // Convert type.
                    const auto pSpotlightNode =
                        reinterpret_cast<SpotlightNode*>(spotlightsInFrustum.vShaderLightNodeArray[i]);

                    // Get light source shape.
                    const auto pMtxShape = pSpotlightNode->getShape();
                    std::scoped_lock shapeGuard(pMtxShape->first);

                    // Make sure shape is is frustum.
                    if (!pCameraFrustum->isConeInFrustum(pMtxShape->second)) {
                        continue;
                    }

                    // Add light index.
                    spotlightsInFrustum.vLightIndicesInFrustum.push_back(static_cast<unsigned int>(i));
                }

                // Notify array.
                lightArrays.pSpotlightDataArray->onLightsInCameraFrustumCulled(iCurrentFrameResourceIndex);
            }
        }
    }

} // namespace ne
