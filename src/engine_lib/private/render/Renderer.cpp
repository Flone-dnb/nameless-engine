#include "render/Renderer.h"

// Custom.
#include "game/Game.h"
#include "game/Window.h"
#include "io/Logger.h"
#include "materials/ShaderParameter.h"
#include "render/pso/PsoManager.h"

// External.
#include "fmt/core.h"

namespace ne {
    Renderer::Renderer(Game* pGame) {
        this->pGame = pGame;

        pShaderManager = std::make_unique<ShaderManager>(this);
        pPsoManager = std::make_unique<PsoManager>(this);
        mtxRenderSettings.second = std::unique_ptr<RenderSettings>(new RenderSettings(this));
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

    std::pair<std::recursive_mutex, std::unique_ptr<RenderSettings>>* Renderer::getRenderSettings() {
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

    const char* Renderer::getRenderConfigurationDirectoryName() {
        return sRendererConfigurationDirectoryName;
    }

    void Renderer::initializeRenderSettings() { mtxRenderSettings.second->initialize(); }

} // namespace ne
