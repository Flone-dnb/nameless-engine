#include "io/ConfigManager.h"

// STL.
#include <ranges>

// Custom.
#include "io/Logger.h"
#include "misc/Globals.h"

namespace ne {
    std::string ConfigManager::getConfigFormatExtension() { return ".toml"; }

    std::vector<std::string> ConfigManager::getAllFiles(ConfigCategory category) {
        const auto categoryFolder = ConfigManager::getCategoryDirectory(category);

        std::vector<std::string> vConfigFiles;
        const auto directoryIterator = std::filesystem::directory_iterator(categoryFolder);
        for (const auto& entry : directoryIterator) {
            if (entry.is_regular_file()) {
                if (entry.path().extension().string() == sBackupFileExtension) {
                    // Backup file. See if original file exists.
                    auto originalFilePath = categoryFolder;
                    originalFilePath += entry.path().stem().string(); // will return 'text.toml'
                    if (!std::filesystem::exists(originalFilePath)) {
                        // Backup file exists, but not the original file.
                        // Copy backup file as the original.
                        std::filesystem::copy_file(entry, originalFilePath);
                    }

                    // Check if we already added this file.
                    if (std::ranges::find(vConfigFiles, originalFilePath.stem().string()) ==
                        vConfigFiles.end()) {
                        vConfigFiles.push_back(originalFilePath.stem().string());
                    }
                } else {
                    // Check if we already added this file.
                    if (std::ranges::find(vConfigFiles, entry.path().stem().string()) == vConfigFiles.end()) {
                        vConfigFiles.push_back(entry.path().stem().string());
                    }
                }
            }
        }

        return vConfigFiles;
    }

