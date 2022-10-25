#include "Renderer.h"

// Custom.
#include "game/Game.h"
#include "io/ConfigManager.h"
#include "io/Logger.h"
#include "misc/ProjectPaths.h"
#include "shaders/ShaderParameter.h"

// External.
#include "fmt/core.h"

namespace ne {
    Renderer::Renderer(Game* pGame) {
        this->pGame = pGame;
        pShaderManager = std::make_unique<ShaderManager>(this);

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

    Window* Renderer::getWindow() const { return pGame->getWindow(); }

    Game* Renderer::getGame() const { return pGame; }

    ShaderManager* Renderer::getShaderManager() const { return pShaderManager.get(); }

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
