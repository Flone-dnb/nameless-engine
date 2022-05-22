// STL.
#include <filesystem>

// Custom.
#include "io/ConfigManager.h"

// External.
#include "Catch2/catch_test_macros.hpp"

constexpr auto sTestConfigFileName = "engine lib test file";
constexpr auto sTestConfigFileSection = "test";

TEST_CASE("create simple config file") {
    using namespace ne;

    // Create file.
    {
        ConfigManager manager;
        manager.setStringValue(
            sTestConfigFileSection, "my cool string", "this is a cool string", "this is a comment");
        manager.setBoolValue(sTestConfigFileSection, "my cool bool", true, "this should be true");
        manager.setDoubleValue(sTestConfigFileSection, "my cool double", 3.14159, "this is a pi value");
        manager.setLongValue(sTestConfigFileSection, "my cool long", 42, "equals to 42");

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

        auto real = manager.getStringValue(sTestConfigFileSection, "my cool string", "");
        REQUIRE(real == "this is a cool string");

        auto realBool = manager.getBoolValue(sTestConfigFileSection, "my cool bool", false);
        REQUIRE(realBool == true);

        auto realDouble = manager.getDoubleValue(sTestConfigFileSection, "my cool double", 0.0);
        REQUIRE(realDouble >= 3.13);

        auto realLong = manager.getLongValue(sTestConfigFileSection, "my cool long", 0);
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
        manager.setStringValue(
            sTestConfigFileSection, "my cool string", "this is a cool string", "this is a comment");
        manager.setBoolValue(sTestConfigFileSection, "my cool bool", true, "this should be true");
        manager.setDoubleValue(sTestConfigFileSection, "my cool double", 3.14159, "this is a pi value");
        manager.setLongValue(sTestConfigFileSection, "my cool long", 42, "equals to 42");

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

        auto real = manager.getStringValue(sTestConfigFileSection, "my cool string", "");
        REQUIRE(real == "this is a cool string");

        auto realBool = manager.getBoolValue(sTestConfigFileSection, "my cool bool", false);
        REQUIRE(realBool == true);

        auto realDouble = manager.getDoubleValue(sTestConfigFileSection, "my cool double", 0.0);
        REQUIRE(realDouble >= 3.13);

        auto realLong = manager.getLongValue(sTestConfigFileSection, "my cool long", 0);
        REQUIRE(realLong == 42);

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
        manager.setStringValue(
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

        auto real = manager.getStringValue(sTestConfigFileSection, "my cool string", "");
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
    manager.setStringValue(
        sTestConfigFileSection, "my cool string", "this is a cool string", "this is a comment");

    auto res = manager.saveFile(ConfigCategory::PROGRESS, sTestConfigFileName);
    if (res.has_value()) {
        res->addEntry();
        INFO(res->getError());
        REQUIRE(false);
    }

    const auto firstFilePath = manager.getFilePath();
    const std::string sSecondFileName = std::string(sTestConfigFileName) + "2";

    res = manager.saveFile(ConfigCategory::PROGRESS, sSecondFileName);
    if (res.has_value()) {
        res->addEntry();
        INFO(res->getError());
        REQUIRE(false);
    }

    const auto secondFilePath = manager.getFilePath();

    // Check that files exists.
    REQUIRE(std::filesystem::exists(firstFilePath));
    REQUIRE(std::filesystem::exists(secondFilePath));

    // Remove the first file.
    res = ConfigManager::removeFile(ConfigCategory::PROGRESS, sTestConfigFileName);
    if (res.has_value()) {
        res->addEntry();
        INFO(res->getError());
        REQUIRE(false);
    }

    // See the only second file exists.
    auto vFiles = ConfigManager::getAllFiles(ConfigCategory::PROGRESS);
    REQUIRE(vFiles.size() == 1);

    REQUIRE(vFiles[0] == sSecondFileName);
}

TEST_CASE("get all config files of category (with backup test)") {
    using namespace ne;

    // Create files.
    ConfigManager manager;
    manager.setStringValue(
        sTestConfigFileSection, "my cool string", "this is a cool string", "this is a comment");

    auto res = manager.saveFile(ConfigCategory::PROGRESS, sTestConfigFileName);
    if (res.has_value()) {
        res->addEntry();
        INFO(res->getError());
        REQUIRE(false);
    }

    const std::wstring sFirstFilePath = manager.getFilePath();

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

    const std::wstring sSecondFilePath = manager.getFilePath();

    // Check that file and backup exist.
    REQUIRE(std::filesystem::exists(sSecondFilePath));
    REQUIRE(std::filesystem::exists(sSecondFilePath + L".old"));

    auto vFiles = ConfigManager::getAllFiles(ConfigCategory::PROGRESS);
    REQUIRE(vFiles.size() == 2);

    // Remove first file without backup.
    std::filesystem::remove(sFirstFilePath);

    // This function should restore original file from backup.
    vFiles = ConfigManager::getAllFiles(ConfigCategory::PROGRESS);
    REQUIRE(vFiles.size() == 2);
    REQUIRE(std::filesystem::exists(sFirstFilePath));

    // Remove first file backup.
    std::filesystem::remove(sFirstFilePath + L".old");

    vFiles = ConfigManager::getAllFiles(ConfigCategory::PROGRESS);
    REQUIRE(vFiles.size() == 2);

    // Remove second file with backup.
    std::filesystem::remove(sSecondFilePath);
    std::filesystem::remove(sSecondFilePath + L".old");

    // Remove first file.
    std::filesystem::remove(sFirstFilePath);
}