    std::filesystem::path ConfigManager::getCategoryDirectory(ConfigCategory category) {
        std::filesystem::path basePath = getBaseDirectoryForConfigs();
        basePath += getApplicationName();

        if (!std::filesystem::exists(basePath)) {
            std::filesystem::create_directory(basePath);
        }

        switch (category) {
        case ConfigCategory::PROGRESS:
            basePath /= sProgressDirectoryName;
            break;
        case ConfigCategory::SETTINGS:
            basePath /= sSettingsDirectoryName;
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

    std::optional<Error> ConfigManager::removeFile(ConfigCategory category, std::string_view sFileName) {
        auto result = ConfigManager::constructFilePath(category, sFileName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }

        const auto pathToFile = std::get<std::filesystem::path>(result);
        auto pathToBackupFile = pathToFile;
        pathToBackupFile += sBackupFileExtension;

        if (!std::filesystem::exists(pathToFile) && !std::filesystem::exists(pathToBackupFile)) {
            return Error("file(s) do not exist");
        }

        if (std::filesystem::exists(pathToFile)) {
            std::filesystem::remove(pathToFile);
        }

        if (std::filesystem::exists(pathToBackupFile)) {
            std::filesystem::remove(pathToBackupFile);
        }

        return {};
    }

    std::optional<Error> ConfigManager::loadFile(ConfigCategory category, std::string_view sFileName) {
        auto result = ConfigManager::constructFilePath(category, sFileName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }

        auto optional = loadFile(std::get<std::filesystem::path>(result));
        if (optional.has_value()) {
            optional->addEntry();
            return optional;
        }
        return {};
    }

    std::optional<Error> ConfigManager::loadFile(const std::filesystem::path& pathToConfigFile) {
        auto pathToFile = pathToConfigFile;

        // Check extension.
        if (!pathToFile.string().ends_with(ConfigManager::getConfigFormatExtension())) {
            pathToFile += ConfigManager::getConfigFormatExtension();
        }

        std::filesystem::path backupFile = pathToFile;
        backupFile += sBackupFileExtension;

        if (!std::filesystem::exists(pathToFile)) {
            // Check if backup file exists.
            if (std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(backupFile, pathToFile);
            } else {
                return Error("file and backup file do not exist");
            }
        }

        // Load file.
        try {
            tomlData = toml::parse(pathToFile);
        } catch (std::exception& exception) {
            return Error(
                std::format("failed to load file {}, error: {}", pathToFile.string(), exception.what()));
        }

        filePath = pathToFile;

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
        for (const auto& key : table | std::views::keys) {
            vKeys.push_back(key);
        }

        return vKeys;
    }

    std::optional<Error> ConfigManager::saveFile(ConfigCategory category, std::string_view sFileName) {
        auto result = ConfigManager::constructFilePath(category, sFileName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }

        const bool bEnableBackup = category == ConfigCategory::PROGRESS ? true : false;

        auto optional = saveFile(std::get<std::filesystem::path>(result), bEnableBackup);
        if (optional.has_value()) {
            optional->addEntry();
            return optional;
        }
        return {};
    }

    std::optional<Error>
    ConfigManager::saveFile(const std::filesystem::path& pathToConfigFile, bool bEnableBackup) {
        auto pathToFile = pathToConfigFile;

#if defined(WIN32)
        // Check if the path length is too long.
        constexpr auto iMaxPathLimit = MAX_PATH - 15;
        const auto iFilePathLength = pathToConfigFile.string().length();
        if (iFilePathLength > iMaxPathLimit - 30 && iFilePathLength < iMaxPathLimit) {
            Logger::get().warn(
                std::format(
                    "file path length {} is close to the platform limit of {} characters (path: {})",
                    iFilePathLength,
                    iMaxPathLimit,
                    pathToConfigFile.string()),
                "");
        } else if (iFilePathLength >= iMaxPathLimit) {
            return Error(std::format(
                "file path length {} exceeds the platform limit of {} characters (path: {})",
                iFilePathLength,
                iMaxPathLimit,
                pathToConfigFile.string()));
        }
#endif

        // Check extension.
        if (!pathToFile.string().ends_with(ConfigManager::getConfigFormatExtension())) {
            pathToFile += ConfigManager::getConfigFormatExtension();
        }

        if (!std::filesystem::exists(pathToFile.parent_path())) {
            std::filesystem::create_directories(pathToFile.parent_path());
        }

        std::filesystem::path backupFile = pathToFile;
        backupFile += sBackupFileExtension;

        if (bEnableBackup) {
            if (std::filesystem::exists(pathToFile)) {
                if (std::filesystem::exists(backupFile)) {
                    std::filesystem::remove(backupFile);
                }
                std::filesystem::rename(pathToFile, backupFile);
            }
        }

        std::ofstream outFile(pathToFile, std::ios::binary);
        if (!outFile.is_open()) {
            return Error(std::format("failed to open file {} for writing", pathToFile.string()));
        }
        outFile << tomlData;
        outFile.close();

        if (bEnableBackup) {
            // Create backup file if it does not exist.
            if (!std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(pathToFile, backupFile);
            }
        }

        filePath = pathToFile;

        return {};
    }

    std::filesystem::path ConfigManager::getFilePath() const { return filePath; }

    std::variant<std::filesystem::path, Error>
    ConfigManager::constructFilePath(ConfigCategory category, std::string_view sFileName) {
        std::filesystem::path basePath;

        // Check if absolute path.
        if (std::filesystem::path(sFileName).is_absolute()) {
            return Error("received an absolute path as a file name");
        } else {
            if (sFileName.contains('/') || sFileName.contains('\\')) {
                return Error("don't use slashes in file name");
            }

            // Prepare directory.
            basePath = getBaseDirectoryForConfigs();
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
            case ConfigCategory::PROGRESS:
                basePath += sProgressDirectoryName;
                break;
            case ConfigCategory::SETTINGS:
                basePath += sSettingsDirectoryName;
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
            if (!sFileName.ends_with(ConfigManager::getConfigFormatExtension())) {
                basePath += ConfigManager::getConfigFormatExtension();
            }
        }

        return basePath;
    }
} // namespace ne
