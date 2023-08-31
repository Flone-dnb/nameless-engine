#include "input/InputManager.h"

// Standard.
#include <charconv>
#include <string>
#include <ranges>
#include <format>
#include <vector>

// Custom.
#include "io/Logger.h"
#include "io/ConfigManager.h"

namespace ne {
    std::optional<Error> InputManager::addActionEvent(
        unsigned int iActionId, const std::vector<std::variant<KeyboardKey, MouseButton>>& vKeys) {
        // Make sure there is an least one key specified to trigger this event.
        if (vKeys.empty()) {
            return Error("vKeys is empty");
        }

        std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

        // Check if an action with this name already exists.
        const auto vRegisteredActions = getAllActionEvents();
        const auto existingActionId = vRegisteredActions.find(iActionId);
        if (existingActionId != vRegisteredActions.end()) {
            return Error(std::format("an action with the ID {} already exists", iActionId));
        }

        // Add action.
        auto optional = overwriteActionEvent(iActionId, vKeys);
        if (optional.has_value()) {
            optional->addCurrentLocationToErrorStack();
            return std::move(optional.value());
        }

        return {};
    }

    std::optional<Error> InputManager::addAxisEvent(
        unsigned int iAxisEventId, const std::vector<std::pair<KeyboardKey, KeyboardKey>>& vAxis) {
        // Make sure there is an least one key specified to trigger this event.
        if (vAxis.empty()) {
            return Error("vAxis is empty");
        }

        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        // Check if an axis event with this name already exists.
        auto vRegisteredAxisEvents = getAllAxisEvents();

        const auto action = vRegisteredAxisEvents.find(iAxisEventId);
        if (action != vRegisteredAxisEvents.end()) {
            return Error(std::format("an axis event with the ID {} already exists", iAxisEventId));
        }

        auto optional = overwriteAxisEvent(iAxisEventId, vAxis);
        if (optional.has_value()) {
            optional->addCurrentLocationToErrorStack();
            return std::move(optional.value());
        }
        return {};
    }

    std::optional<Error> InputManager::modifyActionEventKey(
        unsigned int iActionId,
        std::variant<KeyboardKey, MouseButton> oldKey,
        std::variant<KeyboardKey, MouseButton> newKey) {
        std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

        // See if this action exists.
        auto actions = getAllActionEvents();
        const auto it = actions.find(iActionId);
        if (it == actions.end()) {
            return Error(std::format("no action with the ID {} exists", iActionId));
        }

        auto vActionKeys = getActionEvent(iActionId);

        // Replace old key.
        std::ranges::replace(vActionKeys, oldKey, newKey);

        // Overwrite event with new keys.
        auto optional = overwriteActionEvent(iActionId, vActionKeys);
        if (optional.has_value()) {
            optional->addCurrentLocationToErrorStack();
            return std::move(optional.value());
        }

        return {};
    }

    std::optional<Error> InputManager::modifyAxisEventKey(
        unsigned int iAxisEventId,
        std::pair<KeyboardKey, KeyboardKey> oldPair,
        std::pair<KeyboardKey, KeyboardKey> newPair) {
        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        // See if this axis exists.
        auto axes = getAllAxisEvents();
        const auto it = axes.find(iAxisEventId);
        if (it == axes.end()) {
            return Error(std::format("no axis event with the ID {} exists", iAxisEventId));
        }

        // Get keys of this event.
        auto vAxisKeys = getAxisEvent(iAxisEventId);

        // See if old key exists.
        const auto foundIt =
            std::ranges::find_if(vAxisKeys, [&](const auto& pair) { return pair == oldPair; });
        if (foundIt == vAxisKeys.end()) {
            return Error("the specified old key pair was not found");
        }

        // Replace old key.
        std::ranges::replace(vAxisKeys, oldPair, newPair);

        // Overwrite event with new keys.
        auto optional = overwriteAxisEvent(iAxisEventId, vAxisKeys);
        if (optional.has_value()) {
            optional->addCurrentLocationToErrorStack();
            return std::move(optional.value());
        }

        return {};
    }

