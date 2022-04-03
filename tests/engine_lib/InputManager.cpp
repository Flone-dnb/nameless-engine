#include "Catch2/catch.hpp"

// Custom.
#include "input/InputManager.h"

TEST_CASE("add action") {
    using namespace ne;

    const std::string sAction1Name = "test1";
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction1Keys = {KeyboardKey::KEY_0,
                                                                              KeyboardKey::KEY_Z};

    const std::string sAction2Name = "test2";
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction2Keys = {MouseButton::LEFT};

    InputManager manager;
    manager.addActionEvent(sAction1Name, vAction1Keys);
    manager.addActionEvent(sAction2Name, vAction2Keys);

    REQUIRE(manager.getActionEvent(sAction1Name).has_value());
    REQUIRE(*manager.getActionEvent(sAction1Name) == vAction1Keys);
}

TEST_CASE("remove action") {
    using namespace ne;

    const std::string sAction1Name = "test1";
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction1Keys = {KeyboardKey::KEY_0,
                                                                              KeyboardKey::KEY_Z};

    const std::string sAction2Name = "test2";
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction2Keys = {MouseButton::LEFT};

    InputManager manager;
    manager.addActionEvent(sAction1Name, vAction1Keys);
    manager.addActionEvent(sAction2Name, vAction2Keys);

    REQUIRE(!manager.removeActionEvent(sAction1Name));

    REQUIRE(manager.getAllActionEvents().size() == 1);
    REQUIRE(*manager.getActionEvent(sAction2Name) == vAction2Keys);
}

TEST_CASE("modify action") {
    using namespace ne;

    const std::string sAction1Name = "test1";
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction1Keys = {KeyboardKey::KEY_0,
                                                                              KeyboardKey::KEY_Z};

    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction2Keys = {MouseButton::LEFT};

    InputManager manager;
    manager.addActionEvent(sAction1Name, vAction1Keys);

    manager.addActionEvent(sAction1Name, vAction2Keys);

    REQUIRE(*manager.getActionEvent(sAction1Name) == vAction2Keys);
}