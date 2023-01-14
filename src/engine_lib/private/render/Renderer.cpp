#include "render/Renderer.h"

// Custom.
#include "game/Game.h"
#include "game/Window.h"
#include "io/Logger.h"
#include "materials/ShaderParameter.h"
#include "render/pso/PsoManager.h"
#include "render/RenderSettings.h"
#include "render/resources/GpuResourceManager.h"
#if defined(WIN32)
#include "render/directx/resources/DirectXResourceManager.h"
#include "render/directx/DirectXRenderer.h"
#endif

// External.
#include "fmt/core.h"

namespace ne {
    Renderer::Renderer(Game* pGame) {
        this->pGame = pGame;

        pShaderManager = std::make_unique<ShaderManager>(this);
        pPsoManager = std::make_unique<PsoManager>(this);
        mtxShaderConfiguration.second = std::make_unique<ShaderConfiguration>(this);

        // Log amount of shader variants per shader pack.
        Logger::get().info(
            fmt::format(
                "using {} shader(s) per pixel shader pack",
                ShaderParameterConfigurations::validPixelShaderParameterConfigurations.size()),
            sRendererLogCategory);
        Logger::get().info(
            fmt::format(
                "using {} shader(s) per vertex shader pack",
                ShaderParameterConfigurations::validVertexShaderParameterConfigurations.size()),
            sRendererLogCategory);
    }

    std::unique_ptr<Renderer> Renderer::create(Game* pGame) {
#if defined(WIN32)
        return std::make_unique<DirectXRenderer>(pGame);
        // TODO: vulkan
#elif __linux__
        // TODO: vulkan
        static_assert(false, "not implemented");
#else
        static_assert(false, "not implemented");
#endif
    }

    std::pair<std::recursive_mutex, std::shared_ptr<RenderSettings>>* Renderer::getRenderSettings() {
        return &mtxRenderSettings;
    }

    std::pair<std::recursive_mutex, std::unique_ptr<ShaderConfiguration>>*
    Renderer::getShaderConfiguration() {
        return &mtxShaderConfiguration;
    }

    Window* Renderer::getWindow() const { return pGame->getWindow(); }

    Game* Renderer::getGame() const { return pGame; }

    ShaderManager* Renderer::getShaderManager() const { return pShaderManager.get(); }

    PsoManager* Renderer::getPsoManager() const { return pPsoManager.get(); }

    GpuResourceManager* Renderer::getResourceManager() const { return pResourceManager.get(); }

    std::recursive_mutex* Renderer::getRenderResourcesMutex() { return &mtxRwRenderResources; }

    void Renderer::updateShaderConfiguration() {
        if (isInitialized()) {
            const auto psoGuard = pPsoManager->clearGraphicsPsosInternalResourcesAndDelayRestoring();

            {
                std::scoped_lock shaderParametersGuard(mtxShaderConfiguration.first);

                // Update shaders.
                pShaderManager->setConfigurationForShaders(
                    mtxShaderConfiguration.second->currentVertexShaderConfiguration,
                    ShaderType::VERTEX_SHADER);
                pShaderManager->setConfigurationForShaders(
                    mtxShaderConfiguration.second->currentPixelShaderConfiguration, ShaderType::PIXEL_SHADER);
            }
        } else {
            std::scoped_lock shaderParametersGuard(mtxShaderConfiguration.first);

            // Update shaders.
            pShaderManager->setConfigurationForShaders(
                mtxShaderConfiguration.second->currentVertexShaderConfiguration, ShaderType::VERTEX_SHADER);
            pShaderManager->setConfigurationForShaders(
                mtxShaderConfiguration.second->currentPixelShaderConfiguration, ShaderType::PIXEL_SHADER);
        }
    }

    void Renderer::initializeRenderSettings() {
        // Construct path to config file.
        const auto pathToConfigFile = ProjectPaths::getDirectoryForEngineConfigurationFiles() /
                                      RenderSettings::getConfigurationFileName(true);

        bool bDeserializedWithoutIssues = false;

        // See if config file exists.
        if (std::filesystem::exists(pathToConfigFile)) {
            // Try to deserialize.
            auto result = Serializable::deserialize<std::shared_ptr, RenderSettings>(pathToConfigFile);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addEntry();
                Logger::get().error(
                    fmt::format(
                        "failed to deserialize render settings from the file \"{}\", using default "
                        "settings instead, error: \"{}\"",
                        pathToConfigFile.string(),
                        error.getError()),
                    sRendererLogCategory);

                // Just create a new object with default settings.
                mtxRenderSettings.second = std::make_shared<RenderSettings>();
            } else {
                // Use the deserialized object.
                mtxRenderSettings.second = std::get<std::shared_ptr<RenderSettings>>(std::move(result));
                bDeserializedWithoutIssues = true;
            }
        } else {
            // Just create a new object with default settings.
            mtxRenderSettings.second = std::make_shared<RenderSettings>();
        }

        // Initialize the setting.
        mtxRenderSettings.second->setRenderer(this);

        // Save if no config existed.
        if (!bDeserializedWithoutIssues) {
            auto optionalError = mtxRenderSettings.second->saveConfigurationToDisk();
            if (optionalError.has_value()) {
                auto error = optionalError.value();
                error.addEntry();
                Logger::get().error(
                    fmt::format("failed to save new render settings, error: \"{}\"", error.getError()),
                    sRendererLogCategory);
            }
        }

        // Apply the configuration.
        mtxRenderSettings.second->updateRendererConfiguration();
    }

    void Renderer::initializeRenderer() { initializeRenderSettings(); }

    void Renderer::initializeResourceManager() {
        // Create GPU resource manager.
        auto result = GpuResourceManager::create(this);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getError());
        }
        pResourceManager = std::get<std::unique_ptr<GpuResourceManager>>(std::move(result));
    }

} // namespace ne
