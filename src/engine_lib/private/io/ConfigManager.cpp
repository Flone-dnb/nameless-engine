﻿#include "io/ConfigManager.h"

// Custom.
#include "misc/Globals.h"

namespace ne {
    ConfigManager::ConfigManager() { ini.SetUnicode(true); }

    std::vector<std::string> ConfigManager::getAllConfigFiles(ConfigCategory category) {
        const auto categoryFolder = ConfigManager::getFolderForConfigFiles(category);

        std::vector<std::string> vConfigFiles;
        const auto directoryIterator = std::filesystem::directory_iterator(categoryFolder);
        for (const auto &entry : directoryIterator) {
            if (entry.is_regular_file() && entry.path().extension().compare(sBackupFileExtension)) {
                vConfigFiles.push_back(entry.path().stem().string());
            }
        }

        return vConfigFiles;
    }

    std::filesystem::path ConfigManager::getFolderForConfigFiles(ConfigCategory category) {
        std::filesystem::path basePath = getBaseDirectory();
        basePath += getApplicationName();

#if defined(WIN32)
        basePath += "\\";
#elif __linux__
        basePath += "/";
#endif

        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directory(basePath);
        }

        switch (category) {
        case ConfigCategory::SAVE:
            basePath += sSavesDirectoryName;
            break;
        case ConfigCategory::CONFIG:
            basePath += sConfigsDirectoryName;
            break;
        case ConfigCategory::ENGINE:
            basePath += sEngineDirectoryName;
            break;
        }

#if defined(WIN32)
        basePath += "\\";
#elif __linux__
        basePath += "/";
#endif

        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directory(basePath);
        }

        return basePath;
    }

    std::optional<Error> ConfigManager::loadFile(ConfigCategory category, std::string_view sFileName) {
        auto result = ConfigManager::constructFilePath(category, sFileName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }

        filePath = std::get<std::filesystem::path>(result);
        std::filesystem::path backupFile = filePath;
        backupFile += sBackupFileExtension;

        if (!std::filesystem::exists(filePath)) {
            // Check if backup file exists.
            if (std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(backupFile, filePath);
            } else {
                return Error("file and backup file do not exist");
            }
        }

        // Load file.
        const SI_Error error = ini.LoadFile(filePath.c_str());
        if (error < 0) {
            return Error(std::format("failed to load file, error code: {}", std::to_string(error)));
        }

        return {};
    }

    std::string_view ConfigManager::getValue(std::string_view sSection, std::string_view sKey,
                                             std::string_view sDefaultValue) const {
        return ini.GetValue(sSection.data(), sKey.data(), sDefaultValue.data());
    }

    bool ConfigManager::getBoolValue(std::string_view sSection, std::string_view sKey,
                                     bool bDefaultValue) const {
        return ini.GetBoolValue(sSection.data(), sKey.data(), bDefaultValue);
    }

    double ConfigManager::getDoubleValue(std::string_view sSection, std::string_view sKey,
                                         double defaultValue) const {
        return ini.GetDoubleValue(sSection.data(), sKey.data(), defaultValue);
    }

    long ConfigManager::getLongValue(std::string_view sSection, std::string_view sKey,
                                     long iDefaultValue) const {
        return ini.GetLongValue(sSection.data(), sKey.data(), iDefaultValue);
    }

    void ConfigManager::setValue(std::string_view sSection, std::string_view sKey, std::string_view sValue,
                                 std::string_view sComment) {
        std::string sFixedComment(sComment);
        if (!sComment.empty() && !sComment.starts_with('#')) {
            sFixedComment.insert(0, "# ");
        }

        if (sFixedComment.empty()) {
            ini.SetValue(sSection.data(), sKey.data(), sValue.data());
        } else {
            ini.SetValue(sSection.data(), sKey.data(), sValue.data(), sFixedComment.c_str());
        }
    }

    void ConfigManager::setBoolValue(std::string_view sSection, std::string_view sKey, bool bValue,
                                     std::string_view sComment) {
        std::string sFixedComment(sComment);
        if (!sComment.empty() && !sComment.starts_with('#')) {
            sFixedComment.insert(0, "# ");
        }

        if (sFixedComment.empty()) {
            ini.SetBoolValue(sSection.data(), sKey.data(), bValue);
        } else {
            ini.SetBoolValue(sSection.data(), sKey.data(), bValue, sFixedComment.c_str());
        }
    }

    void ConfigManager::setDoubleValue(std::string_view sSection, std::string_view sKey, double value,
                                       std::string_view sComment) {
        std::string sFixedComment(sComment);
        if (!sComment.empty() && !sComment.starts_with('#')) {
            sFixedComment.insert(0, "# ");
        }

        if (sFixedComment.empty()) {
            ini.SetDoubleValue(sSection.data(), sKey.data(), value);
        } else {
            ini.SetDoubleValue(sSection.data(), sKey.data(), value, sFixedComment.c_str());
        }
    }

    void ConfigManager::setLongValue(std::string_view sSection, std::string_view sKey, long iValue,
                                     std::string_view sComment) {
        std::string sFixedComment(sComment);
        if (!sComment.empty() && !sComment.starts_with('#')) {
            sFixedComment.insert(0, "# ");
        }

        if (sFixedComment.empty()) {
            ini.SetLongValue(sSection.data(), sKey.data(), iValue);
        } else {
            ini.SetLongValue(sSection.data(), sKey.data(), iValue, sFixedComment.c_str());
        }
    }

    std::optional<Error> ConfigManager::saveFile(ConfigCategory category, std::string_view sFileName,
                                                 bool bEnableBackup) {
        auto result = ConfigManager::constructFilePath(category, sFileName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }
        filePath = std::get<std::filesystem::path>(result);
        std::filesystem::path backupFile = filePath;
        backupFile += sBackupFileExtension;

        if (bEnableBackup) {
            if (std::filesystem::exists(filePath)) {
                if (std::filesystem::exists(backupFile)) {
                    std::filesystem::remove(backupFile);
                }
                std::filesystem::rename(filePath, backupFile);
            }
        }

        const SI_Error error = ini.SaveFile(filePath.c_str());
        if (error < 0) {
            return Error(std::format("failed to load file, error code: {}", std::to_string(error)));
        }

        if (bEnableBackup) {
            // Create backup file if not exists.
            if (!std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(filePath, backupFile);
            }
        }

        return {};
    }

    std::filesystem::path ConfigManager::getFilePath() const { return filePath; }

    std::variant<std::filesystem::path, Error> ConfigManager::constructFilePath(ConfigCategory category,
                                                                                std::string_view sFileName) {
        std::filesystem::path basePath;

        // Check if absolute path.
        if (std::filesystem::path(sFileName).is_absolute()) {
            return Error("received an absolute path as a file name");
        } else {
            if (sFileName.contains('/') || sFileName.contains('\\')) {
                return Error("either specify an absolute path or a name (in the case don't use slashes");
            }

            // Prepare directory.
            basePath = getBaseDirectory();
            basePath += getApplicationName();

#if defined(WIN32)
            basePath += "\\";
#elif __linux__
            basePath += "/";
#endif

            if (!std::filesystem::exists(basePath)) {
                std::filesystem::create_directory(basePath);
            }

            switch (category) {
            case ConfigCategory::SAVE:
                basePath += sSavesDirectoryName;
                break;
            case ConfigCategory::CONFIG:
                basePath += sConfigsDirectoryName;
                break;
            case ConfigCategory::ENGINE:
                basePath += sEngineDirectoryName;
                break;
            }

#if defined(WIN32)
            basePath += "\\";
#elif __linux__
            basePath += "/";
#endif

            if (!std::filesystem::exists(basePath)) {
                std::filesystem::create_directory(basePath);
            }

            basePath += sFileName;

            // Check extension.
            if (!sFileName.ends_with(".ini")) {
                basePath += ".ini";
            }
        }

        return basePath;
    }
} // namespace ne
