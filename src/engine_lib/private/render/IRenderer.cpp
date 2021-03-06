#include "IRenderer.h"

// Custom.
#include "game/Game.h"
#include "io/ConfigManager.h"
#include "io/Logger.h"
#include "misc/Globals.h"
#include "shaders/ShaderParameter.h"

namespace ne {
    IRenderer::IRenderer(Game* pGame) {
        this->pGame = pGame;
        pShaderManager = std::make_unique<ShaderManager>(this);

        // Log amount of shader variants per shader pack.
        Logger::get().info(
            std::format(
                "using {} shader(s) per pixel shader pack",
                ShaderParameterConfigurations::validPixelShaderParameterConfigurations.size()),
            getRendererLoggingCategory());
        Logger::get().info(
            std::format(
                "using {} shader(s) per vertex shader pack",
                ShaderParameterConfigurations::validVertexShaderParameterConfigurations.size()),
            getRendererLoggingCategory());
    }

    Window* IRenderer::getWindow() const { return pGame->getWindow(); }

    Game* IRenderer::getGame() const { return pGame; }

    ShaderManager* IRenderer::getShaderManager() const { return pShaderManager.get(); }

    bool IRenderer::isConfigurationFileExists() {
        const auto configPath = getRendererConfigurationFilePath();
        return std::filesystem::exists(configPath);
    }

    std::filesystem::path IRenderer::getRendererConfigurationFilePath() {
        std::filesystem::path basePath = getBaseDirectoryForConfigs();
        basePath += getApplicationName();

        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directory(basePath);
        }

        basePath /= sEngineDirectoryName;

        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directory(basePath);
        }

        basePath /= sRendererConfigurationFileName;

        // Check extension.
        if (!std::string_view(sRendererConfigurationFileName)
                 .ends_with(ConfigManager::getConfigFormatExtension())) {
            basePath += ConfigManager::getConfigFormatExtension();
        }

        return basePath;
    }

    const char* IRenderer::getConfigurationSectionGpu() { return sConfigurationSectionGpu; }

    const char* IRenderer::getConfigurationSectionResolution() { return sConfigurationSectionResolution; }

    const char* IRenderer::getConfigurationSectionRefreshRate() { return sConfigurationSectionRefreshRate; }

    const char* IRenderer::getConfigurationSectionAntialiasing() { return sConfigurationSectionAntialiasing; }

    const char* IRenderer::getConfigurationSectionVSync() { return sConfigurationSectionVSync; }

    const char* IRenderer::getConfigurationSectionTextureFiltering() {
        return sConfigurationSectionTextureFiltering;
    }

    const char* IRenderer::getRendererLoggingCategory() { return sRendererLogCategory; }
} // namespace ne
