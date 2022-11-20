#pragma once

// Standard.
#include <variant>
#include <optional>
#include <filesystem>

// Custom.
#include "misc/Error.h"

// External.
#define TOML11_PRESERVE_COMMENTS_BY_DEFAULT
#include "toml11/toml.hpp"

namespace ne {
    /**
     * Describes different directories in which we can store configuration files.
     */
    enum class ConfigCategory {
        PROGRESS, // used to store player's game progress (uses backup files under the hood)
        SETTINGS  // used to store player's game specific settings (no backup files)
    };

    /**
     * Allows saving and loading configuration in key-value style.
     */
    class ConfigManager {
    public:
        /**
         * Constructs an empty configuration, use @ref loadFile to read configuration from a file
         * or setValue methods and then @ref saveFile to save a new configuration.
         */
        ConfigManager() = default;

        ConfigManager(const ConfigManager&) = delete;
        ConfigManager& operator=(const ConfigManager&) = delete;

        /**
         * Returns file format extension used to store config files.
         *
         * @return File extension that starts with a dot, for example: ".toml".
         */
        static std::string getConfigFormatExtension();

        /**
         * Returns file extension used to store backup files, for example: ".old".
         *
         * @return File extension used to store backup files.
         */
        static std::string getBackupFileExtension();

        /**
         * Returns file names (without extension) that this category (directory)
         * contains.
         *
         * How backup files are handled:
         * Imagine you had a file 'player.toml' and a backup file ('player.toml.old').
         * If, for some reason, 'player.toml' (the original file) does not exist,
         * but its backup file is there, we will copy the backup file (player.toml.old)
         * as the original file (will copy 'player.toml.old' as 'player.toml') and
         * return 'player' as an available config file.
         *
         * @param category Category (directory) in which to look for files.
         *
         * @return All files in the specified category (backup files are excluded).
         */
        static std::vector<std::string> getAllFiles(ConfigCategory category);

        /**
         * Returns path to the directory used to store specific category of files.
         * Path will be created if not existed before.
         *
         * @param category Category for which to return path.
         *
         * @return Path to the directory of the specified category.
         */
        static std::filesystem::path getCategoryDirectory(ConfigCategory category);

        /**
         * Removes a file.
         *
         * @remark This function will also remove the backup file of this file (if exists).
         *
         * @param category    Directory in which we will search for this file.
         * Use PROGRESS category to search for player's game progress and SETTINGS to search for player's
         * settings.
         * @param sFileName   Name of the file to remove. We will search it in a predefined directory
         * that we also use in @ref saveFile (use @ref getFilePath to see full path),
         * the .toml extension will be added if the passed name does not have it.
         *
         * @return Error if something went wrong. No error will be returned if the file does not exist.
         */
        static std::optional<Error> removeFile(ConfigCategory category, std::string_view sFileName);

        /**
         * Removes a file.
         *
         * @remark This function will also remove the backup file of this file (if exists).
         *
         * @param pathToConfigFile Path to the file to remove.
         * The .toml extension will be added if the passed path does not have it.
         */
        static void removeFile(std::filesystem::path pathToConfigFile);

