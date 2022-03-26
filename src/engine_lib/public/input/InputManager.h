#pragma once

// STL.
#include <unordered_map>
#include <string>
#include <vector>
#include <variant>
#include <optional>

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
         * Adds a new action.
         *
         * @warning If an action with this name already exists, it will be overwritten
         * with the new keys (old keys will be removed).
         *
         * @param sActionName   Name of a new action.
         * @param vKeys         Keyboard/mouse keys/buttons associated with this action.
         */
        void addAction(const std::string &sActionName,
                       const std::vector<std::variant<KeyboardKey, MouseButton>> &vKeys);

        /**
         * Looks for an action with the specified name, if one is found a copy of this action's
         * keys will be returned. Changes made to the returned vector will not be applied to the
         * action, use @ref addAction for this purpose.
         *
         * @param sActionName   Name an action to look for.
         *
         * @return Keys associated with the action if one was found.
         */
        std::optional<std::vector<std::variant<KeyboardKey, MouseButton>>>
        getAction(const std::string &sActionName) const;

        /**
         * Removes an action with the specified name.
         *
         * @param sActionName   Name of the action to remove.
         *
         * @return 'false' if the action was found and removed, 'true' if not.
         */
        bool removeAction(const std::string &sActionName);

        /**
         * Returns all actions.
         *
         * @return A copy of all actions.
         */
        std::unordered_map<std::string, std::vector<std::variant<KeyboardKey, MouseButton>>>
        getAllActions() const;

    private:
        friend class Game;

        /** Map of actions. */
        std::unordered_map<std::string, std::vector<std::variant<KeyboardKey, MouseButton>>> actions;
    };
} // namespace ne
