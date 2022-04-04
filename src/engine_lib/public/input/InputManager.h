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
     * Holds current axis state.
     */
    class AxisState {
    public:
        AxisState() = delete;

        /**
         * Initialize axis state.
         *
         * @param plusKey  Key for '+1' input.
         * @param minusKey Key for '-1' input.
         */
        AxisState(KeyboardKey plusKey, KeyboardKey minusKey) {
            this->plusKey = plusKey;
            this->minusKey = minusKey;

            isPlusKeyPressed = false;
            isMinusKeyPressed = false;
        }

        /** Plus key (input value '+1'). */
        KeyboardKey plusKey;
        /** Plus key (input value '-1'). */
        KeyboardKey minusKey;

        /** Whether the plus key is pressed or not. */
        bool isPlusKeyPressed;
        /** Whether the minus key is pressed or not. */
        bool isMinusKeyPressed;
    };

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
         * Action event allows binding mouse button(-s) and/or keyboard key(-s)
         * with a name. When one if the specified buttons is pressed you will receive
         * an action event with the specified name.
         *
         * This way you can have an action "jump" with a space bar button
         * and can easily change input key space bar to something else if
         * the user wants to. For this, just call this function again with
         * the same action name "jump" but different buttons.
         *
         * @warning If an action with this name already exists, it will be overwritten
         * with the new keys (old keys will be removed).
         * If this action is triggered with an old key right now
         * (when you call this function), there is a chance that this action will be triggered
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
         * Adds a new axis event.
         *
         * Using axis event you can easily implement player movement (for example).
         * An axis event consists of 2 buttons: one for '+1' input and the other
         * for '-1' input. When the first button is pressed this event will be
         * triggered with '+1' value, if other is pressed this event will be
         * triggered with '-1' value, if both buttons were released this event
         * will be triggered with '0' value. If both buttons were pressed this event
         * will be triggered with the value of the last pressed button ('+1' if
         * the first button was pressed last, '-1' if the second button was pressed last).
         *
         * You can specify multiple pairs, for example: W/S buttons and up/down arrow keys.
         *
         * @warning If an axis event with this name already exists, it will be overwritten
         * with the new keys (old keys will be removed).
         * If this axis event is triggered with an old key right now
         * (when you call this function), there is a chance that this axis event will be triggered
         * using old keys for the last time (even if after you removed this axis event).
         * This is because when we receive input key we make a copy of all axes
         * associated with the key and then call these axes, because we operate
         * on a copy, removed elements will be reflected only on the next user input.
         *
         * @param sAxisName     Name of a new axis.
         * @param vAxis         A pair of keyboard buttons associated with this axis,
         * first button will be associated with '+1' input and the second with '-1' input.
         */
        void addAxisEvent(const std::string &sAxisName,
                          const std::vector<std::pair<KeyboardKey, KeyboardKey>> &vAxis);

        /**
         * Looks for an action event with the specified name, if one is found a copy of this action's
         * keys will be returned. Changes made to the returned vector will not be applied to the
         * action, use @ref addActionEvent for this purpose.
         *
         * @param sActionName   Name of the action to look for.
         *
         * @return Keys associated with the action event if one was found.
         */
        std::optional<std::vector<std::variant<KeyboardKey, MouseButton>>>
        getActionEvent(const std::string &sActionName);

        /**
         * Looks for an axis event with the specified name, if one is found a copy of this axis's
         * keys will be returned. Changes made to the returned vector will not be applied to the
         * axis, use @ref addAxisEvent for this purpose.
         *
         * @param sAxisName   Name of the axis to look for.
         *
         * @return Keys associated with the axis event if one was found.
         */
        std::optional<std::vector<std::pair<KeyboardKey, KeyboardKey>>>
        getAxisEvent(const std::string &sAxisName);

        /**
         * Removes an action event with the specified name.
         *
         * @warning If this action is triggered with an old key right now
         * (when you call this function), there is a chance that this action will be triggered
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
         * Removes an axis event with the specified name.
         *
         * @warning If an axis event with this name already exists, it will be overwritten
         * with the new keys (old keys will be removed).
         * If this axis event is triggered with an old key right now
         * (when you call this function), there is a chance that this axis event will be triggered
         * using old keys for the last time (even if after you removed this axis event).
         * This is because when we receive input key we make a copy of all axes
         * associated with the key and then call these axes, because we operate
         * on a copy, removed elements will be reflected only on the next user input.
         *
         * @param sAxisName   Name of the axis to remove.
         *
         * @return 'false' if the axis was found and removed, 'true' if not.
         */
        bool removeAxisEvent(const std::string &sAxisName);

        /**
         * Returns all action events.
         *
         * @return A copy of all actions.
         */
        std::unordered_map<std::string, std::vector<std::variant<KeyboardKey, MouseButton>>>
        getAllActionEvents();

        /**
         * Returns all axis events.
         *
         * @return A copy of all axes.
         */
        std::unordered_map<std::string, std::vector<std::pair<KeyboardKey, KeyboardKey>>> getAllAxisEvents();

    private:
        friend class Game;

        /**
         * Map of action events.
         * TODO: add GamepadKey into variant when gamepad will be supported.
         */
        std::unordered_map<std::variant<KeyboardKey, MouseButton>, std::set<std::string>> actionEvents;

        /**
         * Map of axis events.
         * A pair of keyboard keys define values for +1 and -1 input.
         * Map's key defines a keyboard key, map's value defines an array of actions with
         * an input value (+1/-1).
         * TODO: add GamepadAxis when gamepad will be supported.
         */
        std::unordered_map<KeyboardKey, std::set<std::pair<std::string, int>>> axisEvents;

        /** Used to store current axis state. */
        std::unordered_map<std::string, std::pair<std::vector<AxisState>, int /* last input */>> axisState;

        /** Mutex for actions editing. */
        std::recursive_mutex mtxActionEvents;

        /** Mutex for axis editing. */
        std::recursive_mutex mtxAxisEvents;
    };
} // namespace ne
