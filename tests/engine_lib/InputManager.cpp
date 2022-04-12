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
    auto optional = manager.addActionEvent(sAction1Name, vAction1Keys);
    REQUIRE(!optional.has_value());
    optional = manager.addActionEvent(sAction2Name, vAction2Keys);
    REQUIRE(!optional.has_value());

    auto result1 = manager.getActionEvent(sAction1Name);
    auto result2 = manager.getActionEvent(sAction2Name);
    REQUIRE(result1.has_value());
    REQUIRE(result2.has_value());
    REQUIRE(result1.value() == vAction1Keys);
    REQUIRE(result2.value() == vAction2Keys);
}

TEST_CASE("remove action") {
    using namespace ne;

    const std::string sAction1Name = "test1";
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction1Keys = {KeyboardKey::KEY_0,
                                                                              KeyboardKey::KEY_Z};

    const std::string sAction2Name = "test2";
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction2Keys = {MouseButton::LEFT};

    InputManager manager;
    auto optional = manager.addActionEvent(sAction1Name, vAction1Keys);
    REQUIRE(!optional.has_value());
    optional = manager.addActionEvent(sAction2Name, vAction2Keys);
    REQUIRE(!optional.has_value());

    REQUIRE(!manager.removeActionEvent(sAction1Name));

    REQUIRE(manager.getAllActionEvents().size() == 1);

    auto result = manager.getActionEvent(sAction2Name);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == vAction2Keys);
}

TEST_CASE("fail to add an action event with already used name") {
    using namespace ne;

    const std::string sAction1Name = "test1";
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction1Keys = {KeyboardKey::KEY_0,
                                                                              KeyboardKey::KEY_Z};

    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction2Keys = {MouseButton::LEFT};

    InputManager manager;
    auto optional = manager.addActionEvent(sAction1Name, vAction1Keys);
    REQUIRE(!optional.has_value());

    optional = manager.addActionEvent(sAction1Name, vAction2Keys);
    REQUIRE(optional.has_value()); // should fail

    const auto eventKeys = manager.getActionEvent(sAction1Name);
    REQUIRE(eventKeys.has_value());
    REQUIRE(eventKeys.value() == vAction1Keys);
}

TEST_CASE("modify action") {
    using namespace ne;

    const std::string sAction1Name = "test1";
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction1Keys = {KeyboardKey::KEY_0,
                                                                              KeyboardKey::KEY_Z};

    const std::variant<KeyboardKey, MouseButton> oldKey = KeyboardKey::KEY_Z;
    const std::variant<KeyboardKey, MouseButton> newKey = MouseButton::LEFT;

    InputManager manager;
    auto optional = manager.addActionEvent(sAction1Name, vAction1Keys);
    REQUIRE(!optional.has_value());

    optional = manager.modifyActionEventKey(sAction1Name, oldKey, newKey);
    REQUIRE(!optional.has_value());

    const std::vector<std::variant<KeyboardKey, MouseButton>> vExpectedKeys = {KeyboardKey::KEY_0,
                                                                               MouseButton::LEFT};

    const auto eventKeys = manager.getActionEvent(sAction1Name);
    REQUIRE(eventKeys.has_value());

    auto resultKeys = eventKeys.value();

    // Compare keys (order may be different).
    REQUIRE(resultKeys.size() == vExpectedKeys.size());
    for (const auto &wantKey : vExpectedKeys) {
        REQUIRE(std::ranges::find(resultKeys, wantKey) != resultKeys.end());
    }
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
    auto optional = manager.addAxisEvent(sAxis1Name, vAxes1);
    REQUIRE(!optional.has_value());
    optional = manager.addAxisEvent(sAxis2Name, vAxes2);
    REQUIRE(!optional.has_value());

    auto firstEvents = manager.getAxisEvent(sAxis1Name);
    auto secondEvents = manager.getAxisEvent(sAxis2Name);
    REQUIRE(firstEvents.has_value());
    REQUIRE(secondEvents.has_value());
    REQUIRE(firstEvents.value() == vAxes1);
    REQUIRE(secondEvents.value() == vAxes2);
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
    auto optional = manager.addAxisEvent(sAxis1Name, vAxes1);
    REQUIRE(!optional.has_value());
    optional = manager.addAxisEvent(sAxis2Name, vAxes2);
    REQUIRE(!optional.has_value());

    REQUIRE(!manager.removeAxisEvent(sAxis1Name));

    REQUIRE(manager.getAllAxisEvents().size() == 1);

    auto result = manager.getAxisEvent(sAxis2Name);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == vAxes2);
}

