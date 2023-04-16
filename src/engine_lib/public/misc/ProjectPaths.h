#pragma once

// Standard.
#include <filesystem>

namespace ne {
    /** Describes various resource directories. */
    enum class ResourceDirectory {
        ROOT,   ///< Root directory for resources, contains other directories, such as game, engine, etc.
        GAME,   ///< Directory for game resources.
        ENGINE, ///< Directory for engine resources.
        EDITOR  ///< Directory for editor resources.
    };

    /** Provides static functions for acquiring paths to project log/save/resources/etc. directories. */
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
        static std::filesystem::path getPathToEngineConfigsDirectory();

        /**
         * Returns path to the directory that is used to store log files.
         *
         * @return Path to the directory.
         */
        static std::filesystem::path getPathToLogsDirectory();

        /**
         * Returns path to the directory that is used to store player's game progress.
         *
         * @return Path to the directory.
         */
        static std::filesystem::path getPathToPlayerProgressDirectory();

        /**
         * Returns path to the directory that is used to store player's game settings.
         *
         * @return Path to the directory.
         */
        static std::filesystem::path getPathToPlayerSettingsDirectory();

        /**
         * Returns path to the directory that is used to store compiled shaders.
         *
         * @return Path to the directory.
         */
        static std::filesystem::path getPathToCompiledShadersDirectory();

        /**
         * Returns path to the directory used to store resources.
         *
         * @remark Shows an error and throws an exception if path to the resources directory does not exist.
         *
         * @param directory Specific directory to query.
         *
         * @return Path to the directory.
         */
        static std::filesystem::path getPathToResDirectory(ResourceDirectory directory);

        /**
         * Returns base (root) directory used to store save and log files,
         * also creates this base directory if not exists.
         *
         * On Windows, returns something like this:
         * "C:\Users\user\AppData\Local\nameless-engine\"
         *
         * On Linux, returns something like this:
         * "~/.config/nameless-engine/"
         *
         * @return Path to base engine directory. Does not contain application name in the returned path.
         */
        static std::filesystem::path getPathToBaseConfigDirectory();

    private:
        /**
         * Returns path to the `res` directory that is located next to the executable.
         *
         * @remark Shows an error and throws an exception if path to the `res` directory does not exist.
         *
         * @return Path to the `res` directory.
         */
        static std::filesystem::path getPathToResDirectory();

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

        /** Name of the directory in which we store game resources. */
        static constexpr std::string_view sGameResourcesDirectoryName = "game";

        /** Name of the directory in which we store engine resources. */
        static constexpr std::string_view sEngineResourcesDirectoryName = "engine";

        /** Name of the directory in which we store editor resources. */
        static constexpr std::string_view sEditorResourcesDirectoryName = "editor";
    };
} // namespace ne
