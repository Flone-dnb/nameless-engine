#include "IRenderer.h"

// Custom.
#include "io/ConfigManager.h"
#include "misc/Globals.h"

namespace ne {
    bool IRenderer::isConfigurationFileExists() {
        const auto configsFolder = getRendererConfigurationFilePath().parent_path();

        const auto directoryIterator = std::filesystem::directory_iterator(configsFolder);
        for (const auto &entry : directoryIterator) {
            if (entry.is_regular_file() && entry.path().stem().string() == sRendererConfigurationFileName) {
                return true;
            }
        }

        return false;
    }

    std::filesystem::path IRenderer::getRendererConfigurationFilePath() {
        std::filesystem::path basePath = getBaseDirectoryForConfigs();
        basePath += getApplicationName();

#if defined(WIN32)
        basePath += "\\";
#elif __linux__
        basePath += "/";
#endif

        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directory(basePath);
        }

        basePath += sEngineDirectoryName;

#if defined(WIN32)
        basePath += "\\";
#elif __linux__
        basePath += "/";
#endif

        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directory(basePath);
        }

        basePath += sRendererConfigurationFileName;

        // Check extension.
        if (!std::string_view(sRendererConfigurationFileName).ends_with(".ini")) {
            basePath += ".ini";
        }

        return basePath;
    }

    const char *IRenderer::getConfigurationSectionGpu() { return sConfigurationSectionGpu; }

    const char *IRenderer::getConfigurationSectionResolution() { return sConfigurationSectionResolution; }

    const char *IRenderer::getConfigurationSectionRefreshRate() { return sConfigurationSectionRefreshRate; }

    const char *IRenderer::getConfigurationSectionAntialiasing() { return sConfigurationSectionAntialiasing; }
} // namespace ne
