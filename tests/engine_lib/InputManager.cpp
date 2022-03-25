#include "Catch2/catch.hpp"

// Custom.
#include "input/InputManager.h"

TEST_CASE("add action") {
    using namespace ne;

    const std::string sAction1Name = "test1";
    const std::vector<KeyboardKey> vAction1Keys = {KeyboardKey::KEY_0, KeyboardKey::KEY_Z};

    const std::string sAction2Name = "test2";
    const std::vector<KeyboardKey> vAction2Keys = {KeyboardKey::KEY_A};

    InputManager manager;
    manager.addAction(sAction1Name, vAction1Keys);
    manager.addAction(sAction2Name, vAction2Keys);

    REQUIRE(manager.getAction(sAction1Name).has_value());
    REQUIRE(*manager.getAction(sAction1Name) == vAction1Keys);
}

TEST_CASE("remove action") {
    using namespace ne;

    const std::string sAction1Name = "test1";
    const std::vector<KeyboardKey> vAction1Keys = {KeyboardKey::KEY_0, KeyboardKey::KEY_Z};

    const std::string sAction2Name = "test2";
    const std::vector<KeyboardKey> vAction2Keys = {KeyboardKey::KEY_A};

    InputManager manager;
    manager.addAction(sAction1Name, vAction1Keys);
    manager.addAction(sAction2Name, vAction2Keys);

    REQUIRE(!manager.removeAction(sAction1Name));

    REQUIRE(manager.getAllActions().size() == 1);
}

TEST_CASE("modify action") {
    using namespace ne;

    const std::string sAction1Name = "test1";
    const std::vector<KeyboardKey> vAction1Keys = {KeyboardKey::KEY_0, KeyboardKey::KEY_Z};

    const std::vector<KeyboardKey> vAction2Keys = {KeyboardKey::KEY_A};

    InputManager manager;
    manager.addAction(sAction1Name, vAction1Keys);

    manager.addAction(sAction1Name, vAction2Keys);

    REQUIRE(*manager.getAction(sAction1Name) == vAction2Keys);
}