        /**
         * Loads data from TOML file.
         * File should exist, otherwise an error will be returned (you can use @ref getAllFiles
         * or @ref getCategoryDirectory to see if files exist).
         * If you used @ref saveFile before and enabled a backup file (see @ref saveFile),
         * if usual file does not exist this function will look for a backup file and if found,
         * will copy this backup file with a name of the usual file.
         *
         * @param category    Directory in which we will store this file.
         * Use PROGRESS category to save player's game progress and SETTINGS to store player's settings.
         * @param sFileName   Name of the file to load. We will load it from a predefined directory
         * that we also use in @ref saveFile (use @ref getFilePath to see full path),
         * the .toml extension will be added if the passed name does not have it.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> loadFile(ConfigCategory category, std::string_view sFileName);

        /**
         * Loads data from file.
         *
         * Prefer to use the other overload (@ref loadFile) that uses a category instead of a path.
         *
         * If you used @ref saveFile before with PROGRESS category,
         * if the usual (original) file does not exist this function
         * will look for a backup file and if found,
         * will copy this backup file with a name of the usual (original) file
         * (so this function will restore the original file if it was deleted).
         *
         * @param pathToConfigFile  Path to the file to load (should exist).
         * The .toml extension will be added if the passed path does not have it.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> loadFile(std::filesystem::path pathToConfigFile);

        /**
         * Reads a value from the loaded file (see @ref loadFile).
         *
         * Possible value types:
         * - integer types (underlying type for integer is std::int64_t),
         * - float/double,
         * - bool,
         * - strings,
         * - date/time,
         * - array containers (vector, list, deque, etc.),
         * - map containers (std::unordered_map<std::string, T>, etc.) (key should be `std::string`).
         *
         * In order to use custom user types include @ref ConfigManager
         * and see our ShaderDescription class or:
         * https://github.com/ToruNiina/toml11#conversion-between-toml-value-and-arbitrary-types
         *
         * Example:
         * @code
         * // sample.toml:
         * // pi      = 3.14
         * // numbers = [1,2,3]
         * // time    = 1979-05-27T07:32:00Z
         * // [server]
         * // port    = 12312
         * using std::chrono::system_clock;
         * double pi = manager.getValue<double>("", "pi", 0.0);
         * std::vector<int> = manager.getValue<std::vector<int>>("", "numbers", std::vector<int>{});
         * system_clock::time_point time = manager.getValue<time_point>("", "time", system_clock::now());
         * int port = manager.getValue<int>("server", "port", 0);
         * @endcode
         *
         * @param sSection       Name of the section (can be empty if the key has no section).
         * @param sKey           Name of the key.
         * @param defaultValue   Value that will be returned if the specified key was not found.
         *
         *
         * @return Default value if the specified section/key was not found,
         * otherwise value from file.
         */
        template <typename T>
        T getValue(std::string_view sSection, std::string_view sKey, T defaultValue) const;

        /**
         * Returns names of all sections.
         *
         * @return Names of all sections.
         */
        std::vector<std::string> getAllSections();

        /**
         * Returns all keys of the specified section.
         *
         * @param sSection Name of the section to look for keys, use @ref getAllSections to get
         * names of all sections.
         *
         * @return Error if something went wrong, a vector of keys otherwise.
         */
        std::variant<std::vector<std::string>, Error> getAllKeysOfSection(std::string_view sSection) const;

        /**
         * Sets a value. This value will not be written to file until @ref saveFile is called.
         *
         * If the specified key was already set before (in the specified section),
         * this call will overwrite it with the new value (that can have a different type).
         *
         * Different sections can have keys with the same name (uniqueness per section).
         * Example:
         * @code
         * // sample.toml:
         * [section1]
         * test = test1
         * [section2]
         * test = test2
         * @endcode
         *
         * Possible value types:
         * - integer types (underlying type for integer is std::int64_t),
         * - float/double,
         * - bool,
         * - strings,
         * - date/time,
         * - array containers (vector, list, deque, etc.),
         * - map containers (unordered_map, etc.).
         *
         * In order to use custom user types include @ref ConfigManager
         * and see our ShaderDescription class or:
         * https://github.com/ToruNiina/toml11#conversion-between-toml-value-and-arbitrary-types
         *
         * Example:
         * @code
         * // in order to create this sample.toml:
         * // pi      = 3.14
         * // numbers = [1,2,3]
         * // time    = 1979-05-27T07:32:00Z
         * // [server]
         * // port    = 12312
         * using std::chrono::system_clock;
         * manager.setValue<double>("", "pi", 3.14);
         * manager.setValue<std::vector<int>>("", "numbers", std::vector<int>{1, 2, 3});
         * manager.setValue<time_point>("", "time", system_clock::now());
         * manager.setValue<int>("server", "port", 12312);
         * @endcode
         *
         * @param sSection   Name of the section (can be empty if the key has no section).
         * @param sKey       Name of the key.
         * @param value      Value to set.
         * @param sComment   Comment to add to this value.
         */
        template <typename T>
        void
        setValue(std::string_view sSection, std::string_view sKey, T value, std::string_view sComment = "");