    std::optional<Error> InputManager::saveToFile(std::string_view sFileName) {
        // Get a copy of all registered events.
        const auto allActionEvents = getAllActionEvents();
        const auto allAxisEvents = getAllAxisEvents();

        ConfigManager manager;

        // Action events.
        for (const auto& [iActionId, vActionKeys] : allActionEvents) {
            std::string sActionKeysText;

            // Put all keys in a string.
            for (const auto& key : vActionKeys) {
                if (std::holds_alternative<KeyboardKey>(key)) {
                    sActionKeysText +=
                        std::format("k{},", std::to_string(static_cast<int>(std::get<KeyboardKey>(key))));
                } else {
                    sActionKeysText +=
                        std::format("m{},", std::to_string(static_cast<int>(std::get<MouseButton>(key))));
                }
            }

            sActionKeysText.pop_back(); // pop comma

            // Set value.
            manager.setValue<std::string>(
                sActionEventSectionName, std::to_string(iActionId), sActionKeysText);
        }

        // Axis events.
        for (const auto& [iAxisEventId, vAxisKeys] : allAxisEvents) {
            std::string sAxisKeysText;

            // Put all keys in a string.
            for (const auto& pair : vAxisKeys) {
                sAxisKeysText += std::format(
                    "{}-{},",
                    std::to_string(static_cast<int>(pair.first)),
                    std::to_string(static_cast<int>(pair.second)));
            }

            sAxisKeysText.pop_back(); // pop comma

            manager.setValue<std::string>(sAxisEventSectionName, std::to_string(iAxisEventId), sAxisKeysText);
        }

        auto optional = manager.saveFile(ConfigCategory::SETTINGS, sFileName);
        if (optional.has_value()) {
            optional->addCurrentLocationToErrorStack();
            return std::move(optional.value());
        }
        return {};
    }

    std::optional<Error> InputManager::loadFromFile(std::string_view sFileName) { // NOLINT: too complex
        ConfigManager manager;
        auto optional = manager.loadFile(ConfigCategory::SETTINGS, sFileName);
        if (optional.has_value()) {
            optional->addCurrentLocationToErrorStack();
            return optional;
        }

        // Read sections.
        const auto vSections = manager.getAllSections();
        if (vSections.empty()) {
            return Error(std::format("the specified file '{}' has no sections", sFileName));
        }

        // Check that we only have 1 or 2 sections.
        if (vSections.size() > 2) {
            return Error(std::format(
                "the specified file '{}' has {} sections, "
                "while expected only 1 or 2 sections",
                sFileName,
                vSections.size()));
        }

        // Check section names.
        bool bHasActionEventsSection = false;
        bool bHasAxisEventsSection = false;
        for (const auto& sSectionName : vSections) {
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
                error.addCurrentLocationToErrorStack();
                return error;
            }
            auto vFileActionEventNames = std::get<std::vector<std::string>>(std::move(variant));

            // Convert action event names to uints.
            std::vector<unsigned int> vFileActionEvents;
            for (const auto& sNumber : vFileActionEventNames) {
                try {
                    // First convert as unsigned long and see if it's out of range.
                    const auto iUnsignedLong = std::stoul(sNumber);
                    if (iUnsignedLong > UINT_MAX) {
                        throw std::out_of_range(sNumber);
                    }

                    // Add new ID.
                    vFileActionEvents.push_back(static_cast<unsigned int>(iUnsignedLong));
                } catch (const std::exception& exception) {
                    return Error(
                        std::format("failed to convert \"{}\" to ID, error: {}", sNumber, exception.what()));
                }
            }

            // Get a copy of all registered action events.
            auto currentActionEvents = getAllActionEvents();

            std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

            for (const auto& [iActionId, value] : currentActionEvents) {
                // Look if this registered event exists in the events from file.
                auto it = std::ranges::find(vFileActionEvents, iActionId);
                if (it == vFileActionEvents.end()) {
                    // We don't have such action event registered so don't import keys.
                    continue;
                }

                // Read keys from this action.
                std::string keys =
                    manager.getValue<std::string>(sActionEventSectionName, std::to_string(iActionId), "");
                if (keys.empty()) {
                    continue;
                }

                // Split string and process each key.
                std::vector<std::string> vActionKeys = splitString(keys, ",");
                std::vector<std::variant<KeyboardKey, MouseButton>> vOutActionKeys;
                for (const auto& key : vActionKeys) {
                    if (key[0] == 'k') {
                        // KeyboardKey.
                        int iKeyboardKey = -1;
                        auto keyString = key.substr(1);
                        auto [ptr, ec] = std::from_chars(
                            keyString.data(), keyString.data() + keyString.size(), iKeyboardKey);
                        if (ec != std::errc()) {
                            return Error(std::format(
                                "failed to convert '{}' to keyboard key code (error code: {})",
                                keyString,
                                static_cast<int>(ec)));
                        }

                        vOutActionKeys.push_back(static_cast<KeyboardKey>(iKeyboardKey));
                    } else if (key[0] == 'm') {
                        // MouseButton.
                        int iMouseButton = -1;
                        auto keyString = key.substr(1);
                        auto [ptr, ec] = std::from_chars(
                            keyString.data(), keyString.data() + keyString.size(), iMouseButton);
                        if (ec != std::errc()) {
                            return Error(std::format(
                                "failed to convert '{}' to mouse button code (error code: {})",
                                keyString,
                                static_cast<int>(ec)));
                        }

                        vOutActionKeys.push_back(static_cast<MouseButton>(iMouseButton));
                    }
                }

                // Add keys (replace old ones).
                optional = overwriteActionEvent(iActionId, vOutActionKeys);
                if (optional.has_value()) {
                    optional->addCurrentLocationToErrorStack();
                    return std::move(optional.value());
                }
            }
        }

