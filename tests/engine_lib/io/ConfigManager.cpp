// Standard.
#include <filesystem>

// Custom.
#include "io/ConfigManager.h"

// External.
#include "catch2/catch_test_macros.hpp"

constexpr auto sTestConfigFileName = "engine lib test file.toml";
constexpr auto sTestConfigFileSection = "test";

TEST_CASE("create simple config file") {
    using namespace ne;

    // Create file.
    {
        ConfigManager manager;
        manager.setValue<std::string>(
            sTestConfigFileSection, "my cool string", "this is a cool string", "this is a comment");
        manager.setValue<bool>(sTestConfigFileSection, "my cool bool", true, "this should be true");
        manager.setValue<double>(sTestConfigFileSection, "my cool double", 3.14159, "this is a pi value");
        manager.setValue<int>(sTestConfigFileSection, "my cool long", 42, "equals to 42");

        auto res = manager.saveFile(ConfigCategory::SETTINGS, sTestConfigFileName);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError());
            REQUIRE(false);
        }

        // Check that file exists.
        REQUIRE(std::filesystem::exists(manager.getFilePath()));
    }

    // Check if everything is correct.
    {
        ConfigManager manager;
        auto res = manager.loadFile(ConfigCategory::SETTINGS, sTestConfigFileName);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError());
            REQUIRE(false);
        }

        auto real = manager.getValue<std::string>(sTestConfigFileSection, "my cool string", "");
        REQUIRE(real == "this is a cool string");

        auto realBool = manager.getValue<bool>(sTestConfigFileSection, "my cool bool", false);
        REQUIRE(realBool == true);

        auto realDouble = manager.getValue<double>(sTestConfigFileSection, "my cool double", 0.0);
        REQUIRE(realDouble >= 3.13);

        auto realLong = manager.getValue<int>(sTestConfigFileSection, "my cool long", 0);
        REQUIRE(realLong == 42);

        REQUIRE(std::filesystem::exists(manager.getFilePath()));

        if (std::filesystem::exists(manager.getFilePath())) {
            std::filesystem::remove(manager.getFilePath());
        }
    }
}

TEST_CASE("create simple config file using path") {
    using namespace ne;

    std::filesystem::path testConfigPath = std::filesystem::temp_directory_path();
    testConfigPath /= "some folder";
    testConfigPath /= sTestConfigFileName;

    // Create file.
    {
        ConfigManager manager;
        manager.setValue<std::string>(
            sTestConfigFileSection, "my cool string", "this is a cool string", "this is a comment");
        manager.setValue<bool>(sTestConfigFileSection, "my cool bool", true, "this should be true");
        manager.setValue<double>(sTestConfigFileSection, "my cool double", 3.14159, "this is a pi value");
        manager.setValue<int>(sTestConfigFileSection, "my cool long", 42, "equals to 42");

        auto res = manager.saveFile(testConfigPath, false);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError());
            REQUIRE(false);
        }

        // Check that file exists.
        REQUIRE(std::filesystem::exists(manager.getFilePath()));
        REQUIRE(std::filesystem::exists(testConfigPath));
    }

    // Check if everything is correct.
    {
        ConfigManager manager;
        auto res = manager.loadFile(testConfigPath);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError());
            REQUIRE(false);
        }

        auto real = manager.getValue<std::string>(sTestConfigFileSection, "my cool string", "");
        REQUIRE(real == "this is a cool string");

        auto realBool = manager.getValue<bool>(sTestConfigFileSection, "my cool bool", false);
        REQUIRE(realBool == true);

        auto realDouble = manager.getValue<double>(sTestConfigFileSection, "my cool double", 0.0);
        REQUIRE(realDouble >= 3.13);

        auto realLong = manager.getValue<int>(sTestConfigFileSection, "my cool long", 0);
        REQUIRE(realLong == 42);

        REQUIRE(std::filesystem::exists(manager.getFilePath()));

        if (std::filesystem::exists(manager.getFilePath())) {
            std::filesystem::remove(manager.getFilePath());
        }
    }
}