        /**
         * Saves the current configuration to a file with a UTF-8 encoding.
         *
         * @warning There is no need to save render settings as
         * some parts of the engine save their own configs, for example, renderer will save last
         * applied settings and restore them on start so you don't need to save them manually.
         *
         * @warning Note that you don't need to save player input settings here, use InputManager for this
         * task, InputManager has save/load functions, for SETTINGS category save settings that InputManager
         * can't handle, for example: mouse sensitivity.
         *
         * @param category   Directory in which we will store this file.
         * Use PROGRESS category to save player's game progress and SETTINGS to store player's settings.
         * The difference is that for PROGRESS category we will use backup files, so that if user's
         * progress was deleted we can use a backup file to restore it. SETTINGS category does
         * not use backup files. Backup files handling is used internally so you don't need to worry
         * about it.
         * @param sFileName   Name of the file to save, prefer to have only ASCII characters in the
         * file name. We will save it to a predefined directory
         * that we also use in @ref loadFile (use @ref getFilePath to see full path),
         * the .toml extension will be added if the passed name does not have it.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> saveFile(ConfigCategory category, std::string_view sFileName);

        /**
         * Saves the current configuration to a file with a UTF-8 encoding.
         *
         * Prefer to use the other overload (@ref saveFile) that uses a category instead of a path.
         *
         * @param pathToConfigFile  Path to the file to save (if file does not exist, it will be created).
         * The .toml extension will be added if the passed name does not have it.
         * @param bEnableBackup     If 'true' will also use a backup (copy) file. @ref loadFile can use
         * backup file if a usual configuration file does not exist. Generally you want to use
         * a backup file if you are saving important information, such as player progress,
         * other cases such as player game settings and etc. usually do not need a backup but
         * you can use it if you want.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> saveFile(std::filesystem::path pathToConfigFile, bool bEnableBackup);

        /**
         * Returns full path to the file if it was loaded using @ref loadFile
         * or saved using @ref saveFile.
         *
         * @return Full path to the file.
         */
        std::filesystem::path getFilePath() const;

    private:
        /**
         * Constructs a file path from file name.
         *
         * @param category    Directory in which we will store this file.
         * Use PROGRESS category to save player's game progress and SETTINGS to store player's settings.
         * @param sFileName   Name of the file, a predefined directory
         * will be appended to the beginning,
         * the .toml extension will be added if the passed name does not have it.
         *
         * @return Error if something went wrong, valid path otherwise.
         */
        static std::variant<std::filesystem::path, Error>
        constructFilePath(ConfigCategory category, std::string_view sFileName);

        /** Config file structure */
        toml::value tomlData;

        /** Full path to file. */
        std::filesystem::path filePath;

        /** File extension used for backup files. */
        inline static const char* sBackupFileExtension = ".old";
    };

    template <typename T>
    T ConfigManager::getValue(std::string_view sSection, std::string_view sKey, T defaultValue) const {
        if (sSection.empty()) {
            return toml::find_or(tomlData, sKey.data(), defaultValue);
        } else {
            return toml::find_or(tomlData, sSection.data(), sKey.data(), defaultValue);
        }
    }

    template <typename T>
    void ConfigManager::setValue(
        std::string_view sSection, std::string_view sKey, T value, std::string_view sComment) {
        if (sSection.empty()) {
            if (sComment.empty()) {
                tomlData[sKey.data()] = toml::value(value);
            } else {
                tomlData[sKey.data()] = toml::value(value, {sComment.data()});
            }
        } else {
            if (sComment.empty()) {
                tomlData[sSection.data()][sKey.data()] = toml::value(value);
            } else {
                tomlData[sSection.data()][sKey.data()] = toml::value(value, {sComment.data()});
            }
        }
    }
} // namespace ne
