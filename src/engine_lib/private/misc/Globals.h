#pragma once

// Std.
#include <filesystem>
#include <string>

namespace ne {
    /**
     * Directory name that we append to the result of the base directory.
     */
    constexpr std::string_view sEngineDirectoryName = "nameless-engine";

    /**
     * Returns base directory used to store save and log files.
     *
     * @return Path to base directory.
     */
    std::filesystem::path getBaseDirectory();

    /**
     * Returns the name of this application.
     *
     * @return Name of this application.
     */
    std::string getApplicationName();
} // namespace ne
