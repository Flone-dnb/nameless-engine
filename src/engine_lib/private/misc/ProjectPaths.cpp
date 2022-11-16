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
        case ResourceDirectory::GAME: {
            path /= sGameResourcesDirectoryName;
            break;
        }
        case ResourceDirectory::ENGINE: {
            path /= sEngineResourcesDirectoryName;
            break;
        }
        case ResourceDirectory::EDITOR: {
            path /= sEditorResourcesDirectoryName;
            break;
        }
        };

        if (!std::filesystem::exists(path)) {
            std::filesystem::create_directory(path);
        }

        return path;
    }
} // namespace ne
