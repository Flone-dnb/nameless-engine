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

TEST_CASE("test saving and loading") {
    using namespace ne;

    // Prepare default action/axis events.
    const std::string sAction1Name = "action1";
    const std::vector<std::variant<KeyboardKey, MouseButton>> vDefaultAction1Keys = {MouseButton::LEFT};

    const std::string sAction2Name = "action2";
    const std::vector<std::variant<KeyboardKey, MouseButton>> vDefaultAction2Keys = {MouseButton::RIGHT,
                                                                                     KeyboardKey::KEY_R};

    const std::string sAxis1Name = "axis1";
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vDefaultAxis1Keys = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_A, KeyboardKey::KEY_D),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_UP, KeyboardKey::KEY_DOWN)};

    // Modify some events.
    const std::vector<std::variant<KeyboardKey, MouseButton>> vModifiedAction1Keys = {
        KeyboardKey::KEY_0, KeyboardKey::KEY_Z, MouseButton::MIDDLE};

    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vModifiedAxis1Keys = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_SPACE, KeyboardKey::KEY_MENU)};

    const auto sFileName = "input";

    {
        // Add default events to manager.
        InputManager manager;
        manager.addActionEvent(sAction1Name, vDefaultAction1Keys);
        manager.addActionEvent(sAction2Name, vDefaultAction2Keys);
        manager.addAxisEvent(sAxis1Name, vDefaultAxis1Keys);

        // The user modifies some keys.
        manager.addActionEvent(sAction1Name, vModifiedAction1Keys);
        manager.addAxisEvent(sAxis1Name, vModifiedAxis1Keys);

        // Save modified events.
        auto result = manager.saveToFile(sFileName);
        if (result.has_value()) {
            result->addEntry();
            INFO(result->getError())
            REQUIRE(false);
        }
    }

    {
        // Next startup, we add default keys first.
        // Add default events to manager.
        InputManager manager;
        manager.addActionEvent(sAction1Name, vDefaultAction1Keys);
        manager.addActionEvent(sAction2Name, vDefaultAction2Keys);
        manager.addAxisEvent(sAxis1Name, vDefaultAxis1Keys);

        // Load modified events.
        auto result = manager.loadFromFile(sFileName);
        if (result.has_value()) {
            result->addEntry();
            INFO(result->getError())
            REQUIRE(false);
        }

        // Check that keys are correct.

        // Action 1.
        auto actionOptional = manager.getActionEvent(sAction1Name);
        REQUIRE(actionOptional.has_value());
        REQUIRE(actionOptional.value().size() == vModifiedAction1Keys.size());
        auto vReadAction = actionOptional.value();

        // Compare keys (order may be different).
        for (const auto &wantAction : vModifiedAction1Keys) {
            auto it = std::ranges::find(vReadAction, wantAction);
            REQUIRE(it != vReadAction.end());
        }

        // Action 2.
        actionOptional = manager.getActionEvent(sAction2Name);
        REQUIRE(actionOptional.has_value());
        REQUIRE(actionOptional.value().size() == vDefaultAction2Keys.size());
        vReadAction = actionOptional.value();

        // Compare keys (order may be different).
        for (const auto &wantAction : vDefaultAction2Keys) {
            auto it = std::ranges::find(vReadAction, wantAction);
            REQUIRE(it != vReadAction.end());
        }

        // Axis 1.
        auto axisOptional = manager.getAxisEvent(sAxis1Name);
        REQUIRE(axisOptional.has_value());
        REQUIRE(axisOptional.value().size() == vModifiedAxis1Keys.size());
        auto vReadAxis = axisOptional.value();

        // Compare keys (order may be different).
        for (const auto &wantAxis : vModifiedAxis1Keys) {
            auto it = std::ranges::find(vReadAxis, wantAxis);
            REQUIRE(it != vReadAxis.end());
        }
    }
}