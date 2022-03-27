﻿#include "input/InputManager.h"

namespace ne {
    void InputManager::addAction(const std::string &sActionName,
                                 const std::vector<std::variant<KeyboardKey, MouseButton>> &vKeys) {
        actions[sActionName] = vKeys;
    }

    std::optional<std::vector<std::variant<KeyboardKey, MouseButton>>>
    InputManager::getAction(const std::string &sActionName) const {
        const auto iterator = actions.find(sActionName);

        if (iterator == actions.end()) {
            return {};
        } else {
            return iterator->second;
        }
    }

    bool InputManager::removeAction(const std::string &sActionName) { return !actions.erase(sActionName); }

    std::unordered_map<std::string, std::vector<std::variant<KeyboardKey, MouseButton>>>
    InputManager::getAllActions() const {
        return actions;
    }
} // namespace ne