#include "IRenderer.h"

// Custom.
#include "io/ConfigManager.h"

namespace ne {
    bool IRenderer::isConfigurationFileExists() {
        const auto vEngineConfigFiles = ConfigManager::getAllConfigFiles(ConfigCategory::ENGINE);
        bool bExists = false;

        for (const auto &sConfigFileName : vEngineConfigFiles) {
            if (sConfigFileName == getRendererConfigurationFileName()) {
                bExists = true;
                break;
            }
        }

        return bExists;
    }

    const char *IRenderer::getRendererConfigurationFileName() { return sRendererConfigurationFileName; }

    const char *IRenderer::getConfigurationSectionGpu() { return sConfigurationSectionGpu; }

    const char *IRenderer::getConfigurationSectionResolution() { return sConfigurationSectionResolution; }

    const char *IRenderer::getConfigurationSectionRefreshRate() { return sConfigurationSectionRefreshRate; }

    const char *IRenderer::getConfigurationSectionAntialiasing() { return sConfigurationSectionAntialiasing; }
} // namespace ne
