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
     * Directory name that we append to the result of the base directory.
     */
    constexpr std::string_view sSavesDirectoryName = "saves";

    /**
     * Directory name that we append to the result of the base directory.
     */
    constexpr std::string_view sConfigsDirectoryName = "configs";

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
    std::filesystem::path getBaseDirectory();

    /**
     * Returns the name of this application.
     *
     * @return Name of this application.
     */
    std::string getApplicationName();
} // namespace ne
