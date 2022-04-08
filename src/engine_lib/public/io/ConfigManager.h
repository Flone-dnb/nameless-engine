#pragma once

// STL.
#include <variant>
#include <optional>
#include <filesystem>

// Custom.
#include "misc/Error.h"

// External.
#include "simpleini/SimpleIni.h"

namespace ne {
    /**
     * Describes different folders in which we can store configuration files.
     */
    enum class ConfigCategory {
        PROGRESS, // used to store player's game progress
        SETTINGS  // used to store player's game specific settings
    };

    /**
     * Allows saving and loading configuration in key-value style.
     */
    class ConfigManager {
    public:
        /**
         * Constructs an empty configuration, use @ref loadFile to read configuration from a file
         * or @ref setValue and then @ref saveFile to save a new configuration.
         */
        ConfigManager();
        ConfigManager(const ConfigManager &) = delete;
        ConfigManager &operator=(const ConfigManager &) = delete;

        /**
         * Returns file names (without extension) that this category (folder)
         * contains.
         *
         * How backup files are handled (for PROGRESS category):
         * Imagine you had a file 'player.ini' and a backup file ('player.ini.old').
         * If, for some reason, 'player.ini' (the original file) does not exist,
         * but its backup file is there, we will copy the backup file (player.ini.old)
         * as the original file (will copy 'player.ini.old' as 'player.ini') and
         * return 'player' as an available config file.
         *
         * @param category Category (folder) in which to look for files.
         *
         * @return All files in the specified category (backup files are excluded).
         */
        static std::vector<std::string> getAllConfigFiles(ConfigCategory category);

        /**
         * Returns path to the folder used to store specific category of files.
         * Returned string ends with the platform specific slash.
         * Path will be created if not existed before.
         *
         * @param category Category (folder) for which to return path.
         *
         * @return Path to the folder of the specified category.
         */
        static std::filesystem::path getFolderForConfigFiles(ConfigCategory category);

        /**
         * Loads data from INI file.
         * File should exist, otherwise an error will be returned (you can use @ref getAllConfigFiles
         * or @ref getFolderForConfigFiles to see if files exist).
         * If you used @ref saveFile before and enabled a backup file (see @ref saveFile),
         * if usual file does not exist this function will look for a backup file and if found,
         * will copy this backup file with a name of the usual file.
         *
         * @param category    Folder in which we will store this file.
         * Use PROGRESS category to save player's game progress and SETTINGS to store player's settings.
         * @param sFileName   Name of the file to load. We will load it from a predefined directory
         * that we also use in @ref saveFile (use @ref getFilePath to see full path),
         * the .ini extension will be added if the passed name does not have it.
         * @arg if absolute path is specified we will try to load it using this path.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> loadFile(ConfigCategory category, std::string_view sFileName);

        /**
         * Loads data from INI file.
         * If you used @ref saveFile before and enabled a backup file (see @ref saveFile),
         * if usual file does not exist this function will look for a backup file and if found,
         * will copy this backup file with a name of the usual file.
         *
         * @param pathToFile  Path to the file to load (should exist).
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> loadFile(std::filesystem::path pathToFile);

        /**
         * Reads a value from loaded INI file (see @ref loadFile).
         *
         * @param sSection       Name of the section. UTF-8 encoded.
         * @param sKey           Name of the key. UTF-8 encoded.
         * @param sDefaultValue  Value that will be returned if the specified key was not found. UTF-8
         * encoded.
         *
         * @return Default value if the specified section/key was not found,
         * otherwise value from INI file.
         */
        std::string_view getValue(std::string_view sSection, std::string_view sKey,
                                  std::string_view sDefaultValue) const;

        /**
         * Reads a value from loaded INI file (see @ref loadFile).
         *
         * @param sSection       Name of the section.
         * @param sKey           Name of the key.
         * @param bDefaultValue  Value that will be returned if the specified key was not found.
         *
         * @return Default value if the specified section/key was not found,
         * otherwise value from INI file.
         */
        bool getBoolValue(std::string_view sSection, std::string_view sKey, bool bDefaultValue) const;

        /**
         * Reads a value from loaded INI file (see @ref loadFile).
         *
         * @param sSection       Name of the section.
         * @param sKey           Name of the key.
         * @param defaultValue   Value that will be returned if the specified key was not found.
         *
         * @return Default value if the specified section/key was not found,
         * otherwise value from INI file.
         */
        double getDoubleValue(std::string_view sSection, std::string_view sKey, double defaultValue) const;