TEST_CASE("fail to add an axis event with already used name") {
    using namespace ne;

    const std::string sAxis1Name = "test1";
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes1 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S)};

    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes2 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_R, KeyboardKey::KEY_A),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_RIGHT, KeyboardKey::KEY_LEFT)};

    InputManager manager;
    auto optional = manager.addAxisEvent(sAxis1Name, vAxes1);
    REQUIRE(!optional.has_value());

    optional = manager.addAxisEvent(sAxis1Name, vAxes2);
    REQUIRE(optional.has_value()); // should fail

    const auto eventKeys = manager.getAxisEvent(sAxis1Name);
    REQUIRE(eventKeys.has_value());
    REQUIRE(eventKeys.value() == vAxes1);
}

TEST_CASE("modify axis") {
    using namespace ne;

    const std::string sAxis1Name = "test1";
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes1 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_UP, KeyboardKey::KEY_DOWN)};

    const std::pair<KeyboardKey, KeyboardKey> oldPair = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S)};
    const std::pair<KeyboardKey, KeyboardKey> newPair = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_A, KeyboardKey::KEY_D)};

    InputManager manager;
    auto optional = manager.addAxisEvent(sAxis1Name, vAxes1);
    REQUIRE(!optional.has_value());

    optional = manager.modifyAxisEventKey(sAxis1Name, oldPair, newPair);
    REQUIRE(!optional.has_value());

    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vExpectedKeys = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_A, KeyboardKey::KEY_D),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_UP, KeyboardKey::KEY_DOWN)};

    const auto eventKeys = manager.getAxisEvent(sAxis1Name);
    REQUIRE(eventKeys.has_value());
    auto resultKeys = eventKeys.value();

    // Compare keys (order may be different).
    REQUIRE(resultKeys.size() == vExpectedKeys.size());
    for (const auto &wantKey : vExpectedKeys) {
        REQUIRE(std::ranges::find(resultKeys, wantKey) != resultKeys.end());
    }
}

