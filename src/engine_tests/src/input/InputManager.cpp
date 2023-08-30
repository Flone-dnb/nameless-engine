// Custom.
#include "input/InputManager.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("add action") {
    using namespace ne;

    const unsigned int iAction1Id = 0;
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction1Keys = {
        KeyboardKey::KEY_0, KeyboardKey::KEY_Z};

    const unsigned int iAction2Id = 1;
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction2Keys = {MouseButton::LEFT};

    InputManager manager;
    auto optional = manager.addActionEvent(iAction1Id, vAction1Keys);
    REQUIRE(!optional.has_value());
    optional = manager.addActionEvent(iAction2Id, vAction2Keys);
    REQUIRE(!optional.has_value());

    const auto vEvent1Keys = manager.getActionEvent(iAction1Id);
    const auto vEvent2Keys = manager.getActionEvent(iAction2Id);
    REQUIRE(!vEvent1Keys.empty());
    REQUIRE(!vEvent2Keys.empty());

    // Compare keys (order may be different).
    REQUIRE(vEvent1Keys.size() == vAction1Keys.size());
    for (const auto& wantKey : vAction1Keys) {
        bool bFound = false;
        for (const auto& foundKey : vEvent1Keys) {
            if (foundKey == wantKey) {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }

    // Compare keys (order may be different).
    REQUIRE(vEvent2Keys.size() == vAction2Keys.size());
    for (const auto& wantKey : vAction2Keys) {
        bool bFound = false;
        for (const auto& foundKey : vEvent2Keys) {
            if (foundKey == wantKey) {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }
}

TEST_CASE("remove action") {
    using namespace ne;

    const unsigned int iAction1Id = 0;
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction1Keys = {
        KeyboardKey::KEY_0, KeyboardKey::KEY_Z};

    const unsigned int iAction2Id = 1;
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction2Keys = {MouseButton::LEFT};

    InputManager manager;
    auto optional = manager.addActionEvent(iAction1Id, vAction1Keys);
    REQUIRE(!optional.has_value());
    optional = manager.addActionEvent(iAction2Id, vAction2Keys);
    REQUIRE(!optional.has_value());

    REQUIRE(!manager.removeActionEvent(iAction1Id));

    REQUIRE(manager.getAllActionEvents().size() == 1);

    const auto vEventKeys = manager.getActionEvent(iAction2Id);
    REQUIRE(!vEventKeys.empty());
    REQUIRE(vEventKeys == vAction2Keys);
}

TEST_CASE("fail to add an action event with already used ID") {
    using namespace ne;

    const unsigned int iAction1Id = 0;
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction1Keys = {
        KeyboardKey::KEY_0, KeyboardKey::KEY_Z};

    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction2Keys = {MouseButton::LEFT};

    InputManager manager;
    auto optional = manager.addActionEvent(iAction1Id, vAction1Keys);
    REQUIRE(!optional.has_value());

    optional = manager.addActionEvent(iAction1Id, vAction2Keys);
    REQUIRE(optional.has_value()); // should fail

    const auto vEventKeys = manager.getActionEvent(iAction1Id);
    REQUIRE(!vEventKeys.empty());

    // Compare keys (order may be different).
    REQUIRE(vEventKeys.size() == vAction1Keys.size());
    for (const auto& wantKey : vAction1Keys) {
        bool bFound = false;
        for (const auto& foundKey : vEventKeys) {
            if (foundKey == wantKey) {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }
}

TEST_CASE("modify action") {
    using namespace ne;

    const unsigned int iAction1Id = 0;
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction1Keys = {
        KeyboardKey::KEY_0, KeyboardKey::KEY_Z};

    const std::variant<KeyboardKey, MouseButton> oldKey = KeyboardKey::KEY_Z;
    const std::variant<KeyboardKey, MouseButton> newKey = MouseButton::LEFT;

    InputManager manager;
    auto optional = manager.addActionEvent(iAction1Id, vAction1Keys);
    REQUIRE(!optional.has_value());

    optional = manager.modifyActionEventKey(iAction1Id, oldKey, newKey);
    REQUIRE(!optional.has_value());

    const std::vector<std::variant<KeyboardKey, MouseButton>> vExpectedKeys = {
        KeyboardKey::KEY_0, MouseButton::LEFT};

    const auto vEventKeys = manager.getActionEvent(iAction1Id);
    REQUIRE(!vEventKeys.empty());

    // Compare keys (order may be different).
    REQUIRE(vEventKeys.size() == vExpectedKeys.size());
    for (const auto& wantKey : vExpectedKeys) {
        bool bFound = false;
        for (const auto& foundKey : vEventKeys) {
            if (foundKey == wantKey) {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }
}

TEST_CASE("add axis") {
    using namespace ne;

    const unsigned int iAxis1Id = 0;
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes1 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S)};

    const unsigned int iAxis2Id = 1;
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes2 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_R, KeyboardKey::KEY_A),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_RIGHT, KeyboardKey::KEY_LEFT)};

    InputManager manager;
    auto optional = manager.addAxisEvent(iAxis1Id, vAxes1);
    REQUIRE(!optional.has_value());
    optional = manager.addAxisEvent(iAxis2Id, vAxes2);
    REQUIRE(!optional.has_value());

    auto vAxisEvent1Keys = manager.getAxisEvent(iAxis1Id);
    auto vAxisEvent2Keys = manager.getAxisEvent(iAxis2Id);
    REQUIRE(!vAxisEvent1Keys.empty());
    REQUIRE(!vAxisEvent2Keys.empty());

    // Compare keys (order may be different).
    REQUIRE(vAxisEvent1Keys == vAxes1);
    REQUIRE(vAxisEvent2Keys.size() == vAxes2.size());
    for (const auto& wantAxis : vAxes2) {
        bool bFound = false;
        for (const auto& foundAxis : vAxisEvent2Keys) {
            if (foundAxis == wantAxis) {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }
}

TEST_CASE("remove axis") {
    using namespace ne;

    const unsigned int iAxis1Id = 0;
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes1 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S)};

    const unsigned int iAxis2Id = 1;
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes2 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_R, KeyboardKey::KEY_A),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_RIGHT, KeyboardKey::KEY_LEFT)};

    InputManager manager;
    auto optional = manager.addAxisEvent(iAxis1Id, vAxes1);
    REQUIRE(!optional.has_value());
    optional = manager.addAxisEvent(iAxis2Id, vAxes2);
    REQUIRE(!optional.has_value());

    REQUIRE(!manager.removeAxisEvent(iAxis1Id));

    REQUIRE(manager.getAllAxisEvents().size() == 1);

    auto vAxisEvent2keys = manager.getAxisEvent(iAxis2Id);
    REQUIRE(!vAxisEvent2keys.empty());

    // Compare keys (order may be different).
    REQUIRE(vAxisEvent2keys.size() == vAxes2.size());
    for (const auto& wantAxis : vAxes2) {
        bool bFound = false;
        for (const auto& foundAxis : vAxisEvent2keys) {
            if (foundAxis == wantAxis) {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }
}

TEST_CASE("fail to add an axis event with already used ID") {
    using namespace ne;

    const unsigned int iAxis1Id = 0;
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes1 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S)};

    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes2 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_R, KeyboardKey::KEY_A),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_RIGHT, KeyboardKey::KEY_LEFT)};

    InputManager manager;
    auto optional = manager.addAxisEvent(iAxis1Id, vAxes1);
    REQUIRE(!optional.has_value());

    optional = manager.addAxisEvent(iAxis1Id, vAxes2);
    REQUIRE(optional.has_value()); // should fail

    const auto vEventKeys = manager.getAxisEvent(iAxis1Id);
    REQUIRE(!vEventKeys.empty());
    REQUIRE(vEventKeys == vAxes1);
}

