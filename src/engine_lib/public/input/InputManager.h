#pragma once

// STL.
#include <unordered_map>
#include <string>
#include <variant>
#include <optional>
#include <set>
#include <mutex>

// Custom.
#include "input/KeyboardKey.hpp"
#include "input/MouseButton.hpp"

namespace ne {
    /**
     * Allows binding names with multiple input keys.
     */
    class InputManager {
    public:
        InputManager() = default;
        InputManager(const InputManager &) = delete;
        InputManager &operator=(const InputManager &) = delete;

        /**
         * Adds a new action event.
         *
         * @warning If an action with this name already exists, it will be overwritten
         * with the new keys (old keys will be removed).
         * If this action is triggered with an old key right now
         * (when you call this function), there is a change that this action will be triggered
         * using old keys for the last time (even if after you removed this action).
         * This is because when we receive input key we make a copy of all actions
         * associated with the key and then call these actions, because we operate
         * on a copy, removed elements will be reflected only on the next user input.
         *
         * @param sActionName   Name of a new action.
         * @param vKeys         Keyboard/mouse keys/buttons associated with this action.
         */
        void addActionEvent(const std::string &sActionName,
                            const std::vector<std::variant<KeyboardKey, MouseButton>> &vKeys);

        /**
         * Looks for an action event with the specified name, if one is found a copy of this action's
         * keys will be returned. Changes made to the returned vector will not be applied to the
         * action, use @ref addActionEvent for this purpose.
         *
         * @param sActionName   Name an action to look for.
         *
         * @return Keys associated with the action event if one was found.
         */
        std::optional<std::vector<std::variant<KeyboardKey, MouseButton>>>
        getActionEvent(const std::string &sActionName);

        /**
         * Removes an action event with the specified name.
         *
         * @warning If this action is triggered with an old key right now
         * (when you call this function), there is a change that this action will be triggered
         * using old keys for the last time (even if after you removed this action).
         * This is because when we receive input key we make a copy of all actions
         * associated with the key and then call these actions, because we operate
         * on a copy, removed elements will be reflected only on the next user input.
         *
         * @param sActionName   Name of the action to remove.
         *
         * @return 'false' if the action was found and removed, 'true' if not.
         */
        bool removeActionEvent(const std::string &sActionName);

        /**
         * Returns all action events.
         *
         * @return A copy of all actions.
         */
        std::unordered_map<std::string, std::vector<std::variant<KeyboardKey, MouseButton>>>
        getAllActionEvents();

    private:
        friend class Game;

        /** Map of action events. */
        std::unordered_map<std::variant<KeyboardKey, MouseButton>, std::set<std::string>> actionEvents;

        /** Mutex for actions editing. */
        std::recursive_mutex mtxActions;
    };
} // namespace ne
