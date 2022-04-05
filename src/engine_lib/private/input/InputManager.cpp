#include "input/InputManager.h"

// Custom.
#include "io/Logger.h"

namespace ne {
    void InputManager::addActionEvent(const std::string &sActionName,
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
    }

    void InputManager::addAxisEvent(const std::string &sAxisName,
                                    const std::vector<std::pair<KeyboardKey, KeyboardKey>> &vAxis) {
        std::scoped_lock<std::recursive_mutex> guard(mtxAxisEvents);

        // Remove all axis with this name if exists.
        removeAxisEvent(sAxisName);

        // Add plus key.
        for (const auto &pair : vAxis) {
            auto it = axisEvents.find(pair.first);
            if (it == axisEvents.end()) {
                axisEvents[pair.first] = {std::make_pair<std::string, int>(sAxisName.data(), 1)};
            } else {
                it->second.insert(std::make_pair<std::string, int>(sAxisName.data(), 1));
            }
        }

        // Add minus key.
        for (const auto &pair : vAxis) {
            auto it = axisEvents.find(pair.second);
            if (it == axisEvents.end()) {
                axisEvents[pair.second] = {std::make_pair<std::string, int>(sAxisName.data(), -1)};
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

        std::vector<KeyboardKey> vPlusKeys;
        std::vector<KeyboardKey> vMinusKeys;

        // Find plus/minus keys.
        for (const auto &pair : axisEvents) {
            auto plusIt = pair.second.find(std::make_pair<std::string, int>(sAxisName.data(), 1));
            auto minusIt = pair.second.find(std::make_pair<std::string, int>(sAxisName.data(), -1));

            if (plusIt != pair.second.end()) {
                vPlusKeys.push_back(pair.first);
            }
            if (minusIt != pair.second.end()) {
                vMinusKeys.push_back(pair.first);
            }
        }

        if (vPlusKeys.empty() && vMinusKeys.empty()) {
            return {};
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

        // Get all axis names first.
        for (const auto &axisPair : axisEvents) {
            for (const std::pair<std::string, int> &pair : axisPair.second) {
                if (!axes.contains(pair.first)) {
                    axes[pair.first] = {};
                }
            }
        }

        // Fill input for those axis.
        for (const auto &axisPair : axisEvents) {
            for (const std::pair<std::string, int> &pair : axisPair.second) {
                auto option = getAxisEvent(pair.first);
                if (option.has_value()) {
                    axes[pair.first] = std::move(option.value());
                } else {
                    return axes;
                }
            }
        }

        return axes;
    }
} // namespace ne