TEST_CASE("fail modify axis with wrong/flipped keys") {
    using namespace ne;

    const std::string sAxis1Name = "test1";
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes1 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_UP, KeyboardKey::KEY_DOWN)};

    const std::pair<KeyboardKey, KeyboardKey> oldPair1 = {
        // flipped keys
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_S, KeyboardKey::KEY_W)};
    const std::pair<KeyboardKey, KeyboardKey> oldPair2 = {
        // wrong key
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_D)};
    const std::pair<KeyboardKey, KeyboardKey> newPair = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_A, KeyboardKey::KEY_D)};

    InputManager manager;
    auto optional = manager.addAxisEvent(sAxis1Name, vAxes1);
    REQUIRE(!optional.has_value());

    // flipped pair
    optional = manager.modifyAxisEventKey(sAxis1Name, oldPair1, newPair);
    REQUIRE(optional.has_value()); // should fail

    // wrong key
    optional = manager.modifyAxisEventKey(sAxis1Name, oldPair2, newPair);
    REQUIRE(optional.has_value()); // should fail

    const auto eventKeys = manager.getAxisEvent(sAxis1Name);
    REQUIRE(eventKeys.has_value());
    auto resultKeys = eventKeys.value();

    // Compare keys (order may be different).
    REQUIRE(resultKeys.size() == vAxes1.size());
    for (const auto &wantKey : vAxes1) {
        REQUIRE(std::ranges::find(resultKeys, wantKey) != resultKeys.end());
    }
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
    const std::variant<KeyboardKey, MouseButton> oldAction2Key = MouseButton::RIGHT;
    const std::variant<KeyboardKey, MouseButton> newAction2Key = KeyboardKey::KEY_A;

    constexpr std::pair<KeyboardKey, KeyboardKey> oldAxis1Key =
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_UP, KeyboardKey::KEY_DOWN);
    constexpr std::pair<KeyboardKey, KeyboardKey> newAxis1Key =
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_T, KeyboardKey::KEY_G);

    // Expected.
    const std::vector<std::variant<KeyboardKey, MouseButton>> vExpectedAction1Keys = {MouseButton::LEFT};

    const std::vector<std::variant<KeyboardKey, MouseButton>> vExpectedAction2Keys = {KeyboardKey::KEY_A,
                                                                                      KeyboardKey::KEY_R};

    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vExpectedAxis1Keys = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_A, KeyboardKey::KEY_D),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_T, KeyboardKey::KEY_G)};

    const auto sFileName = "input";

    {
        // Add default events to manager.
        InputManager manager;
        auto optional = manager.addActionEvent(sAction1Name, vDefaultAction1Keys);
        REQUIRE(!optional.has_value());
        optional = manager.addActionEvent(sAction2Name, vDefaultAction2Keys);
        REQUIRE(!optional.has_value());
        optional = manager.addAxisEvent(sAxis1Name, vDefaultAxis1Keys);
        REQUIRE(!optional.has_value());

        // The user modifies some keys.
        optional = manager.modifyActionEventKey(sAction2Name, oldAction2Key, newAction2Key);
        REQUIRE(!optional.has_value());
        optional = manager.modifyAxisEventKey(sAxis1Name, oldAxis1Key, newAxis1Key);
        REQUIRE(!optional.has_value());

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
        auto optional = manager.addActionEvent(sAction1Name, vDefaultAction1Keys);
        REQUIRE(!optional.has_value());
        optional = manager.addActionEvent(sAction2Name, vDefaultAction2Keys);
        REQUIRE(!optional.has_value());
        optional = manager.addAxisEvent(sAxis1Name, vDefaultAxis1Keys);
        REQUIRE(!optional.has_value());

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
        REQUIRE(actionOptional.value().size() == vExpectedAction1Keys.size());
        auto vReadAction = actionOptional.value();

        // Compare keys (order may be different).
        for (const auto &wantAction : vExpectedAction1Keys) {
            auto it = std::ranges::find(vReadAction, wantAction);
            REQUIRE(it != vReadAction.end());
        }

        // Action 2.
        actionOptional = manager.getActionEvent(sAction2Name);
        REQUIRE(actionOptional.has_value());
        REQUIRE(actionOptional.value().size() == vExpectedAction2Keys.size());
        vReadAction = actionOptional.value();

        // Compare keys (order may be different).
        for (const auto &wantAction : vExpectedAction2Keys) {
            auto it = std::ranges::find(vReadAction, wantAction);
            REQUIRE(it != vReadAction.end());
        }

        // Axis 1.
        auto axisOptional = manager.getAxisEvent(sAxis1Name);
        REQUIRE(axisOptional.has_value());
        REQUIRE(axisOptional.value().size() == vExpectedAxis1Keys.size());
        auto vReadAxis = axisOptional.value();

        // Compare keys (order may be different).
        for (const auto &wantAxis : vExpectedAxis1Keys) {
            auto it = std::ranges::find(vReadAxis, wantAxis);
            REQUIRE(it != vReadAxis.end());
        }
    }
}

TEST_CASE("is key used") {
    using namespace ne;

    const std::string sAction1Name = "test1";
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction1Keys = {KeyboardKey::KEY_0,
                                                                              KeyboardKey::KEY_Z};

    const std::string sAction2Name = "test2";
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction2Keys = {KeyboardKey::KEY_LEFT};

    const std::string sAxis2Name = "test2";
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes2 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_R, KeyboardKey::KEY_A),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_RIGHT, KeyboardKey::KEY_LEFT)};

    InputManager manager;
    auto optional = manager.addActionEvent(sAction1Name, vAction1Keys);
    REQUIRE(!optional.has_value());
    optional = manager.addActionEvent(sAction2Name, vAction2Keys);
    REQUIRE(!optional.has_value());
    optional = manager.addAxisEvent(sAxis2Name, vAxes2);
    REQUIRE(!optional.has_value());

    // First test.
    auto [actionEvents, axisEvents] = manager.isKeyUsed(KeyboardKey::KEY_LEFT);
    REQUIRE(actionEvents.size() == 1);
    REQUIRE(axisEvents.size() == 1);

    REQUIRE(actionEvents.find(sAction2Name) != actionEvents.end());
    REQUIRE(axisEvents.find(sAxis2Name) != axisEvents.end());

    // Another test.
    auto [actionEvents2, axisEvents2] = manager.isKeyUsed(KeyboardKey::KEY_0);
    REQUIRE(actionEvents2.size() == 1);
    REQUIRE(axisEvents2.size() == 0);

    REQUIRE(actionEvents2.find(sAction1Name) != actionEvents2.end());
}