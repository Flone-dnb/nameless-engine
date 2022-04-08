#pragma once

// Std.
#include <filesystem>
#include <string>

namespace ne {
    /**
     * Directory name that we append to the result of the base directory.
     */
    constexpr std::string_view sBaseEngineDirectoryName = "nameless-engine";

    /**
     * Directory name that we append to the result of the base directory.
     */
    constexpr std::string_view sLogsDirectoryName = "logs";

    /**
     * Directory name that is used to store player's progress.
     */
    constexpr std::string_view sProgressDirectoryName = "progress";

    /**
     * Directory name that is used to store player's settings.
     */
    constexpr std::string_view sSettingsDirectoryName = "settings";

    /**
     * Directory name that we append to the result of the base directory.
     */
    constexpr std::string_view sEngineDirectoryName = "engine";

    /**
     * Returns base directory used to store save and log files,
     * also creates a base directory if not exists.
     *
     * @return Path to base directory that ends with a platform specific slash.
     */
    std::filesystem::path getBaseDirectoryForConfigs();

    /**
     * Returns the name of this application.
     *
     * @return Name of this application.
     */
    std::string getApplicationName();
} // namespace ne
