#pragma once

// STL.
#include <variant>
#include <memory>
#include <optional>
#include <filesystem>

// Custom.
#include "misc/Error.h"

// External.
#include "simpleini/SimpleIni.h"

namespace ne {
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
         *
         * @param sFileName Name of the file to load:
         * @arg (preferred option) if only name is specified, we will load it from a predefined directory
         * that we also use in @ref saveFile (use @ref getFilePath to see full path),
         * the .ini extension will be added if the passed name does not have it.
         * @arg if absolute path is specified we will try to load it using this path.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> loadFile(std::string_view sFileName);

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
         * @param sFileName Name of the file to load:
         * @arg (preferred option) if only name is specified, we will save it to a predefined directory
         * that we also use in @ref loadFile (use @ref getFilePath to see full path),
         * the .ini extension will be added if the passed name does not have it.
         * @arg if absolute path is specified we will try to save it using this path.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> saveFile(std::string_view sFileName);

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
         * @param sFileName Name of the file:
         * @arg (preferred option) if only name is specified, a predefined directory
         * will be appended to the beginning,
         * the .ini extension will be added if the passed name does not have it.
         * @arg if absolute path is specified we will check if it's pointing to a regular file.
         *
         * @return Error if something went wrong, ConfigManager if loaded file successfully.
         */
        static std::variant<std::filesystem::path, Error> constructFilePath(std::string_view sFileName);

        /** Config file structure */
        CSimpleIniA ini;

        /** Full path to file. */
        std::wstring sFilePath;
    };

} // namespace ne
