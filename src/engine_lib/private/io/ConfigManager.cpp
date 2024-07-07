#include "io/ConfigManager.h"

// Standard.
#include <format>

// Custom.
#include "io/Logger.h"
#include "misc/ProjectPaths.h"

namespace ne {
    std::string ConfigManager::getConfigFormatExtension() { return ".toml"; }

    std::string ConfigManager::getBackupFileExtension() { return sBackupFileExtension; }

    std::set<std::string> ConfigManager::getAllFileNames(ConfigCategory category) {
        // Prepare some variables.
        std::set<std::string> configFiles;
        const auto categoryDirectory = getCategoryDirectory(category);
        const auto directoryIterator = std::filesystem::directory_iterator(categoryDirectory);

        // Prepare a helper lambda.

        // Iterate over all entries of the directory.
        for (const auto& entry : directoryIterator) {
            // Skip directories.
            if (!entry.is_regular_file()) {
                continue;
            }

            if (entry.path().extension().string() == sBackupFileExtension) {
                // Backup file. See if original file exists.
                auto originalFilePath = categoryDirectory;
                originalFilePath /= entry.path().stem().string(); // will return 'text.toml'
                if (!std::filesystem::exists(originalFilePath)) {
                    // Backup file exists, but not the original file.
                    // Copy backup file as the original.
                    std::filesystem::copy_file(entry, originalFilePath);
                }

                // Add original file name.
                configFiles.insert(originalFilePath.stem().string());
            } else {
                // Add entry file name.
                configFiles.insert(entry.path().stem().string());
            }
        }

        return configFiles;
    }

    std::string ConfigManager::getFreeProgressProfileName() {
        const auto vProgressConfigFileNames = getAllFileNames(ConfigCategory::PROGRESS);
        return generateFreeFileName(vProgressConfigFileNames);
    }

    std::filesystem::path ConfigManager::getCategoryDirectory(ConfigCategory category) {
        std::filesystem::path basePath;

        switch (category) {
        case ConfigCategory::PROGRESS:
            basePath = ProjectPaths::getPathToPlayerProgressDirectory();
            break;
        case ConfigCategory::SETTINGS:
            basePath = ProjectPaths::getPathToPlayerSettingsDirectory();
            break;
        }

        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directories(basePath);
        }