TEST_CASE("create simple config file using non ASCII content") {
    using namespace ne;

    // Create file.
    {
        ConfigManager manager;
        manager.setValue<std::string>(sTestConfigFileSection, "имя персонажа", "герой", "this is a comment");

        auto res = manager.saveFile(ConfigCategory::SETTINGS, sTestConfigFileName);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError());
            REQUIRE(false);
        }

        // Check that file exists.
        REQUIRE(std::filesystem::exists(manager.getFilePath()));
    }

    // Check if everything is correct.
    {
        ConfigManager manager;
        auto res = manager.loadFile(ConfigCategory::SETTINGS, sTestConfigFileName);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError());
            REQUIRE(false);
        }

        auto real = manager.getValue<std::string>(sTestConfigFileSection, "имя персонажа", "");
        REQUIRE(real == "герой");

        REQUIRE(std::filesystem::exists(manager.getFilePath()));

        if (std::filesystem::exists(manager.getFilePath())) {
            std::filesystem::remove(manager.getFilePath());
        }
    }
}

TEST_CASE("access field that does not exist") {
    using namespace ne;

    // Create file.
    {
        ConfigManager manager;
        manager.setValue<std::string>(sTestConfigFileSection, "test", "test", "this is a comment");

        auto res = manager.saveFile(ConfigCategory::SETTINGS, sTestConfigFileName);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError());
            REQUIRE(false);
        }

        // Check that file exists.
        REQUIRE(std::filesystem::exists(manager.getFilePath()));
    }

    // Check if everything is correct.
    {
        ConfigManager manager;
        auto res = manager.loadFile(ConfigCategory::SETTINGS, sTestConfigFileName);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError());
            REQUIRE(false);
        }

        auto real = manager.getValue<std::string>(sTestConfigFileSection, "test1", "42");
        REQUIRE(real == "42");

        REQUIRE(std::filesystem::exists(manager.getFilePath()));

        if (std::filesystem::exists(manager.getFilePath())) {
            std::filesystem::remove(manager.getFilePath());
        }
    }
}

TEST_CASE("same keys in different sections") {
    using namespace ne;

    // Create file.
    {
        ConfigManager manager;
        manager.setValue<std::string>("section1", "test", "test1");
        manager.setValue<std::string>("section2", "test", "test2");

        auto res = manager.saveFile(ConfigCategory::SETTINGS, sTestConfigFileName);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError());
            REQUIRE(false);
        }

        // Check that file exists.
        REQUIRE(std::filesystem::exists(manager.getFilePath()));
    }

    // Check if everything is correct.
    {
        ConfigManager manager;
        auto res = manager.loadFile(ConfigCategory::SETTINGS, sTestConfigFileName);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError());
            REQUIRE(false);
        }

        auto real = manager.getValue<std::string>("section1", "test", "");
        REQUIRE(real == "test1");

        real = manager.getValue<std::string>("section2", "test", "");
        REQUIRE(real == "test2");

        REQUIRE(std::filesystem::exists(manager.getFilePath()));

        if (std::filesystem::exists(manager.getFilePath())) {
            std::filesystem::remove(manager.getFilePath());
        }
    }
}

TEST_CASE("test backup file") {
    using namespace ne;

    // Create file.
    {
        ConfigManager manager;
        manager.setValue<std::string>(
            sTestConfigFileSection, "my cool string", "this is a cool string", "this is a comment");

        auto res = manager.saveFile(ConfigCategory::PROGRESS, sTestConfigFileName);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError());
            REQUIRE(false);
        }

        // Check that file exists.
        REQUIRE(std::filesystem::exists(manager.getFilePath()));

        // Remove usual file.
        std::filesystem::remove(manager.getFilePath());
    }

    // Try to load configuration while usual file does not exist.
    {
        ConfigManager manager;
        auto res = manager.loadFile(ConfigCategory::PROGRESS, sTestConfigFileName);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError());
            REQUIRE(false);
        }

        auto real = manager.getValue<std::string>(sTestConfigFileSection, "my cool string", "");
        REQUIRE(real == "this is a cool string");

        REQUIRE(std::filesystem::exists(manager.getFilePath()));
        std::filesystem::remove(manager.getFilePath());

        auto backupFile = manager.getFilePath();
        backupFile += ".old";

        REQUIRE(std::filesystem::exists(backupFile));
        std::filesystem::remove(backupFile);
    }
}