        // Add axis events.
        if (bHasAxisEventsSection) {
            auto variant = manager.getAllKeysOfSection(sAxisEventSectionName);
            if (std::holds_alternative<Error>(variant)) {
                auto error = std::get<Error>(std::move(variant));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            auto vFileAxisEventNames = std::get<std::vector<std::string>>(std::move(variant));

            // Convert axis event names to uints.
            std::vector<unsigned int> vFileAxisEvents;
            for (const auto& sNumber : vFileAxisEventNames) {
                try {
                    // First convert as unsigned long and see if it's out of range.
                    const auto iUnsignedLong = std::stoul(sNumber);
                    if (iUnsignedLong > UINT_MAX) {
                        throw std::out_of_range(sNumber);
                    }

                    // Add new ID.
                    vFileAxisEvents.push_back(static_cast<unsigned int>(iUnsignedLong));
                } catch (const std::exception& exception) {
                    return Error(
                        std::format("failed to convert \"{}\" to ID, error: {}", sNumber, exception.what()));
                }
            }

            // Get a copy of all registered axis events.
            const auto currentAxisEvents = getAllAxisEvents();

            std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

            for (const auto& [iAxisEventId, value] : currentAxisEvents) {
                // Look for this axis in file.
                auto it = std::ranges::find(vFileAxisEvents, iAxisEventId);
                if (it == vFileAxisEvents.end()) {
                    continue;
                }

                // Read keys from this axis.
                std::string keys =
                    manager.getValue<std::string>(sAxisEventSectionName, std::to_string(iAxisEventId), "");
                if (keys.empty()) {
                    continue;
                }

                // Split string and process each key.
                std::vector<std::string> vAxisKeys = splitString(keys, ",");
                std::vector<std::pair<KeyboardKey, KeyboardKey>> vOutAxisKeys;
                for (const auto& key : vAxisKeys) {
                    std::vector<std::string> vPlusMinusKeys = splitString(key, "-");

                    if (vPlusMinusKeys.size() != 2) {
                        return Error(std::format("axis entry '{}' does not have 2 keys", key));
                    }

                    // Convert the first one.
                    int iKeyboardPlusKey = -1;
                    auto [ptr1, ec1] = std::from_chars(
                        vPlusMinusKeys[0].data(),
                        vPlusMinusKeys[0].data() + vPlusMinusKeys[0].size(),
                        iKeyboardPlusKey);
                    if (ec1 != std::errc()) {
                        return Error(std::format(
                            "failed to convert the first key of axis entry '{}' "
                            "to keyboard key code (error code: {})",
                            key,
                            static_cast<int>(ec1)));
                    }

                    // Convert the second one.
                    int iKeyboardMinusKey = -1;
                    auto [ptr2, ec2] = std::from_chars(
                        vPlusMinusKeys[1].data(),
                        vPlusMinusKeys[1].data() + vPlusMinusKeys[1].size(),
                        iKeyboardMinusKey);
                    if (ec2 != std::errc()) {
                        return Error(std::format(
                            "failed to convert the second key of axis entry '{}' "
                            "to keyboard key code (error code: {})",
                            key,
                            static_cast<int>(ec2)));
                    }

                    vOutAxisKeys.push_back(std::make_pair<KeyboardKey, KeyboardKey>(
                        static_cast<KeyboardKey>(iKeyboardPlusKey),
                        static_cast<KeyboardKey>(iKeyboardMinusKey)));
                }

                // Add keys (replace old ones).
                optional = overwriteAxisEvent(iAxisEventId, vOutAxisKeys);
                if (optional.has_value()) {
                    optional->addCurrentLocationToErrorStack();
                    return std::move(optional.value());
                }
            }
        }

        return {};
    }