        return basePath;
    }

    std::optional<Error> ConfigManager::removeFile(ConfigCategory category, std::string_view sFileName) {
        auto result = constructFilePath(category, sFileName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        const auto pathToFile = std::get<std::filesystem::path>(result);
        auto pathToBackupFile = pathToFile;
        pathToBackupFile += sBackupFileExtension;

        if (std::filesystem::exists(pathToFile)) {
            std::filesystem::remove(pathToFile);
        }

        if (std::filesystem::exists(pathToBackupFile)) {
            std::filesystem::remove(pathToBackupFile);
        }

        return {};
    }

    void ConfigManager::removeFile(std::filesystem::path pathToConfigFile) {
        // Check extension.
        if (!pathToConfigFile.string().ends_with(ConfigManager::getConfigFormatExtension())) {
            pathToConfigFile += ConfigManager::getConfigFormatExtension();
        }

        auto pathToBackupFile = pathToConfigFile;
        pathToBackupFile += sBackupFileExtension;

        if (std::filesystem::exists(pathToConfigFile)) {
            std::filesystem::remove(pathToConfigFile);
        }

        if (std::filesystem::exists(pathToBackupFile)) {
            std::filesystem::remove(pathToBackupFile);
        }
    }

    std::optional<Error> ConfigManager::loadFile(ConfigCategory category, std::string_view sFileName) {
        auto result = ConfigManager::constructFilePath(category, sFileName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        auto optional = loadFile(std::get<std::filesystem::path>(result));
        if (optional.has_value()) {
            optional->addCurrentLocationToErrorStack();
            return optional;
        }
        return {};
    }

    std::optional<Error> ConfigManager::loadFile(std::filesystem::path pathToConfigFile) {
        // Check extension.
        if (!pathToConfigFile.string().ends_with(ConfigManager::getConfigFormatExtension())) {
            pathToConfigFile += ConfigManager::getConfigFormatExtension();
        }

        std::filesystem::path backupFile = pathToConfigFile;
        backupFile += sBackupFileExtension;

        if (!std::filesystem::exists(pathToConfigFile)) {
            // Check if backup file exists.
            if (std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(backupFile, pathToConfigFile);
            } else {
                return Error("file or backup file do not exist");
            }
        }

        // Load file.
        try {
            tomlData = toml::parse(pathToConfigFile);
        } catch (std::exception& exception) {
            return Error(std::format(
                "failed to load file {}, error: {}", pathToConfigFile.string(), exception.what()));
        }

        filePath = pathToConfigFile;

        return {};
    }

    std::vector<std::string> ConfigManager::getAllSections() {
        std::vector<std::string> vSections;

        const auto table = tomlData.as_table();
        for (const auto& [key, value] : table) {
            if (value.is_table()) {
                vSections.push_back(key);
            }
        }

        return vSections;
    }

    std::variant<std::vector<std::string>, Error>
    ConfigManager::getAllKeysOfSection(std::string_view sSection) const {
        toml::value section;
        try {
            section = toml::find(tomlData, sSection.data());
        } catch (std::exception& ex) {
            return Error(std::format("no section \"{}\" was found ({})", sSection, ex.what()));
        }

        if (!section.is_table()) {
            return Error(std::format("found \"{}\" is not a section", sSection));
        }

        const auto table = section.as_table();

        std::vector<std::string> vKeys;
        for (const auto& [key, value] : table) {
            vKeys.push_back(key);
        }

        return vKeys;
    }

    std::optional<Error> ConfigManager::saveFile(ConfigCategory category, std::string_view sFileName) {
        auto result = ConfigManager::constructFilePath(category, sFileName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        const bool bEnableBackup = category == ConfigCategory::PROGRESS;

        auto optional = saveFile(std::get<std::filesystem::path>(result), bEnableBackup);
        if (optional.has_value()) {
            optional->addCurrentLocationToErrorStack();
            return optional;
        }
        return {};
    }

    std::optional<Error> ConfigManager::saveFile(std::filesystem::path pathToConfigFile, bool bEnableBackup) {
        // Check extension.
        if (!pathToConfigFile.string().ends_with(ConfigManager::getConfigFormatExtension())) {
            pathToConfigFile += ConfigManager::getConfigFormatExtension();
        }

        // Make sure the path exists.
        if (!std::filesystem::exists(pathToConfigFile.parent_path())) {
            std::filesystem::create_directories(pathToConfigFile.parent_path());
        }

#if defined(WIN32)
        // Check if the path length is too long.
        constexpr auto iMaxPathLimitBound = 15;
        constexpr auto iMaxPathLimit = MAX_PATH - iMaxPathLimitBound;
        const auto iFilePathLength = pathToConfigFile.string().length();
        if (iFilePathLength > iMaxPathLimit - (iMaxPathLimitBound * 2) && iFilePathLength < iMaxPathLimit) {
            Logger::get().warn(std::format(
                "file path length {} is close to the platform limit of {} characters (path: {})",
                iFilePathLength,
                iMaxPathLimit,
                pathToConfigFile.string()));
        } else if (iFilePathLength >= iMaxPathLimit) {
            return Error(std::format(
                "file path length {} exceeds the platform limit of {} characters (path: {})",
                iFilePathLength,
                iMaxPathLimit,
                pathToConfigFile.string()));
        }
#endif

        std::filesystem::path backupFile = pathToConfigFile;
        backupFile += sBackupFileExtension;

        if (bEnableBackup) {
            // Check if we already had this file saved.
            if (std::filesystem::exists(pathToConfigFile)) {
                if (std::filesystem::exists(backupFile)) {
                    std::filesystem::remove(backupFile);
                }
                std::filesystem::rename(pathToConfigFile, backupFile);
            }
        }

        std::ofstream outFile(pathToConfigFile, std::ios::binary);
        if (!outFile.is_open()) {
            return Error(std::format("failed to open file {} for writing", pathToConfigFile.string()));
        }
        outFile << toml::format(tomlData);
        outFile.close();

        if (bEnableBackup) {
            // Create backup file if it does not exist.
            if (!std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(pathToConfigFile, backupFile);
            }
        }

        filePath = pathToConfigFile;

        return {};
    }

    std::filesystem::path ConfigManager::getFilePath() const { return filePath; }

    std::variant<std::filesystem::path, Error>
    ConfigManager::constructFilePath(ConfigCategory category, std::string_view sFileName) {
        std::filesystem::path basePath;

        // Check if absolute path.
        if (std::filesystem::path(sFileName).is_absolute()) {
            return Error("received an absolute path as a file name");
        }

        if (sFileName.contains('/') || sFileName.contains('\\')) {
            return Error("don't use slashes in file name");
        }

        switch (category) {
        case ConfigCategory::PROGRESS:
            basePath = ProjectPaths::getPathToPlayerProgressDirectory();
            break;
        case ConfigCategory::SETTINGS:
            basePath = ProjectPaths::getPathToPlayerSettingsDirectory();
            break;
        }

        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directories(basePath);
        }

        basePath /= sFileName;

        // Check extension.
        if (!sFileName.ends_with(ConfigManager::getConfigFormatExtension())) {
            basePath += ConfigManager::getConfigFormatExtension();
        }

        return basePath;
    }

    std::string ConfigManager::generateFreeFileName(
        const std::set<std::string>& usedFileNames, const std::string& sFileNamePrefix) {
        for (size_t i = 0; i < SIZE_MAX; i++) {
            std::string sFileNameToUse = sFileNamePrefix + std::to_string(i);
            bool bIsFree = true;

            for (const auto& sUsedFileName : usedFileNames) {
                if (sUsedFileName == sFileNameToUse) {
                    bIsFree = false;
                    break;
                }
            }

            if (bIsFree) {
                return sFileNameToUse;
            }
        }

        return generateFreeFileName(usedFileNames, sFileNamePrefix + "0");
    }
} // namespace ne
