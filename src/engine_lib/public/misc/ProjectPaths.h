#pragma once

// STL.
#include <filesystem>

namespace ne {
    /** Provides static functions for acquiring paths to project log/save/etc. directories. */
    class ProjectPaths {
    public:
        ProjectPaths() = delete;
        ProjectPaths(const ProjectPaths&) = delete;
        ProjectPaths& operator=(const ProjectPaths&) = delete;

        /**
         * Returns path to the directory that is used to store engine configuration files (settings files),
         * such as renderer configuration, shader manager configuration and etc.
         *
         * @return Path to the directory that is used to store engine configuration files.
         */
        static std::filesystem::path getDirectoryForEngineConfigurationFiles();

        /**
         * Returns path to the directory that is used to store log files.
         *
         * @return Path to the directory that is used to store log files.
         */
        static std::filesystem::path getDirectoryForLogFiles();

        /**
         * Returns path to the directory that is used to store player's game progress.
         *
         * @return Path to the directory that is used to store player's game progress.
         */
        static std::filesystem::path getDirectoryForPlayerProgress();

        /**
         * Returns path to the directory that is used to store player's game settings.
         *
         * @return Path to the directory that is used to store player's game settings.
         */
        static std::filesystem::path getDirectoryForPlayerSettings();

        /**
         * Returns path to the directory that is used to store compiled shaders.
         *
         * @return Path to the directory that is used to store compiled shaders.
         */
        static std::filesystem::path getDirectoryForCompiledShaders();

    private:
        /** Name of the directory in which we store engine logs. */
        static constexpr std::string_view sLogsDirectoryName = "logs";

        /** Directory name that is used to store player's progress. */
        static constexpr std::string_view sProgressDirectoryName = "progress";

        /** Directory name that is used to store player's settings. */
        static constexpr std::string_view sSettingsDirectoryName = "settings";

        /** Name of the directory in which we store engine configuration files. */
        static constexpr std::string_view sEngineDirectoryName = "engine";

        /** Name of the directory in which we store compiled shaders. */
        static constexpr std::string_view sShaderCacheDirectoryName = "shader_cache";
    };
} // namespace ne
