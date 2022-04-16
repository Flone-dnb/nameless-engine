#include "input/InputManager.h"

// STL.
#include <ranges>

// Custom.
#include "io/Logger.h"
#include "io/ConfigManager.h"

namespace ne {
    std::optional<Error>
    InputManager::addActionEvent(const std::string &sActionName,
                                 const std::vector<std::variant<KeyboardKey, MouseButton>> &vKeys) {
        if (vKeys.empty()) {
            return Error("vKeys is empty");
        }

        std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

        // Check if an action with this name already exists.
        auto vRegisteredActions = getAllActionEvents();

        const auto action = vRegisteredActions.find(sActionName);
        if (action != vRegisteredActions.end()) {
            return Error(std::format("an action with the name '{}' already exists", sActionName));
        }

        auto optional = overwriteActionEvent(sActionName, vKeys);
        if (optional.has_value()) {
            optional->addEntry();
            return std::move(optional.value());
        }
        return {};
    }

    std::optional<Error>
    InputManager::addAxisEvent(const std::string &sAxisName,
                               const std::vector<std::pair<KeyboardKey, KeyboardKey>> &vAxis) {
        if (vAxis.empty()) {
            return Error("vAxis is empty");
        }

        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        // Check if an axis event with this name already exists.
        auto vRegisteredAxisEvents = getAllAxisEvents();

        const auto action = vRegisteredAxisEvents.find(sAxisName);
        if (action != vRegisteredAxisEvents.end()) {
            return Error(std::format("an axis event with the name '{}' already exists", sAxisName));
        }

        auto optional = overwriteAxisEvent(sAxisName, vAxis);
        if (optional.has_value()) {
            optional->addEntry();
            return std::move(optional.value());
        }
        return {};
    }

    std::optional<Error> InputManager::modifyActionEventKey(const std::string &sActionName,
                                                            std::variant<KeyboardKey, MouseButton> oldKey,
                                                            std::variant<KeyboardKey, MouseButton> newKey) {
        std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

        // See if this action exists.
        auto actions = getAllActionEvents();
        const auto it = actions.find(sActionName);
        if (it == actions.end()) {
            return Error(std::format("no action with the name '{}' exists", sActionName));
        }

        auto vActionKeys = getActionEvent(sActionName).value();

        // Replace old key.
        std::ranges::replace(vActionKeys, oldKey, newKey);

        // Overwrite event with new keys.
        auto optional = overwriteActionEvent(sActionName, vActionKeys);
        if (optional.has_value()) {
            optional->addEntry();
            return std::move(optional.value());
        }
        return {};
    }

    std::optional<Error> InputManager::modifyAxisEventKey(const std::string &sAxisName,
                                                          std::pair<KeyboardKey, KeyboardKey> oldPair,
                                                          std::pair<KeyboardKey, KeyboardKey> newPair) {
        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        // See if this axis exists.
        auto axes = getAllAxisEvents();
        const auto it = axes.find(sAxisName);
        if (it == axes.end()) {
            return Error(std::format("no axis with the name '{}' exists", sAxisName));
        }

        auto vAxisKeys = getAxisEvent(sAxisName).value();

        // See if old key exists.
        const auto foundIt =
            std::ranges::find_if(vAxisKeys, [&](const auto &pair) { return pair == oldPair; });
        if (foundIt == vAxisKeys.end()) {
            return Error("the specified old key pair was not found");
        }

        // Replace old key.
        std::ranges::replace(vAxisKeys, oldPair, newPair);

        // Overwrite event with new keys.
        auto optional = overwriteAxisEvent(sAxisName, vAxisKeys);
        if (optional.has_value()) {
            optional->addEntry();
            return std::move(optional.value());
        }

        return {};
    }

