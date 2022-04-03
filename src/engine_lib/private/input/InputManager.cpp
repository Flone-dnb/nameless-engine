#include "input/InputManager.h"

namespace ne {
    void InputManager::addActionEvent(const std::string &sActionName,
                                      const std::vector<std::variant<KeyboardKey, MouseButton>> &vKeys) {
        actionEvents[sActionName] = vKeys;
    }

    std::optional<std::vector<std::variant<KeyboardKey, MouseButton>>>
    InputManager::getActionEvent(const std::string &sActionName) const {
        const auto iterator = actionEvents.find(sActionName);

        if (iterator == actionEvents.end()) {
            return {};
        } else {
            return iterator->second;
        }
    }

    bool InputManager::removeActionEvent(const std::string &sActionName) {
        return !actionEvents.erase(sActionName);
    }

    std::unordered_map<std::string, std::vector<std::variant<KeyboardKey, MouseButton>>>
    InputManager::getAllActionEvents() const {
        return actionEvents;
    }
} // namespace ne