TEST_CASE("modify axis") {
    using namespace ne;

    const unsigned int iAxis1Id = 0;
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes1 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_UP, KeyboardKey::KEY_DOWN)};

    const std::pair<KeyboardKey, KeyboardKey> oldPair = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S)};
    const std::pair<KeyboardKey, KeyboardKey> newPair = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_A, KeyboardKey::KEY_D)};

    InputManager manager;
    auto optional = manager.addAxisEvent(iAxis1Id, vAxes1);
    REQUIRE(!optional.has_value());

    optional = manager.modifyAxisEventKey(iAxis1Id, oldPair, newPair);
    REQUIRE(!optional.has_value());

    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vExpectedKeys = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_A, KeyboardKey::KEY_D),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_UP, KeyboardKey::KEY_DOWN)};

    const auto vEventKeys = manager.getAxisEvent(iAxis1Id);
    REQUIRE(!vEventKeys.empty());

    // Compare keys (order may be different).
    REQUIRE(vEventKeys.size() == vExpectedKeys.size());
    for (const auto& wantKey : vExpectedKeys) {
        bool bFound = false;
        for (const auto& foundKey : vEventKeys) {
            if (foundKey == wantKey) {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }
}

TEST_CASE("fail modify axis with wrong/flipped keys") {
    using namespace ne;

    const unsigned int iAxis1Id = 0;
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
    auto optional = manager.addAxisEvent(iAxis1Id, vAxes1);
    REQUIRE(!optional.has_value());

    // flipped pair
    optional = manager.modifyAxisEventKey(iAxis1Id, oldPair1, newPair);
    REQUIRE(optional.has_value()); // should fail

    // wrong key
    optional = manager.modifyAxisEventKey(iAxis1Id, oldPair2, newPair);
    REQUIRE(optional.has_value()); // should fail

    const auto vEventKeys = manager.getAxisEvent(iAxis1Id);
    REQUIRE(!vEventKeys.empty());

    // Compare keys (order may be different).
    REQUIRE(vEventKeys.size() == vAxes1.size());
    for (const auto& wantKey : vAxes1) {
        bool bFound = false;
        for (const auto& foundKey : vEventKeys) {
            if (foundKey == wantKey) {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }
}

TEST_CASE("test saving and loading") {
    using namespace ne;

    // Prepare default action/axis events.
    const unsigned int iAction1Id = 0;
    const std::vector<std::variant<KeyboardKey, MouseButton>> vDefaultAction1Keys = {MouseButton::LEFT};

    const unsigned int iAction2Id = 1;
    const std::vector<std::variant<KeyboardKey, MouseButton>> vDefaultAction2Keys = {
        MouseButton::RIGHT, KeyboardKey::KEY_R};

    const unsigned int iAxis1Id = 0;
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

    const std::vector<std::variant<KeyboardKey, MouseButton>> vExpectedAction2Keys = {
        KeyboardKey::KEY_A, KeyboardKey::KEY_R};

    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vExpectedAxis1Keys = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_A, KeyboardKey::KEY_D),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_T, KeyboardKey::KEY_G)};

    const auto sFileName = "input";

    {
        // Add default events to manager.
        InputManager manager;
        auto optional = manager.addActionEvent(iAction1Id, vDefaultAction1Keys);
        REQUIRE(!optional.has_value());
        optional = manager.addActionEvent(iAction2Id, vDefaultAction2Keys);
        REQUIRE(!optional.has_value());
        optional = manager.addAxisEvent(iAxis1Id, vDefaultAxis1Keys);
        REQUIRE(!optional.has_value());

        // The user modifies some keys.
        optional = manager.modifyActionEventKey(iAction2Id, oldAction2Key, newAction2Key);
        REQUIRE(!optional.has_value());
        optional = manager.modifyAxisEventKey(iAxis1Id, oldAxis1Key, newAxis1Key);
        REQUIRE(!optional.has_value());

        // Save modified events.
        auto result = manager.saveToFile(sFileName);
        if (result.has_value()) {
            result->addCurrentLocationToErrorStack();
            INFO(result->getFullErrorMessage());
            REQUIRE(false);
        }
    }

    {
        // Next startup, we add default keys first.
        // Add default events to manager.
        InputManager manager;
        auto optional = manager.addActionEvent(iAction1Id, vDefaultAction1Keys);
        REQUIRE(!optional.has_value());
        optional = manager.addActionEvent(iAction2Id, vDefaultAction2Keys);
        REQUIRE(!optional.has_value());
        optional = manager.addAxisEvent(iAxis1Id, vDefaultAxis1Keys);
        REQUIRE(!optional.has_value());

        // Load modified events.
        auto result = manager.loadFromFile(sFileName);
        if (result.has_value()) {
            result->addCurrentLocationToErrorStack();
            INFO(result->getFullErrorMessage());
            REQUIRE(false);
        }

        // Check that keys are correct.

        // Action 1.
        auto vReadAction = manager.getActionEvent(iAction1Id);
        REQUIRE(!vReadAction.empty());
        REQUIRE(vReadAction.size() == vExpectedAction1Keys.size());

        // Compare keys (order may be different).
        for (const auto& wantAction : vExpectedAction1Keys) {
            bool bFound = false;
            for (const auto& foundAction : vReadAction) {
                if (foundAction == wantAction) {
                    bFound = true;
                    break;
                }
            }
            REQUIRE(bFound);
        }

        // Action 2.
        vReadAction = manager.getActionEvent(iAction2Id);
        REQUIRE(!vReadAction.empty());
        REQUIRE(vReadAction.size() == vExpectedAction2Keys.size());

        // Compare keys (order may be different).
        for (const auto& wantAction : vExpectedAction2Keys) {
            bool bFound = false;
            for (const auto& foundAction : vReadAction) {
                if (foundAction == wantAction) {
                    bFound = true;
                    break;
                }
            }
            REQUIRE(bFound);
        }

        // Axis 1.
        auto vReadAxis = manager.getAxisEvent(iAxis1Id);
        REQUIRE(!vReadAxis.empty());
        REQUIRE(vReadAxis.size() == vExpectedAxis1Keys.size());

        // Compare keys (order may be different).
        for (const auto& wantAxis : vExpectedAxis1Keys) {
            bool bFound = false;
            for (const auto& foundAxis : vReadAxis) {
                if (foundAxis == wantAxis) {
                    bFound = true;
                    break;
                }
            }
            REQUIRE(bFound);
        }
    }
}