    std::optional<Error> InputManager::saveToFile(std::string_view sFileName) {
        const auto allActionEvents = getAllActionEvents();
        const auto allAxisEvents = getAllAxisEvents();

        ConfigManager manager;

        // Action events.
        for (const auto &[sActionName, vActionKeys] : allActionEvents) {
            std::string sActionKeysText;

            // Put all keys in a string.
            for (const auto &key : vActionKeys) {
                if (std::holds_alternative<KeyboardKey>(key)) {
                    sActionKeysText +=
                        std::format("k{},", std::to_string(static_cast<int>(std::get<KeyboardKey>(key))));
                } else {
                    sActionKeysText +=
                        std::format("m{},", std::to_string(static_cast<int>(std::get<MouseButton>(key))));
                }
            }

            sActionKeysText.pop_back(); // pop comma

            manager.setStringValue(sActionEventSectionName, sActionName, sActionKeysText);
        }

        // Axis events.
        for (const auto &[sAxisName, vAxisKeys] : allAxisEvents) {
            std::string sAxisKeysText;

            // Put all keys in a string.
            for (const auto &pair : vAxisKeys) {
                sAxisKeysText += std::format("{}-{},", std::to_string(static_cast<int>(pair.first)),
                                             std::to_string(static_cast<int>(pair.second)));
            }

            sAxisKeysText.pop_back(); // pop comma

            manager.setStringValue(sAxisEventSectionName, sAxisName, sAxisKeysText);
        }

        auto optional = manager.saveFile(ConfigCategory::SETTINGS, sFileName);
        if (optional.has_value()) {
            optional->addEntry();
            return std::move(optional.value());
        }
        return {};
    }

    std::optional<Error> InputManager::loadFromFile(std::string_view sFileName) {
        ConfigManager manager;
        auto optional = manager.loadFile(ConfigCategory::SETTINGS, sFileName);
        if (optional.has_value()) {
            optional->addEntry();
            return optional;
        }

        // Read sections.
        const auto vSections = manager.getAllSections();
        if (vSections.empty()) {
            return Error(std::format("the specified file '{}' has no sections", sFileName));
        }

        // Check that we only have 1 or 2 sections.
        if (vSections.size() > 2) {
            return Error(std::format("the specified file '{}' has {} sections, "
                                     "while expected only 1 or 2 sections",
                                     sFileName, vSections.size()));
        }

        // Check section names.
        bool bHasActionEventsSection = false;
        bool bHasAxisEventsSection = false;
        for (const auto &sSectionName : vSections) {
            if (sSectionName == sActionEventSectionName) {
                bHasActionEventsSection = true;
            } else if (sSectionName == sAxisEventSectionName) {
                bHasAxisEventsSection = true;
            } else {
                return Error(std::format("section '{}' has unexpected name", sSectionName));
            }
        }

        // Add action events.
        if (bHasActionEventsSection) {
            auto variant = manager.getAllKeysOfSection(sActionEventSectionName);
            if (std::holds_alternative<Error>(variant)) {
                auto error = std::get<Error>(std::move(variant));
                error.addEntry();
                return error;
            }
            auto fileActionEvents = std::get<std::vector<std::string>>(std::move(variant));
            auto currentActionEvents = getAllActionEvents();

            std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);
            for (const auto &sActionName : currentActionEvents | std::views::keys) {
                // Look for this action in file.
                auto it = std::ranges::find(fileActionEvents, sActionName);
                if (it == fileActionEvents.end()) {
                    continue;
                }

                // Read keys from this action.
                auto keys = manager.getStringValue(sActionEventSectionName, sActionName, "");
                if (keys.empty()) {
                    continue;
                }

                // Split string and process each key.
                auto range = keys | std::views::split(',');
                std::vector<std::string> vActionKeys{range.begin(), range.end()};
                std::vector<std::variant<KeyboardKey, MouseButton>> vOutActionKeys;
                for (const auto &key : vActionKeys) {
                    if (key[0] == 'k') {
                        // KeyboardKey.
                        int iKeyboardKey = -1;
                        auto keyString = key.substr(1);
                        auto [ptr, ec] = std::from_chars(keyString.data(),
                                                         keyString.data() + keyString.size(), iKeyboardKey);
                        if (ec != std::errc()) {
                            return Error(
                                std::format("failed to convert '{}' to keyboard key code (error code: {})",
                                            keyString, static_cast<int>(ec)));
                        }

                        vOutActionKeys.push_back(static_cast<KeyboardKey>(iKeyboardKey));
                    } else if (key[0] == 'm') {
                        // MouseButton.
                        int iMouseButton = -1;
                        auto keyString = key.substr(1);
                        auto [ptr, ec] = std::from_chars(keyString.data(),
                                                         keyString.data() + keyString.size(), iMouseButton);
                        if (ec != std::errc()) {
                            return Error(
                                std::format("failed to convert '{}' to mouse button code (error code: {})",
                                            keyString, static_cast<int>(ec)));
                        }

                        vOutActionKeys.push_back(static_cast<MouseButton>(iMouseButton));
                    }
                }

                // Add keys (replace old ones).
                optional = overwriteActionEvent(sActionName, vOutActionKeys);
                if (optional.has_value()) {
                    optional->addEntry();
                    return std::move(optional.value());
                }
            }
        }