TEST_CASE("remove file") {
    using namespace ne;

    // Create file.
    ConfigManager manager;
    manager.setValue<std::string>(
        sTestConfigFileSection, "my cool string", "this is a cool string", "this is a comment");

    auto res = manager.saveFile(ConfigCategory::PROGRESS, sTestConfigFileName);
    if (res.has_value()) {
        res->addEntry();
        INFO(res->getError());
        REQUIRE(false);
    }

    const auto pathToFirstFileBackup =
        std::filesystem::path(manager.getFilePath().string() + ConfigManager::getBackupFileExtension());

    const auto firstFilePath = manager.getFilePath();
    const std::string sSecondFileName = std::string(sTestConfigFileName) + "2";

    // Create another file.
    res = manager.saveFile(ConfigCategory::PROGRESS, sSecondFileName);
    if (res.has_value()) {
        res->addEntry();
        INFO(res->getError());
        REQUIRE(false);
    }

    const auto secondFilePath = manager.getFilePath();
    const auto pathToSecondFileBackup =
        std::filesystem::path(manager.getFilePath().string() + ConfigManager::getBackupFileExtension());

    // Check that files exists.
    REQUIRE(std::filesystem::exists(firstFilePath));
    REQUIRE(std::filesystem::exists(pathToFirstFileBackup));
    REQUIRE(std::filesystem::exists(secondFilePath));
    REQUIRE(std::filesystem::exists(pathToSecondFileBackup));

    // Remove the first file.
    res = ConfigManager::removeFile(ConfigCategory::PROGRESS, sTestConfigFileName);
    if (res.has_value()) {
        res->addEntry();
        INFO(res->getError());
        REQUIRE(false);
    }

    // Make sure there the backup file was deleted.
    REQUIRE(!std::filesystem::exists(pathToFirstFileBackup));

    // See the only second file exists.
    auto vFiles = ConfigManager::getAllFiles(ConfigCategory::PROGRESS);
    REQUIRE(vFiles.size() == 1);
    REQUIRE(vFiles[0] == sSecondFileName);

    // Remove the second file using absolute path.
    ConfigManager::removeFile(secondFilePath);

    // Make sure the second file was deleted.
    REQUIRE(!std::filesystem::exists(secondFilePath));
    REQUIRE(!std::filesystem::exists(pathToSecondFileBackup));
}

TEST_CASE("get all config files of category (with backup test)") {
    using namespace ne;

    // Create files.
    ConfigManager manager;
    manager.setValue<std::string>(
        sTestConfigFileSection, "my cool string", "this is a cool string", "this is a comment");

    auto res = manager.saveFile(ConfigCategory::PROGRESS, sTestConfigFileName);
    if (res.has_value()) {
        res->addEntry();
        INFO(res->getError());
        REQUIRE(false);
    }

    const auto firstFilePath = manager.getFilePath();

    // Check that file and backup exist.
    auto backupFile = manager.getFilePath();
    backupFile += ".old";

    REQUIRE(std::filesystem::exists(manager.getFilePath()));
    REQUIRE(std::filesystem::exists(backupFile));

    const std::string sSecondFileName = std::string(sTestConfigFileName) + "2";

    res = manager.saveFile(ConfigCategory::PROGRESS, sSecondFileName);
    if (res.has_value()) {
        res->addEntry();
        INFO(res->getError());
        REQUIRE(false);
    }

    const auto secondFilePath = manager.getFilePath();

    // Check that file and backup exist.
    REQUIRE(std::filesystem::exists(secondFilePath));
    REQUIRE(std::filesystem::exists(secondFilePath.string() + ".old"));

    auto vFiles = ConfigManager::getAllFiles(ConfigCategory::PROGRESS);
    REQUIRE(vFiles.size() == 2);

    // Remove first file without backup.
    std::filesystem::remove(firstFilePath);

    // This function should restore original file from backup.
    vFiles = ConfigManager::getAllFiles(ConfigCategory::PROGRESS);
    REQUIRE(vFiles.size() == 2);
    REQUIRE(std::filesystem::exists(firstFilePath));

    // Remove first file backup.
    std::filesystem::remove(firstFilePath.string() + ".old");

    vFiles = ConfigManager::getAllFiles(ConfigCategory::PROGRESS);
    REQUIRE(vFiles.size() == 2);

    // Remove second file with backup.
    std::filesystem::remove(secondFilePath);
    std::filesystem::remove(secondFilePath.string() + ".old");

    // Remove first file.
    std::filesystem::remove(firstFilePath);
}