TEST_CASE("is key used") {
    using namespace ne;

    const unsigned int iAction1Id = 0;
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction1Keys = {
        KeyboardKey::KEY_0, KeyboardKey::KEY_Z};

    const unsigned int iAction2Id = 1;
    const std::vector<std::variant<KeyboardKey, MouseButton>> vAction2Keys = {KeyboardKey::KEY_LEFT};

    const unsigned int iAxis2Id = 0;
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxes2 = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_R, KeyboardKey::KEY_A),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_RIGHT, KeyboardKey::KEY_LEFT)};

    InputManager manager;
    auto optional = manager.addActionEvent(iAction1Id, vAction1Keys);
    REQUIRE(!optional.has_value());
    optional = manager.addActionEvent(iAction2Id, vAction2Keys);
    REQUIRE(!optional.has_value());
    optional = manager.addAxisEvent(iAxis2Id, vAxes2);
    REQUIRE(!optional.has_value());

    // First test.
    auto [vActionEvent1Ids, vAxisEvents] = manager.isKeyUsed(KeyboardKey::KEY_LEFT);
    REQUIRE(vActionEvent1Ids.size() == 1);
    REQUIRE(vAxisEvents.size() == 1);

    // Find registered action event.
    bool bFound = false;
    for (const auto& iFoundActionId : vActionEvent1Ids) {
        if (iFoundActionId == iAction2Id) {
            bFound = true;
            break;
        }
    }
    REQUIRE(bFound);

    // Find registered axis event.
    for (const auto& iFoundActionId : vAxisEvents) {
        if (iFoundActionId == iAxis2Id) {
            bFound = true;
            break;
        }
    }
    REQUIRE(bFound);

    // Another test.
    auto [vActionEvent2Ids, vAxisEvents2] = manager.isKeyUsed(KeyboardKey::KEY_0);
    REQUIRE(vActionEvent2Ids.size() == 1);
    REQUIRE(vAxisEvents2.empty());

    // Find registered action.
    bFound = false;
    for (const auto& iFoundActionId : vActionEvent2Ids) {
        if (iFoundActionId == iAction1Id) {
            bFound = true;
            break;
        }
    }
    REQUIRE(bFound);
}