    std::pair<std::vector<unsigned int>, std::vector<unsigned int>>
    InputManager::isKeyUsed(const std::variant<KeyboardKey, MouseButton>& key) {
        std::scoped_lock<std::recursive_mutex> guard1(mtxActionEvents);
        std::scoped_lock<std::recursive_mutex> guard2(mtxAxisEvents);

        std::vector<unsigned int> vUsedActionEvents;
        std::vector<unsigned int> vUsedAxisEvents;

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
                for (const auto& [iAxisId, value] : axisIt->second) {
                    vUsedAxisEvents.push_back(iAxisId);
                }
            }
        }

        return std::make_pair(vUsedActionEvents, vUsedAxisEvents);
    }

    std::vector<std::variant<KeyboardKey, MouseButton>> InputManager::getActionEvent(unsigned int iActionId) {
        std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

        std::vector<std::variant<KeyboardKey, MouseButton>> vKeys;

        // Look if this action exists, get all keys.
        for (const auto& [key, vEvents] : actionEvents) {
            for (const auto& iEventId : vEvents) {
                if (iEventId == iActionId) {
                    vKeys.push_back(key);
                }
            }
        }

        return vKeys;
    }

    std::vector<std::pair<KeyboardKey, KeyboardKey>> InputManager::getAxisEvent(unsigned int iAxisEventId) {
        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        std::vector<std::pair<KeyboardKey, KeyboardKey>> vAxisKeys;

        // Find only plus keys.
        std::vector<KeyboardKey> vPlusKeys;
        for (const auto& [key, vEvents] : axisEvents) {
            for (const auto& [iRegisteredAxisEventId, bTriggersPlusInput] : vEvents) {
                if (!bTriggersPlusInput) {
                    // Only looking at plus keys for now.
                    continue;
                }

                if (iRegisteredAxisEventId == iAxisEventId) {
                    vPlusKeys.push_back(key);
                }
            }
        }

        if (vPlusKeys.empty()) {
            return {};
        }

        // Add correct minus pair to each plus key using info from states.
        std::vector<KeyboardKey> vMinusKeys;
        const auto vAxisEventStates = axisState[iAxisEventId].first;

        for (const auto& plusKey : vPlusKeys) {
            // Find a state that has this plus key.
            auto it = std::ranges::find_if(
                vAxisEventStates.begin(), vAxisEventStates.end(), [&](const AxisState& item) {
                    return item.plusKey == plusKey;
                });
            if (it == vAxisEventStates.end()) [[unlikely]] {
                Logger::get().error(
                    std::format("can't find minus key for plus key in axis event with ID {}", iAxisEventId));
                return {};
            }

            // Add found minus key from the state.
            vMinusKeys.push_back(it->minusKey);
        }

        // Check sizes.
        if (vPlusKeys.size() != vMinusKeys.size()) [[unlikely]] {
            Logger::get().error(std::format(
                "not equal size of plus and minus keys, found {} plus key(s) and {} minus(s) keys "
                "for axis event with ID {}",
                vPlusKeys.size(),
                vMinusKeys.size(),
                iAxisEventId));
            return {};
        }

        // Fill axis.
        for (size_t i = 0; i < vPlusKeys.size(); i++) {
            vAxisKeys.push_back(std::make_pair(vPlusKeys[i], vMinusKeys[i]));
        }

        return vAxisKeys;
    }

    float InputManager::getCurrentAxisEventState(unsigned int iAxisEventId) {
        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        // Find the specified axis event by ID.
        const auto stateIt = axisState.find(iAxisEventId);
        if (stateIt == axisState.end()) {
            return 0.0F;
        }

        return static_cast<float>(stateIt->second.second);
    }

    bool InputManager::removeActionEvent(unsigned int iActionId) {
        std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

        bool bFound = false;

        // Look if this action exists and remove all entries.
        for (auto it = actionEvents.begin(); it != actionEvents.end();) {
            // Save the number of events right now.
            const auto iSizeBefore = it->second.size();

            // Remove all events with the specified ID.
            const auto [first, last] = std::ranges::remove_if(
                it->second, [&](const auto& iExistingActionId) { return iExistingActionId == iActionId; });
            it->second.erase(first, last);

            // Get the number of events right now.
            const auto iSizeAfter = it->second.size();

            // See if we removed something.
            if (iSizeAfter < iSizeBefore) {
                // Only change to `true`, don't do: `bFound = iSizeAfter < iSizeBefore` as this might
                // change `bFound` from `true` to `false`.
                bFound = true;
            }

            // Check if we need to remove this key from map.
            if (it->second.empty()) {
                // Remove key for this event as there are no events registered to this key.
                it = actionEvents.erase(it);
            } else {
                ++it;
            }
        }

        // Remove action state.
        const auto it = actionState.find(iActionId);
        if (it != actionState.end()) {
            actionState.erase(it);
        }

        return !bFound;
    }

    bool InputManager::removeAxisEvent(unsigned int iAxisEventId) {
        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        bool bFound = false;

        // Look for plus and minus keys.
        for (auto it = axisEvents.begin(); it != axisEvents.end();) {
            // Save the number of events right now.
            const auto iSizeBefore = it->second.size();

            // Remove all events with the specified ID.
            const auto [first, last] = std::ranges::remove_if(
                it->second, [&](const auto& pair) { return pair.first == iAxisEventId; });
            it->second.erase(first, last);

            // Get the number of events right now.
            const auto iSizeAfter = it->second.size();

            // See if we removed something.
            if (iSizeAfter < iSizeBefore) {
                // Only change to `true`, don't do: `bFound = iSizeAfter < iSizeBefore` as this might
                // change `bFound` from `true` to `false`.
                bFound = true;
            }

            // Check if we need to remove this key from map.
            if (it->second.empty()) {
                // Remove key for this event as there are no events registered to this key.
                it = axisEvents.erase(it);
            } else {
                ++it;
            }
        }

        // Remove axis state.
        const auto it = axisState.find(iAxisEventId);
        if (it != axisState.end()) {
            axisState.erase(it);
        }

        return !bFound;
    }

    std::unordered_map<unsigned int, std::vector<std::variant<KeyboardKey, MouseButton>>>
    InputManager::getAllActionEvents() {
        std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

        std::unordered_map<unsigned int, std::vector<std::variant<KeyboardKey, MouseButton>>> actions;

        // Get all action names first.
        for (const auto& [key, sActionName] : actionEvents) {
            for (const auto& sName : sActionName) {
                if (!actions.contains(sName)) {
                    actions[sName] = {};
                }
            }
        }

        // Fill keys for those actions.
        for (const auto& actionPair : actionEvents) {
            for (const auto& actionName : actionPair.second) {
                actions[actionName].push_back(actionPair.first);
            }
        }

        return actions;
    }

    std::unordered_map<unsigned int, std::vector<std::pair<KeyboardKey, KeyboardKey>>>
    InputManager::getAllAxisEvents() {
        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        std::unordered_map<unsigned int, std::vector<std::pair<KeyboardKey, KeyboardKey>>> axes;

        for (const auto& [key, keyAxisNames] : axisEvents) {
            for (const auto& [iAxisId, value] : keyAxisNames) {
                if (axes.contains(iAxisId)) {
                    continue;
                }

                // Get keys of this axis event.
                auto vKeys = getAxisEvent(iAxisId);
                if (!vKeys.empty()) {
                    axes[iAxisId] = std::move(vKeys);
                } else {
                    axes[iAxisId] = {};
                    Logger::get().error(std::format("no axis event found by ID {}", iAxisId));
                }
            }
        }

        return axes;
    }

    std::vector<std::string>
    InputManager::splitString(const std::string& sStringToSplit, const std::string& sDelimiter) {
        std::vector<std::string> vResult;
        size_t last = 0;
        size_t next = 0;

        while ((next = sStringToSplit.find(sDelimiter, last)) != std::string::npos) {
            vResult.push_back(sStringToSplit.substr(last, next - last));
            last = next + 1;
        }

        vResult.push_back(sStringToSplit.substr(last));
        return vResult;
    }

    std::optional<Error> InputManager::overwriteActionEvent(
        unsigned int iActionId, const std::vector<std::variant<KeyboardKey, MouseButton>>& vKeys) {
        std::scoped_lock<std::recursive_mutex> guard(mtxActionEvents);

        // Remove all keys with this action if exists.
        removeActionEvent(iActionId);

        // Add keys for actions.
        std::vector<ActionState> vActionState;
        for (const auto& key : vKeys) {
            // Find action events that use this key.
            auto it = actionEvents.find(key);
            if (it == actionEvents.end()) {
                actionEvents[key] = {iActionId};
            } else {
                it->second.push_back(iActionId);
            }

            vActionState.push_back(ActionState(key));
        }

        // Add/overwrite state.
        actionState[iActionId] =
            std::make_pair<std::vector<ActionState>, bool>(std::move(vActionState), false);

        return {};
    }

    std::optional<Error> InputManager::overwriteAxisEvent(
        unsigned int iAxisEventId, const std::vector<std::pair<KeyboardKey, KeyboardKey>>& vAxis) {
        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        // Remove all axis with this name if exists.
        removeAxisEvent(iAxisEventId);

        // Add keys.
        std::vector<AxisState> vAxisState;
        for (const auto& [plusKey, minusKey] : vAxis) {
            // Find axis events that use this plus key.
            auto it = axisEvents.find(plusKey);
            if (it == axisEvents.end()) {
                axisEvents[plusKey] = std::vector<std::pair<unsigned int, bool>>{{iAxisEventId, true}};
            } else {
                it->second.push_back(std::pair<unsigned int, bool>{iAxisEventId, true});
            }

            // Find axis events that use this minus key.
            it = axisEvents.find(minusKey);
            if (it == axisEvents.end()) {
                axisEvents[minusKey] = std::vector<std::pair<unsigned int, bool>>{{iAxisEventId, false}};
            } else {
                it->second.push_back(std::pair<unsigned int, bool>(iAxisEventId, false));
            }

            // Add new keys to states.
            vAxisState.push_back(AxisState(plusKey, minusKey));
        }

        // Add/overwrite event state.
        axisState[iAxisEventId] = std::make_pair<std::vector<AxisState>, int>(std::move(vAxisState), 0);

        return {};
    }
} // namespace ne
