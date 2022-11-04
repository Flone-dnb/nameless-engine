#include "misc/ProjectPaths.h"

// Custom.
#include "misc/Globals.h"

namespace ne {
    std::filesystem::path ProjectPaths::getDirectoryForEngineConfigurationFiles() {
        return getBaseDirectoryForConfigs() / getApplicationName() / sEngineDirectoryName;
    }

    std::filesystem::path ProjectPaths::getDirectoryForLogFiles() {
        return getBaseDirectoryForConfigs() / getApplicationName() / sLogsDirectoryName;
    }

    std::filesystem::path ProjectPaths::getDirectoryForPlayerProgress() {
        return getBaseDirectoryForConfigs() / getApplicationName() / sProgressDirectoryName;
    }

    std::filesystem::path ProjectPaths::getDirectoryForPlayerSettings() {
        return getBaseDirectoryForConfigs() / getApplicationName() / sSettingsDirectoryName;
    }

    std::filesystem::path ProjectPaths::getDirectoryForCompiledShaders() {
        return getBaseDirectoryForConfigs() / getApplicationName() / sShaderCacheDirectoryName;
    }

    std::filesystem::path ProjectPaths::getDirectoryForResources(ResourceDirectory directory) {
        std::filesystem::path path = ne::getPathToResDirectory();

        switch (directory) {
        case ResourceDirectory::ROOT:
            return path;
        case ResourceDirectory::GAME:
            path /= sGameResourcesDirectoryName;
        case ResourceDirectory::ENGINE:
            path /= sEngineResourcesDirectoryName;
        case ResourceDirectory::EDITOR:
            path /= sEditorResourcesDirectoryName;
        };

        if (!std::filesystem::exists(path)) {
            std::filesystem::create_directory(path);
        }

        return path;
    }
} // namespace ne
