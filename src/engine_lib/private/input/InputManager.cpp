#include "input/InputManager.h"

namespace ne {
    void InputManager::addActionEvent(const std::string &sActionName,
                                      const std::vector<std::variant<KeyboardKey, MouseButton>> &vKeys) {
        // Remove all keys with this action if exists.
        removeActionEvent(sActionName);

        for (const auto &key : vKeys) {
            auto it = actionEvents.find(key);
            if (it == actionEvents.end()) {
                actionEvents[key] = {sActionName};
            } else {
                it->second.insert(sActionName);
            }
        }
    }

    std::optional<std::vector<std::variant<KeyboardKey, MouseButton>>>
    InputManager::getActionEvent(const std::string &sActionName) const {
        std::vector<std::variant<KeyboardKey, MouseButton>> vKeys;

        // Look if this action exists get all keys.
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

    bool InputManager::removeActionEvent(const std::string &sActionName) {
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

        return !bFound;
    }

    std::unordered_map<std::string, std::vector<std::variant<KeyboardKey, MouseButton>>>
    InputManager::getAllActionEvents() const {
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
} // namespace ne
