#include "io/ConfigManager.h"

// Custom.
#include "misc/Globals.h"

namespace ne {
    std::optional<Error> ConfigManager::loadFile(ConfigCategory category, std::string_view sFileName) {
        auto result = ConfigManager::constructFilePath(category, sFileName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }

        sFilePath = std::get<std::filesystem::path>(result);
        if (!std::filesystem::exists(sFilePath)) {
            // Check if backup file exists.
            if (std::filesystem::exists(sFilePath + sBackupFileExtension)) {
                std::filesystem::copy_file(sFilePath + sBackupFileExtension, sFilePath);
            } else {
                return Error("file and backup file do not exist (maybe non ASCII characters in path?)");
            }
        }

        // Load file.
        const SI_Error error = ini.LoadFile(sFilePath.c_str());
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
        if (!sComment.starts_with('#')) {
            sFixedComment.insert(0, "#");
        }
        ini.SetValue(sSection.data(), sKey.data(), sValue.data(), sFixedComment.c_str());
    }

    void ConfigManager::setBoolValue(std::string_view sSection, std::string_view sKey, bool bValue,
                                     std::string_view sComment) {
        std::string sFixedComment(sComment);
        if (!sComment.starts_with('#')) {
            sFixedComment.insert(0, "#");
        }
        ini.SetBoolValue(sSection.data(), sKey.data(), bValue, sFixedComment.c_str());
    }

    void ConfigManager::setDoubleValue(std::string_view sSection, std::string_view sKey, double value,
                                       std::string_view sComment) {
        std::string sFixedComment(sComment);
        if (!sComment.starts_with('#')) {
            sFixedComment.insert(0, "#");
        }
        ini.SetDoubleValue(sSection.data(), sKey.data(), value, sFixedComment.c_str());
    }

    void ConfigManager::setLongValue(std::string_view sSection, std::string_view sKey, long iValue,
                                     std::string_view sComment) {
        std::string sFixedComment(sComment);
        if (!sComment.starts_with('#')) {
            sFixedComment.insert(0, "#");
        }
        ini.SetLongValue(sSection.data(), sKey.data(), iValue, sFixedComment.c_str());
    }

    std::optional<Error> ConfigManager::saveFile(ConfigCategory category, std::string_view sFileName,
                                                 bool bEnableBackup) {
        auto result = ConfigManager::constructFilePath(category, sFileName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }
        sFilePath = std::get<std::filesystem::path>(result);

        if (bEnableBackup) {
            if (std::filesystem::exists(sFilePath)) {
                if (std::filesystem::exists(sFilePath + sBackupFileExtension)) {
                    std::filesystem::remove(sFilePath + sBackupFileExtension);
                }
                std::filesystem::rename(sFilePath, sFilePath + sBackupFileExtension);
            }
        }

        const SI_Error error = ini.SaveFile(sFilePath.c_str());
        if (error < 0) {
            return Error(std::format("failed to load file, error code: {}", std::to_string(error)));
        }

        if (bEnableBackup) {
            // Create backup file if not exists.
            if (!std::filesystem::exists(sFilePath + sBackupFileExtension)) {
                std::filesystem::copy_file(sFilePath, sFilePath + sBackupFileExtension);
            }
        }

        return {};
    }

    std::wstring ConfigManager::getFilePath() const { return sFilePath; }

    std::variant<std::filesystem::path, Error> ConfigManager::constructFilePath(ConfigCategory category,
                                                                                std::string_view sFileName) {
        std::filesystem::path basePath;

        // Check if absolute path.
        if (std::filesystem::path(sFileName).is_absolute()) {
            if (!is_regular_file(std::filesystem::path(sFileName))) {
                return Error("the specified absolute path does not point to a regular file");
            }
            basePath = sFileName;
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
