﻿#include "render/Renderer.h"

// Standard.
#include <array>

// Custom.
#include "game/GameManager.h"
#include "game/Window.h"
#include "io/Logger.h"
#include "materials/ShaderFilesystemPaths.hpp"
#include "misc/MessageBox.h"
#include "materials/ShaderMacro.h"
#include "render/general/pso/PsoManager.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif
#include "render/vulkan/VulkanRenderer.h"

// External.
#include "fmt/core.h"

namespace ne {
    Renderer::Renderer(GameManager* pGameManager) {
        // There should be at least 2 swap chain images.
        static_assert(iSwapChainBufferCount >= 2);

        // Make sure there are N swap chain images and N frame resources (frames in flight).
        static_assert(iSwapChainBufferCount == FrameResourcesManager::getFrameResourcesCount());

        this->pGameManager = pGameManager;

        // Create some objects.
        pShaderManager = std::make_unique<ShaderManager>(this);
        pPsoManager = std::make_unique<PsoManager>(this);
        mtxShaderConfiguration.second = std::make_unique<ShaderConfiguration>(this);
    }

    std::optional<Error> Renderer::compileEngineShaders() const {
        // Prepare shaders to compile.
        const auto vEngineShaders = getEngineShadersToCompile();

        // Prepare a promise/future to synchronously wait for the compilation to finish.
        auto pPromiseFinish = std::make_shared<std::promise<bool>>();
        auto future = pPromiseFinish->get_future();

        // Prepare callbacks.
        auto onProgress = [](size_t iCompiledShaderCount, size_t iTotalShadersToCompile) {};
        auto onError = [](ShaderDescription shaderDescription, std::variant<std::string, Error> error) {
            if (std::holds_alternative<std::string>(error)) {
                const auto sErrorMessage = std::format(
                    "failed to compile shader \"{}\" due to compilation error:\n{}",
                    shaderDescription.sShaderName,
                    std::get<std::string>(std::move(error)));
                const Error err(sErrorMessage);
                err.showError();
                throw std::runtime_error(err.getFullErrorMessage());
            }

            const auto sErrorMessage = std::format(
                "failed to compile shader \"{}\" due to internal error:\n{}",
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
            error->addEntry();
            error->showError();
            throw std::runtime_error(error->getFullErrorMessage());
        }

        // Wait synchronously (before user adds his shaders).
        try {
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

    std::unique_ptr<Renderer>
    Renderer::createRenderer(GameManager* pGameManager, std::optional<RendererType> preferredRenderer) {
        // Describe default renderer preference.
        std::array<RendererType, 2> vRendererPreferenceQueue = {RendererType::DIRECTX, RendererType::VULKAN};

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
                    error.addEntry();
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
        for (const auto& rendererType : vRendererPreferenceQueue) {
            const auto pRendererName = rendererType == RendererType::DIRECTX ? "DirectX" : "Vulkan";

            // Log test.
            Logger::get().info(fmt::format(
                "attempting to initialize {} renderer to test if the hardware/OS supports it...",
                pRendererName));

            // Attempt to create a renderer.
            auto result = createRenderer(rendererType, pGameManager);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));

                // Log failure (not an error).
                Logger::get().info(fmt::format(
                    "failed to initialize {} renderer, error: {}",
                    pRendererName,
                    error.getFullErrorMessage()));
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

        return nullptr;
    }

    std::variant<std::unique_ptr<Renderer>, Error>
    Renderer::createRenderer(RendererType type, GameManager* pGameManager) {
        if (type == RendererType::DIRECTX) {
#if defined(WIN32)
            return DirectXRenderer::create(pGameManager);
#else
            return Error("DirectX renderer is not supported on this OS");
#endif
        }

        return VulkanRenderer::create(pGameManager);
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
                optionalError->addEntry();
                return optionalError.value();
            }
        }

        // Update shader cache (clears if the old cache is no longer valid).
        auto optionalError = pCreatedRenderer->getShaderManager()->refreshShaderCache();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addEntry();
            return optionalError.value();
        }

        // Compile/verify engine shaders.
        optionalError = pCreatedRenderer->compileEngineShaders();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addEntry();
            return optionalError.value();
        }

        return pCreatedRenderer;
    }

    std::pair<std::recursive_mutex, std::shared_ptr<RenderSettings>>* Renderer::getRenderSettings() {
        return &mtxRenderSettings;
    }

    std::pair<std::recursive_mutex, std::unique_ptr<ShaderConfiguration>>*
    Renderer::getShaderConfiguration() {
        return &mtxShaderConfiguration;
    }

    Window* Renderer::getWindow() const { return pGameManager->getWindow(); }

    GameManager* Renderer::getGameManager() const { return pGameManager; }

    ShaderManager* Renderer::getShaderManager() const { return pShaderManager.get(); }

    PsoManager* Renderer::getPsoManager() const { return pPsoManager.get(); }

    GpuResourceManager* Renderer::getResourceManager() const { return pResourceManager.get(); }

    FrameResourcesManager* Renderer::getFrameResourcesManager() const { return pFrameResourcesManager.get(); }

    ShaderCpuReadWriteResourceManager* Renderer::getShaderCpuReadWriteResourceManager() const {
        return pShaderCpuReadWriteResourceManager.get();
    }

    std::recursive_mutex* Renderer::getRenderResourcesMutex() { return &mtxRwRenderResources; }

    void Renderer::updateShaderConfiguration() {
        if (isInitialized()) {
            const auto psoGuard = pPsoManager->clearGraphicsPsosInternalResourcesAndDelayRestoring();

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
                error.addEntry();
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
        mtxRenderSettings.second->updateRendererConfiguration();

        return {};
    }

    std::optional<Error> Renderer::initializeRenderer() {
        auto optionalError = initializeRenderSettings();
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> Renderer::initializeResourceManagers() {
        // Create GPU resource manager.
        auto gpuResourceManagerResult = GpuResourceManager::create(this);
        if (std::holds_alternative<Error>(gpuResourceManagerResult)) {
            auto error = std::get<Error>(std::move(gpuResourceManagerResult));
            error.addEntry();
            return error;
        }
        pResourceManager = std::get<std::unique_ptr<GpuResourceManager>>(std::move(gpuResourceManagerResult));

        // Create frame resources manager.
        auto frameResourceManagerResult = FrameResourcesManager::create(this);
        if (std::holds_alternative<Error>(gpuResourceManagerResult)) {
            auto error = std::get<Error>(std::move(gpuResourceManagerResult));
            error.addEntry();
            return error;
        }
        pFrameResourcesManager =
            std::get<std::unique_ptr<FrameResourcesManager>>(std::move(frameResourceManagerResult));

        // Create shader read/write resource manager.
        pShaderCpuReadWriteResourceManager =
            std::unique_ptr<ShaderCpuReadWriteResourceManager>(new ShaderCpuReadWriteResourceManager(this));

        return {};
    }

} // namespace ne
