#pragma once

// STL.
#include <unordered_map>
#include <string>
#include <variant>
#include <optional>
#include <set>

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
         * @warning This function is not thread safe, it should only be called from the main thread.
         * @warning If an action with this name already exists, it will be overwritten
         * with the new keys (old keys will be removed).
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
        getActionEvent(const std::string &sActionName) const;

        /**
         * Removes an action event with the specified name.
         *
         * @warning This function is not thread safe, it should only be called from the main thread.
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
        getAllActionEvents() const;

    private:
        friend class Game;

        /** Map of action events. */
        std::unordered_map<std::variant<KeyboardKey, MouseButton>, std::set<std::string>> actionEvents;
    };
} // namespace ne
