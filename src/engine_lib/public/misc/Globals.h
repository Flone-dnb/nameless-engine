#pragma once

// Standard.
#include <filesystem>
#include <string>

// Custom.
#include "math/GLMath.hpp"

namespace ne {
    /**
     * Returns the name of this application.
     *
     * @return Name of this application.
     */
    std::string getApplicationName();

    /**
     * Returns path to the `res` directory that is located next to the executable.
     *
     * @remark Shows an error and throws an exception if path to the `res` directory does not exist.
     *
     * @return Path to the `res` directory.
     */
    std::filesystem::path getPathToResDirectory();

    /**
     * Converts wstring to its narrow multibyte representation.
     *
     * @param sText String to convert.
     *
     * @return Converted string.
     */
    std::string wstringToString(const std::wstring& sText);

    /**
     * Converts string to wstring.
     *
     * @param sText String to convert.
     *
     * @return Converted string.
     */
    std::wstring stringToWstring(const std::string& sText);

    /**
     * Returns base directory used to store save and log files,
     * also creates this base directory if not exists.
     *
     * Does not contain application name in path.
     * Use @ref getApplicationName to append it yourself.
     *
     * On Windows, returns something like this:
     * "C:\Users\user\AppData\Local\nameless-engine\"
     *
     * On Linux, returns something like this:
     * "~/.config/nameless-engine/"
     *
     * @return Path to base engine directory.
     */
    std::filesystem::path getBaseDirectoryForConfigs();

    /** Vector that points in world's up direction. */
    static inline const glm::vec3 worldUpDirection = glm::vec3(0.0F, 0.0F, 1.0F);

    /** Vector that points in world's right direction. */
    static inline const glm::vec3 worldRightDirection = glm::vec3(0.0F, 1.0F, 0.0F);

    /** Vector that points in world's forward direction. */
    static inline const glm::vec3 worldForwardDirection = glm::vec3(1.0F, 0.0F, 0.0F);

    /** Name of the root (base) engine directory. */
    constexpr std::string_view sBaseEngineDirectoryName = "nameless-engine";

    /** Name of the root (base) engine directory. */
    constexpr auto sDebugOnlyLoggingSubCategory = "Debug mode only";

    /** Name of the `res` directory. */
    constexpr auto sResDirectoryName = "res";
} // namespace ne
