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
} // namespace ne