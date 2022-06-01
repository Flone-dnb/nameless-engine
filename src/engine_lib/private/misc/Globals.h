#pragma once

// Std.
#include <filesystem>
#include <string>

// C.
#include <cstdlib>

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
     * Does not contain application name in path.
     * Use @ref getApplicationName to append it yourself.
     *
     * On Windows, returns something like this:
     * "C:\Users\user\AppData\Local\nameless-engine\"
     *
     * On Linux, returns something like this:
     * "~/.local/share/nameless-engine/"
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

    /**
     * Converts wstring to its narrow multibyte representation.
     *
     * @param sText String to convert.
     *
     * @return Converted string.
     */
    std::string wstringToString(const std::wstring& sText);
} // namespace ne