        // Add axis events.
        if (bHasAxisEventsSection) {
            auto variant = manager.getAllKeysOfSection(sAxisEventSectionName);
            if (std::holds_alternative<Error>(variant)) {
                auto error = std::get<Error>(std::move(variant));
                error.addEntry();
                return error;
            }
            auto fileAxisEvents = std::get<std::vector<std::string>>(std::move(variant));
            auto currentAxisEvents = getAllAxisEvents();

            std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);
            for (const auto &sAxisName : currentAxisEvents | std::views::keys) {
                // Look for this axis in file.
                auto it = std::ranges::find(fileAxisEvents, sAxisName);
                if (it == fileAxisEvents.end()) {
                    continue;
                }

                // Read keys from this axis.
                auto keys = manager.getStringValue(sAxisEventSectionName, sAxisName, "");
                if (keys.empty()) {
                    continue;
                }

                // Split string and process each key.
                auto range = keys | std::views::split(',');
                std::vector<std::string> vAxisKeys{range.begin(), range.end()};
                std::vector<std::pair<KeyboardKey, KeyboardKey>> vOutAxisKeys;
                for (const auto &key : vAxisKeys) {
                    auto plusMinusRange = key | std::views::split('-');
                    std::vector<std::string> vPlusMinusKeys{plusMinusRange.begin(), plusMinusRange.end()};

                    if (vPlusMinusKeys.size() != 2) {
                        return Error(std::format("axis entry '{}' does not have 2 keys", key));
                    }

                    // Convert the first one.
                    int iKeyboardPlusKey = -1;
                    auto [ptr1, ec1] = std::from_chars(vPlusMinusKeys[0].data(),
                                                       vPlusMinusKeys[0].data() + vPlusMinusKeys[0].size(),
                                                       iKeyboardPlusKey);
                    if (ec1 != std::errc()) {
                        return Error(std::format("failed to convert the first key of axis entry '{}' "
                                                 "to keyboard key code (error code: {})",
                                                 key, static_cast<int>(ec1)));
                    }

                    // Convert the second one.
                    int iKeyboardMinusKey = -1;
                    auto [ptr2, ec2] = std::from_chars(vPlusMinusKeys[1].data(),
                                                       vPlusMinusKeys[1].data() + vPlusMinusKeys[1].size(),
                                                       iKeyboardMinusKey);
                    if (ec2 != std::errc()) {
                        return Error(std::format("failed to convert the second key of axis entry '{}' "
                                                 "to keyboard key code (error code: {})",
                                                 key, static_cast<int>(ec2)));
                    }

                    vOutAxisKeys.push_back(std::make_pair<KeyboardKey, KeyboardKey>(
                        static_cast<KeyboardKey>(iKeyboardPlusKey),
                        static_cast<KeyboardKey>(iKeyboardMinusKey)));
                }

                // Add keys (replace old ones).
                optional = overwriteAxisEvent(sAxisName, vOutAxisKeys);
                if (optional.has_value()) {
                    optional->addEntry();
                    return std::move(optional.value());
                }
            }
        }

        return {};
    }

    std::pair<std::set<std::string>, std::set<std::string>>
    InputManager::isKeyUsed(std::variant<KeyboardKey, MouseButton> key) {
        std::scoped_lock<std::recursive_mutex> guard1(mtxActionEvents);
        std::scoped_lock<std::recursive_mutex> guard2(mtxAxisEvents);

        std::set<std::string> vUsedActionEvents;
        std::set<std::string> vUsedAxisEvents;

        // Action events.
        const auto actionIt = actionEvents.find(key);
        if (actionIt != actionEvents.end()) {
            vUsedActionEvents = actionIt->second;
        }

        // Axis events.
        if (std::holds_alternative<KeyboardKey>(key)) {
            const auto keyboardKey = std::get<KeyboardKey>(key);
            const auto axisIt = axisEvents.find(keyboardKey);
            if (axisIt != axisEvents.end()) {
                for (const auto &sAxisName : axisIt->second | std::views::keys) {
                    vUsedAxisEvents.insert(sAxisName);
                }
            }
        }

        return std::make_pair(vUsedActionEvents, vUsedAxisEvents);
    }

    std::optional<std::vector<std::variant<KeyboardKey, MouseButton>>>
    InputManager::getActionEvent(const std::string &sActionName) {
        std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

        std::vector<std::variant<KeyboardKey, MouseButton>> vKeys;

        // Look if this action exists, get all keys.
        for (const auto &pair : actionEvents) {
            auto foundIt = pair.second.find(sActionName);
            if (foundIt == pair.second.end()) {
                // Not found.
                continue;
            }
            vKeys.push_back(pair.first);
        }

        if (vKeys.empty()) {
            return {};
        } else {
            return vKeys;
        }
    }

    std::optional<std::vector<std::pair<KeyboardKey, KeyboardKey>>>
    InputManager::getAxisEvent(const std::string &sAxisName) {
        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxis;

        // Find only plus keys.
        std::vector<KeyboardKey> vPlusKeys;
        for (const auto &pair : axisEvents) {
            auto plusIt = pair.second.find(std::make_pair<std::string, int>(sAxisName.data(), 1));

            if (plusIt != pair.second.end()) {
                vPlusKeys.push_back(pair.first);
            }
        }

        if (vPlusKeys.empty()) {
            return {};
        }

        // Add correct minus pair to each plus key using info from states.
        std::vector<KeyboardKey> vMinusKeys;
        const std::pair<std::vector<AxisState>, int> currentState = axisState[sAxisName];
        std::vector<AxisState> pairs = currentState.first;
        for (const auto &plusKey : vPlusKeys) {
            auto it = std::ranges::find_if(pairs.begin(), pairs.end(),
                                           [&](const AxisState &item) { return item.plusKey == plusKey; });
            if (it == pairs.end()) {
                Logger::get().error(
                    std::format("can't find minus key for plus key in axis event '{}'", sAxisName));
                return {};
            }

            vMinusKeys.push_back(it->minusKey);
        }

        // Check sizes.
        if (vPlusKeys.size() != vMinusKeys.size()) {
            Logger::get().error(std::format(
                "not equal size of plus and minus keys, found {} plus key(-s) and {} minus(-s) keys "
                "for axis event {}",
                vPlusKeys.size(), vMinusKeys.size(), sAxisName));
            return {};
        }

        // Fill axis.
        for (size_t i = 0; i < vPlusKeys.size(); i++) {
            vAxis.push_back(
                std::make_pair<KeyboardKey, KeyboardKey>(std::move(vPlusKeys[i]), std::move(vMinusKeys[i])));
        }

        return vAxis;
    }

    float InputManager::getCurrentAxisEventValue(const std::string &sAxisName) {
        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        const auto stateIt = axisState.find(sAxisName);
        if (stateIt == axisState.end()) {
            return 0.0f;
        }

        return static_cast<float>(stateIt->second.second);
    }

    bool InputManager::getCurrentActionEventValue(const std::string &sActionName) {
        std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

        const auto stateIt = actionState.find(sActionName);
        if (stateIt == actionState.end()) {
            return false;
        }

        return stateIt->second.second;
    }

    bool InputManager::removeActionEvent(const std::string &sActionName) {
        std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

        bool bFound = false;

        // Look if this action exists and remove all entries.
        for (auto it = actionEvents.begin(); it != actionEvents.end();) {
            auto foundIt = it->second.find(sActionName);
            if (foundIt == it->second.end()) {
                // Not found.
                ++it;
                continue;
            }
            bFound = true;
            it->second.erase(foundIt);

            if (it->second.empty()) {
                it = actionEvents.erase(it);
            } else {
                ++it;
            }
        }

        // Remove action state.
        const auto it = actionState.find(sActionName);
        if (it != actionState.end()) {
            actionState.erase(it);
        }

        return !bFound;
    }

    bool InputManager::removeAxisEvent(const std::string &sAxisName) {
        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        bool bFound = false;

        // Look for plus and minus keys.
        for (auto it = axisEvents.begin(); it != axisEvents.end();) {
            auto plusIt = it->second.find(std::make_pair<std::string, int>(sAxisName.data(), 1));
            auto minusIt = it->second.find(std::make_pair<std::string, int>(sAxisName.data(), -1));
            if (plusIt == it->second.end() && minusIt == it->second.end()) {
                // Not found.
                ++it;
                continue;
            }
            bFound = true;

            if (plusIt != it->second.end()) {
                it->second.erase(plusIt);
            }
            if (minusIt != it->second.end()) {
                it->second.erase(minusIt);
            }

            if (it->second.empty()) {
                it = axisEvents.erase(it);
            } else {
                ++it;
            }
        }

        // Remove axis state.
        const auto it = axisState.find(sAxisName);
        if (it != axisState.end()) {
            axisState.erase(it);
        }

        return !bFound;
    }

    std::unordered_map<std::string, std::vector<std::variant<KeyboardKey, MouseButton>>>
    InputManager::getAllActionEvents() {
        std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

        std::unordered_map<std::string, std::vector<std::variant<KeyboardKey, MouseButton>>> actions;

        // Get all action names first.
        for (const auto &actionPair : actionEvents) {
            for (const auto &sName : actionPair.second) {
                if (!actions.contains(sName)) {
                    actions[sName] = {};
                }
            }
        }

        // Fill keys for those actions.
        for (const auto &actionPair : actionEvents) {
            for (const auto &actionName : actionPair.second) {
                actions[actionName].push_back(actionPair.first);
            }
        }

        return actions;
    }

    std::unordered_map<std::string, std::vector<std::pair<KeyboardKey, KeyboardKey>>>
    InputManager::getAllAxisEvents() {
        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        std::unordered_map<std::string, std::vector<std::pair<KeyboardKey, KeyboardKey>>> axes;

        for (const auto &keyAxisNames : axisEvents | std::views::values) {
            for (const auto &sAxisName : keyAxisNames | std::views::keys) {
                if (!axes.contains(sAxisName)) {
                    // Get keys.
                    auto option = getAxisEvent(sAxisName);
                    if (option.has_value()) {
                        axes[sAxisName] = std::move(option.value());
                    } else {
                        axes[sAxisName] = {};
                        Logger::get().error(std::format("no axis event found by name '{}'", sAxisName));
                    }
                }
            }
        }

        return axes;
    }

    std::optional<Error>
    InputManager::overwriteActionEvent(const std::string &sActionName,
                                       const std::vector<std::variant<KeyboardKey, MouseButton>> &vKeys) {
        std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

        // Remove all keys with this action if exists.
        removeActionEvent(sActionName);

        // Add keys for actions.
        for (const auto &key : vKeys) {
            auto it = actionEvents.find(key);
            if (it == actionEvents.end()) {
                actionEvents[key] = {sActionName};
            } else {
                it->second.insert(sActionName);
            }
        }

        // Add state.
        std::vector<ActionState> vActionState;
        for (const auto &action : vKeys) {
            vActionState.push_back(ActionState(action));
        }
        actionState[sActionName] =
            std::make_pair<std::vector<ActionState>, bool>(std::move(vActionState), false);

        return {};
    }

    std::optional<Error>
    InputManager::overwriteAxisEvent(const std::string &sAxisName,
                                     const std::vector<std::pair<KeyboardKey, KeyboardKey>> &vAxis) {
        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        // Remove all axis with this name if exists.
        removeAxisEvent(sAxisName);

        // Add keys.
        for (const auto &[plusKey, minusKey] : vAxis) {
            auto it = axisEvents.find(plusKey);
            if (it == axisEvents.end()) {
                axisEvents[plusKey] = {std::make_pair<std::string, int>(sAxisName.data(), 1)};
            } else {
                it->second.insert(std::make_pair<std::string, int>(sAxisName.data(), 1));
            }

            it = axisEvents.find(minusKey);
            if (it == axisEvents.end()) {
                axisEvents[minusKey] = {std::make_pair<std::string, int>(sAxisName.data(), -1)};
            } else {
                it->second.insert(std::make_pair<std::string, int>(sAxisName.data(), -1));
            }
        }

        // Add state.
        std::vector<AxisState> vAxisState;
        for (const auto &axis : vAxis) {
            vAxisState.push_back(AxisState(axis.first, axis.second));
        }
        axisState[sAxisName] = std::make_pair<std::vector<AxisState>, int>(std::move(vAxisState), 0);

        return {};
    }
} // namespace ne
