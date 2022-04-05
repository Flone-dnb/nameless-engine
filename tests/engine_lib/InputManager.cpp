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
    REQUIRE(manager.getActionEvent(sAction2Name).has_value());
    REQUIRE(*manager.getActionEvent(sAction1Name) == vAction1Keys);
    REQUIRE(*manager.getActionEvent(sAction2Name) == vAction2Keys);
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
    REQUIRE(manager.getActionEvent(sAction2Name).has_value());
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

    REQUIRE(manager.getActionEvent(sAction1Name).has_value());
    REQUIRE(*manager.getActionEvent(sAction1Name) == vAction2Keys);
}

TEST_CASE("add axis") {
    using namespace ne;

    const std::string sAxis1Name = "test1";
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes1 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S)};

    const std::string sAxis2Name = "test2";
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes2 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_R, KeyboardKey::KEY_A),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_RIGHT, KeyboardKey::KEY_LEFT)};

    InputManager manager;
    manager.addAxisEvent(sAxis1Name, vAxes1);
    manager.addAxisEvent(sAxis2Name, vAxes2);

    REQUIRE(manager.getAxisEvent(sAxis1Name).has_value());
    REQUIRE(manager.getAxisEvent(sAxis2Name).has_value());
    REQUIRE(*manager.getAxisEvent(sAxis1Name) == vAxes1);
    REQUIRE(*manager.getAxisEvent(sAxis2Name) == vAxes2);
}

TEST_CASE("remove axis") {
    using namespace ne;

    const std::string sAxis1Name = "test1";
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes1 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S)};

    const std::string sAxis2Name = "test2";
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes2 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_R, KeyboardKey::KEY_A),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_RIGHT, KeyboardKey::KEY_LEFT)};

    InputManager manager;
    manager.addAxisEvent(sAxis1Name, vAxes1);
    manager.addAxisEvent(sAxis2Name, vAxes2);

    REQUIRE(!manager.removeAxisEvent(sAxis1Name));

    REQUIRE(manager.getAllAxisEvents().size() == 1);
    REQUIRE(manager.getAxisEvent(sAxis2Name).has_value());
    REQUIRE(*manager.getAxisEvent(sAxis2Name) == vAxes2);
}

TEST_CASE("modify axis") {
    using namespace ne;

    const std::string sAxis1Name = "test1";
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes1 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S)};

    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes2 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_R, KeyboardKey::KEY_A),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_RIGHT, KeyboardKey::KEY_LEFT)};

    InputManager manager;
    manager.addAxisEvent(sAxis1Name, vAxes1);
    manager.addAxisEvent(sAxis1Name, vAxes2);

    REQUIRE(manager.getAxisEvent(sAxis1Name).has_value());
    REQUIRE(*manager.getAxisEvent(sAxis1Name) == vAxes2);
}