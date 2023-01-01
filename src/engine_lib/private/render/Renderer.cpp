#include "render/Renderer.h"

// Custom.
#include "game/Game.h"
#include "io/ConfigManager.h"
#include "io/Logger.h"
#include "misc/ProjectPaths.h"
#include "materials/ShaderParameter.h"
#include "render/pso/PsoManager.h"

// External.
#include "fmt/core.h"

namespace ne {
    Renderer::Renderer(Game* pGame) {
        this->pGame = pGame;
        pShaderManager = std::make_unique<ShaderManager>(this);
        pPsoManager = std::make_unique<PsoManager>(this);

        // Log amount of shader variants per shader pack.
        Logger::get().info(
            fmt::format(
                "using {} shader(s) per pixel shader pack",
                ShaderParameterConfigurations::validPixelShaderParameterConfigurations.size()),
            getRendererLoggingCategory());
        Logger::get().info(
            fmt::format(
                "using {} shader(s) per vertex shader pack",
                ShaderParameterConfigurations::validVertexShaderParameterConfigurations.size()),
            getRendererLoggingCategory());
    }

    std::set<ShaderParameter>* Renderer::getShaderConfiguration(ShaderType shaderType) {
        switch (shaderType) {
        case (ShaderType::VERTEX_SHADER): {
            return &currentVertexShaderConfiguration;
            break;
        }
        case (ShaderType::PIXEL_SHADER): {
            return &currentPixelShaderConfiguration;
            break;
        }
        case (ShaderType::COMPUTE_SHADER): {
            Error error("unexpected case");
            error.showError();
            throw std::runtime_error(error.getError());
            break;
        }
        }

        Error error("unexpected case");
        error.showError();
        throw std::runtime_error(error.getError());
    }

    Window* Renderer::getWindow() const { return pGame->getWindow(); }

    Game* Renderer::getGame() const { return pGame; }

    ShaderManager* Renderer::getShaderManager() const { return pShaderManager.get(); }

    PsoManager* Renderer::getPsoManager() const { return pPsoManager.get(); }

    std::recursive_mutex* Renderer::getRenderResourcesMutex() { return &mtxRwRenderResources; }

    bool Renderer::isConfigurationFileExists() {
        const auto configPath = getRendererConfigurationFilePath();
        return std::filesystem::exists(configPath);
    }

    std::filesystem::path Renderer::getRendererConfigurationFilePath() {
        std::filesystem::path basePath = ProjectPaths::getDirectoryForEngineConfigurationFiles();

        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directories(basePath);
        }

        basePath /= sRendererConfigurationFileName;

        // Check extension.
        if (!std::string_view(sRendererConfigurationFileName)
                 .ends_with(ConfigManager::getConfigFormatExtension())) {
            basePath += ConfigManager::getConfigFormatExtension();
        }

        return basePath;
    }

    void Renderer::setShaderConfiguration(
        const std::set<ShaderParameter>& shaderConfiguration, ShaderType shaderType) {
        Logger::get().info(
            "renderer's current shader configuration was changed, flushing the command queue and recreating "
            "all PSOs' internal resources",
            sRendererLogCategory);

        {
            const auto psoGuard = pPsoManager->clearGraphicsPsosInternalResourcesAndDelayRestoring();

            // Change configuration.
            switch (shaderType) {
            case (ShaderType::VERTEX_SHADER): {
                currentVertexShaderConfiguration = shaderConfiguration;
                break;
            }
            case (ShaderType::PIXEL_SHADER): {
                currentPixelShaderConfiguration = shaderConfiguration;
                break;
            }
            case (ShaderType::COMPUTE_SHADER): {
                Error error("unexpected case");
                error.showError();
                throw std::runtime_error(error.getError());
                break;
            }
            }

            // Update shaders.
            pShaderManager->setConfigurationForShaders(shaderConfiguration, shaderType);
        }
    }

    const char* Renderer::getConfigurationSectionGpu() { return sConfigurationSectionGpu; }

    const char* Renderer::getConfigurationSectionResolution() { return sConfigurationSectionResolution; }

    const char* Renderer::getConfigurationSectionRefreshRate() { return sConfigurationSectionRefreshRate; }

    const char* Renderer::getConfigurationSectionAntialiasing() { return sConfigurationSectionAntialiasing; }

    const char* Renderer::getConfigurationSectionVSync() { return sConfigurationSectionVSync; }

    const char* Renderer::getConfigurationSectionTextureFiltering() {
        return sConfigurationSectionTextureFiltering;
    }

    const char* Renderer::getRendererLoggingCategory() { return sRendererLogCategory; }
} // namespace ne