        /**
         * Reads a value from loaded INI file (see @ref loadFile).
         *
         * @param sSection       Name of the section.
         * @param sKey           Name of the key.
         * @param iDefaultValue  Value that will be returned if the specified key was not found.
         *
         * @return Default value if the specified section/key was not found,
         * otherwise value from INI file.
         */
        long getLongValue(std::string_view sSection, std::string_view sKey, long iDefaultValue) const;

        /**
         * Sets a value. This value will not be written to file until @ref saveFile is called.
         *
         *
         * @param sSection   Name of the section. UTF-8 encoded.
         * @param sKey       Name of the key. UTF-8 encoded.
         * @param sValue     Value to set. UTF-8 encoded.
         * @param sComment   Comment to add to this value. UTF-8 encoded.
         */
        void setValue(std::string_view sSection, std::string_view sKey, std::string_view sValue,
                      std::string_view sComment = "");

        /**
         * Sets a value. This value will not be written to file until @ref saveFile is called.
         *
         *
         * @param sSection   Name of the section.
         * @param sKey       Name of the key.
         * @param bValue     Value to set.
         * @param sComment   Comment to add to this value.
         */
        void setBoolValue(std::string_view sSection, std::string_view sKey, bool bValue,
                          std::string_view sComment = "");

        /**
         * Sets a value. This value will not be written to file until @ref saveFile is called.
         *
         *
         * @param sSection   Name of the section.
         * @param sKey       Name of the key.
         * @param value      Value to set.
         * @param sComment   Comment to add to this value.
         */
        void setDoubleValue(std::string_view sSection, std::string_view sKey, double value,
                            std::string_view sComment = "");

        /**
         * Sets a value. This value will not be written to file until @ref saveFile is called.
         *
         *
         * @param sSection   Name of the section.
         * @param sKey       Name of the key.
         * @param iValue     Value to set.
         * @param sComment   Comment to add to this value.
         */
        void setLongValue(std::string_view sSection, std::string_view sKey, long iValue,
                          std::string_view sComment = "");

        /**
         * Saves the current configuration to a file with a UTF-8 encoding.
         *
         * @param category       Folder in which we will store this file.
         * Use PROGRESS category to save player's game progress and SETTINGS to store player's settings.
         * There is no need to save render settings here as
         * some parts of the engine save their own configs, for example, renderer will save last
         * applied settings and restore them on start so you don't need to save them manually.
         * @param sFileName      Name of the file to save, prefer to have only ASCII characters in the
         * file name. We will save it to a predefined directory
         * that we also use in @ref loadFile (use @ref getFilePath to see full path),
         * the .ini extension will be added if the passed name does not have it.
         * @param bEnableBackup  If 'true' will also use a backup (copy) file. @ref loadFile can use
         * backup file if a usual configuration file does not exist. Generally you want to use
         * a backup file if you are saving important information, such as player progress,
         * other cases such as player game settings and etc. usually do not need a backup but
         * you can use a backup if you want.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> saveFile(ConfigCategory category, std::string_view sFileName,
                                      bool bEnableBackup);

        /**
         * Saves the current configuration to a file with a UTF-8 encoding.
         *
         * @param pathToFile  Path to the file to save (if file does not exist, it will be created).
         * @param bEnableBackup  If 'true' will also use a backup (copy) file. @ref loadFile can use
         * backup file if a usual configuration file does not exist. Generally you want to use
         * a backup file if you are saving important information, such as player progress,
         * other cases such as player game settings and etc. usually do not need a backup but
         * you can use a backup if you want.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> saveFile(const std::filesystem::path &pathToFile, bool bEnableBackup);

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
         * @param category    Folder in which we will store this file.
         * Use PROGRESS category to save player's game progress and SETTINGS to store player's settings.
         * @param sFileName   Name of the file, a predefined directory
         * will be appended to the beginning,
         * the .ini extension will be added if the passed name does not have it.
         *
         * @return Error if something went wrong, valid path otherwise.
         */
        static std::variant<std::filesystem::path, Error> constructFilePath(ConfigCategory category,
                                                                            std::string_view sFileName);

        /** Config file structure */
        CSimpleIniA ini;

        /** Full path to file. */
        std::filesystem::path filePath;

        /** File extension used for backup files. */
        inline static const char *sBackupFileExtension = ".old";
    };

} // namespace ne
