#pragma once

// Standard.
#include <filesystem>

namespace ne {
    /**
     * Describes different resource directories.
     */
    enum class ResourceDirectory {
        ROOT,   ///< Root directory for resources, contains other directories, such as game, engine, etc.
        GAME,   ///< Directory for game resources, all game resources are stored there.
        ENGINE, ///< Directory for engine resources, all engine resources are stored there.
        EDITOR  ///< Directory for editor resources, all editor resources are stored there.
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

        /**
         * Returns path to the directory used to store resources.
         *
         * @remark Shows an error and throws an exception if path to the `res` directory does not exist.
         *
         * @param directory Specific directory to query.
         *
         * @return Path to the directory for resources, the directory will be created if not
         * existed before.
         */
        static std::filesystem::path getDirectoryForResources(ResourceDirectory directory);

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

        /** Name of the directory in which we store game resources. */
        static constexpr std::string_view sGameResourcesDirectoryName = "game";

        /** Name of the directory in which we store engine resources. */
        static constexpr std::string_view sEngineResourcesDirectoryName = "engine";

        /** Name of the directory in which we store editor resources. */
        static constexpr std::string_view sEditorResourcesDirectoryName = "editor";
    };
} // namespace ne
