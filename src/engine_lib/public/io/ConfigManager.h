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
        SAVE,   // used to store user's game progress
        CONFIG, // used to store user's game settings
        ENGINE, // used by engine to store engine configuration
    };

    class ConfigManager {
    public:
        /**
         * Constructs an empty configuration, use @loadFile to read configuration from a file
         * or @setValue and then @saveFile to save a new configuration.
         */
        ConfigManager() = default;
        ConfigManager(const ConfigManager &) = delete;
        ConfigManager &operator=(const ConfigManager &) = delete;

        /**
         * Loads data from INI file.
         * File should exist, otherwise an error will be returned.
         * If you used @ref saveFile before and enabled a backup file (see @ref saveFile),
         * if usual file does  not exist this function will look for a backup file and if found,
         * will copy this backup file with a name of the usual file.
         *
         * @param category    Folder in which we will store this file, note that ENGINE
         * category is used by engine internals and should not be used to store game configs.
         * Use SAVE category to save user's game progress and CONFIG to store user's settings.
         * @param sFileName   Name of the file to load:
         * @arg (preferred option) if only name is specified, we will load it from a predefined directory
         * that we also use in @ref saveFile (use @ref getFilePath to see full path),
         * the .ini extension will be added if the passed name does not have it.
         * @arg if absolute path is specified we will try to load it using this path.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> loadFile(ConfigCategory category, std::string_view sFileName);

        /**
         * Reads a value from loaded INI file (see @ref loadFile).
         *
         * @param sSection       Name of the section.
         * @param sKey           Name of the key.
         * @param sDefaultValue  Value that will be returned if the specified key was not found.
         */
        std::string_view getValue(std::string_view sSection, std::string_view sKey,
                                  std::string_view sDefaultValue) const;

        /**
         * Reads a value from loaded INI file (see @ref loadFile).
         *
         * @param sSection       Name of the section.
         * @param sKey           Name of the key.
         * @param bDefaultValue  Value that will be returned if the specified key was not found.
         */
        bool getBoolValue(std::string_view sSection, std::string_view sKey, bool bDefaultValue) const;

        /**
         * Reads a value from loaded INI file (see @ref loadFile).
         *
         * @param sSection       Name of the section.
         * @param sKey           Name of the key.
         * @param defaultValue   Value that will be returned if the specified key was not found.
         */
        double getDoubleValue(std::string_view sSection, std::string_view sKey, double defaultValue) const;

        /**
         * Reads a value from loaded INI file (see @ref loadFile).
         *
         * @param sSection       Name of the section.
         * @param sKey           Name of the key.
         * @param iDefaultValue  Value that will be returned if the specified key was not found.
         */
        long getLongValue(std::string_view sSection, std::string_view sKey, long iDefaultValue) const;

        /**
         * Sets a value. This value will not be written to file until @ref saveFile is called.
         *
         *
         * @param sSection   Name of the section.
         * @param sKey       Name of the key.
         * @param sValue     Value to set.
         * @param sComment   Comment to add to this value.
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
         * Saves the current configuration to a file.
         *
         * @param category       Folder in which we will store this file, note that ENGINE
         * category is used by engine internals and should not be used to store game configs.
         * Use SAVE category to save user's game progress and CONFIG to store user's settings.
         * @param sFileName      Name of the file to load:
         * @arg (preferred option) if only name is specified, we will save it to a predefined directory
         * that we also use in @ref loadFile (use @ref getFilePath to see full path),
         * the .ini extension will be added if the passed name does not have it.
         * @arg if absolute path is specified we will try to save it using this path.
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
         * Returns full path to the file if it was loaded using @loadFile
         * or saved using @saveFile.
         *
         * @return Full path to the file.
         */
        std::wstring getFilePath() const;

    private:
        /**
         * Constructs a file path from file name.
         *
         * @param category    Folder in which we will store this file, note that ENGINE
         * category is used by engine internals and should not be used to store game configs.
         * Use SAVE category to save user's game progress and CONFIG to store user's settings.
         * @param sFileName   Name of the file:
         * @arg (preferred option) if only name is specified, a predefined directory
         * will be appended to the beginning,
         * the .ini extension will be added if the passed name does not have it.
         * @arg if absolute path is specified we will check if it's pointing to a regular file.
         *
         * @return Error if something went wrong, ConfigManager if loaded file successfully.
         */
        static std::variant<std::filesystem::path, Error> constructFilePath(ConfigCategory category,
                                                                            std::string_view sFileName);

        /** Config file structure */
        CSimpleIniA ini;

        /** Full path to file. */
        std::wstring sFilePath;

        /** File extension used for backup files. */
        const wchar_t *sBackupFileExtension = L".old";
    };

} // namespace ne
