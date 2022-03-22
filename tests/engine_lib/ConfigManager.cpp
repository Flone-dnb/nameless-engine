#include "Catch2/catch.hpp"

// STL.
#include <filesystem>

// Custom.
#include "io/ConfigManager.h"

constexpr auto sTestConfigFileName = "engine lib test file";
constexpr auto sTestConfigFileSection = "test";

TEST_CASE("create simple config file") {
    using namespace ne;

    // Create file.
    {
        ConfigManager manager;
        manager.setValue(sTestConfigFileSection, "my cool string", "this is a cool string",
                         "this is a comment");
        manager.setBoolValue(sTestConfigFileSection, "my cool bool", true, "this should be true");
        manager.setDoubleValue(sTestConfigFileSection, "my cool double", 3.14159, "this is a pi value");
        manager.setLongValue(sTestConfigFileSection, "my cool long", 42, "equals to 42");

        auto res = manager.saveFile(ConfigCategory::CONFIG, sTestConfigFileName, false);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError())
            REQUIRE(false);
        }

        // Check that file exists.
        REQUIRE(std::filesystem::exists(manager.getFilePath()));
    }

    // Check if everything is correct.
    {
        ConfigManager manager;
        auto res = manager.loadFile(ConfigCategory::CONFIG, sTestConfigFileName);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError())
            REQUIRE(false);
        }

        auto real = manager.getValue(sTestConfigFileSection, "my cool string", "");
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
        manager.setValue(sTestConfigFileSection, "my cool string", "this is a cool string",
                         "this is a comment");

        auto res = manager.saveFile(ConfigCategory::CONFIG, sTestConfigFileName, true);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError())
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
        auto res = manager.loadFile(ConfigCategory::CONFIG, sTestConfigFileName);
        if (res.has_value()) {
            res->addEntry();
            INFO(res->getError())
            REQUIRE(false);
        }

        auto real = manager.getValue(sTestConfigFileSection, "my cool string", "");
        REQUIRE(real == "this is a cool string");

        REQUIRE(std::filesystem::exists(manager.getFilePath()));
        std::filesystem::remove(manager.getFilePath());

        REQUIRE(std::filesystem::exists(manager.getFilePath() + L".old"));
        std::filesystem::remove(manager.getFilePath() + L".old");
    }
}