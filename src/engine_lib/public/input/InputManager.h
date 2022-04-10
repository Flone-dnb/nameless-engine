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
     * Holds current action state.
     */
    class ActionState {
    public:
        ActionState() = delete;

        /**
         * Initialize action state.
         *
         * @param key  Action key.
         */
        ActionState(std::variant<KeyboardKey, MouseButton> key) {
            this->key = key;
            bIsPressed = false;
        }

        /** Whether the key is pressed or not. */
        bool bIsPressed;

        /** Action key. */
        std::variant<KeyboardKey, MouseButton> key;
    };

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

            bIsPlusKeyPressed = false;
            bIsMinusKeyPressed = false;
        }

        /** Plus key (input value '+1'). */
        KeyboardKey plusKey;
        /** Plus key (input value '-1'). */
        KeyboardKey minusKey;

        /** Whether the plus key is pressed or not. */
        bool bIsPlusKeyPressed;
        /** Whether the minus key is pressed or not. */
        bool bIsMinusKeyPressed;
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
         * the same action name "jump" but different buttons or use @ref modifyActionEventKey
         * to only change one button of the action.
         *
         * @warning If this action is triggered with an old key right now
         * (when you call this function), there is a chance that this action will be triggered
         * using old keys for the last time (even if after you removed this action).
         * This is because when we receive input key we make a copy of all actions
         * associated with the key and then call these actions, because we operate
         * on a copy, removed elements will be reflected only on the next user input.
         *
         * @param sActionName   Name of a new action.
         * @param vKeys         Keyboard/mouse keys/buttons associated with this action.
         * If empty, no event will be added.
         *
         * @return Returns an error if passed 'vKeys' argument is empty.
         */
        std::optional<Error> addActionEvent(const std::string &sActionName,
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
         *
         * @return Returns an error if passed 'vAxis' argument is empty.
         */
        std::optional<Error> addAxisEvent(const std::string &sAxisName,
                                          const std::vector<std::pair<KeyboardKey, KeyboardKey>> &vAxis);

        /**
         * Change action event's key.
         *
         * @param sActionName Name of the action event to modify.
         * @param oldKey      Key/button of the specified action event that you want to replace.
         * @param newKey      New key/button that should replace the old key.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> modifyActionEventKey(const std::string &sActionName,
                                                  std::variant<KeyboardKey, MouseButton> oldKey,
                                                  std::variant<KeyboardKey, MouseButton> newKey);

        /**
         * Change axis event's key.
         *
         * @param sAxisName   Name of the axis event to modify.
         * @param oldPair     A pair of buttons of the specified axis event that you want to replace.
         * @param newPair     A new pair of buttons that should replace the old pair.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> modifyAxisEventKey(const std::string &sAxisName,
                                                std::pair<KeyboardKey, KeyboardKey> oldPair,
                                                std::pair<KeyboardKey, KeyboardKey> newPair);

        /**
         * Saves added action/axis events to a file.
         *
         * @param sFileName   Name of the file to save, prefer to have only ASCII characters in the
         * file name. We will save it to a predefined directory using SETTINGS category,
         * the .ini extension will be added if the passed name does not have it.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> saveToFile(std::string_view sFileName);

        /**
         * Loads action/axis events from a file.
         *
         * @warning This function will only read action/axis events that exist
         * in this InputManager. File's keys for action/axis event will replace
         * the keys of existing action/axis event.
         *
         * The usual workflow goes like this:
         * - add your action/axis events with some default keys,
         * - the user may change the keys of action/axis events during game,
         * - save changed events using @ref saveToFile,
         * - on next startup add your action/axis events with some default keys,
         * - use @ref loadFromFile to load previously changed keys.
         *
         * We are using this approach because during development you may
         * rename/remove different events. Imagine today you have an event "goForward",
         * you test your game and saving, the save file will contain "goForward" event,
         * tomorrow you decided to rename it to "moveForward" but @ref loadFromFile
         * will load "goForward" while you would expect "moveForward", this is why
         * we only read events that were already added to InputManager.
         *
         * @param sFileName   Name of the file to load, prefer to have only ASCII characters in the
         * file name. We will load it from a predefined directory using SETTINGS category,
         * the .ini extension will be added if the passed name does not have it.
         *
         * @return Error if something went wrong, this usually means that the file was corrupted.
         */
        std::optional<Error> loadFromFile(std::string_view sFileName);

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
         * Returns the current value of an axis event.
         * This value is equal to the last value passed to IGameInstance::onInputAxisEvent.
         *
         * @param sAxisName Name of the axis event that you used in @ref addAxisEvent.
         *
         * @return Zero if axis event with this name does not exist, last input value otherwise.
         */
        float getCurrentAxisEventValue(const std::string &sAxisName);

        /**
         * Returns the current value of an action event.
         * This value is equal to the last value passed to IGameInstance::onInputActionEvent.
         *
         * @param sActionName Name of the action event that you used in @ref addActionEvent.
         *
         * @return Zero if action event with this name does not exist, 'true' if at least one
         * key/button associated with this action is pressed, 'false' if all released (not pressed).
         */
        bool getCurrentActionEventValue(const std::string &sActionName);

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
         * TODO: add GamepadKey into variant when gamepad will be supported and save/load them in
         * TODO: saveToFile/loadFromFile (+ add tests).
         */
        std::unordered_map<std::variant<KeyboardKey, MouseButton>, std::set<std::string>> actionEvents;

        /** Used to store current action state. TODO: gamepad */
        std::unordered_map<std::string, std::pair<std::vector<ActionState>, bool /* action state */>>
            actionState;

        /**
         * Map of axis events.
         * A pair of keyboard keys define values for +1 and -1 input.
         * Map's key defines a keyboard key, map's value defines an array of actions with
         * an input value (+1/-1).
         * TODO: add GamepadAxis when gamepad will be supported and save/load
         * TODO: them in saveToFile/loadFromFile (+ add tests).
         */
        std::unordered_map<KeyboardKey, std::set<std::pair<std::string, int>>> axisEvents;

        /** Used to store current axis state. TODO: gamepad */
        std::unordered_map<std::string, std::pair<std::vector<AxisState>, int /* axis state */>> axisState;

        /** Mutex for actions editing. */
        std::recursive_mutex mtxActionEvents;

        /** Mutex for axis editing. */
        std::recursive_mutex mtxAxisEvents;

        /** Section name to store action events, used in .ini files. */
        const std::string_view sActionEventSectionName = "action event";

        /** Section name to store axis events, used in .ini files. */
        const std::string_view sAxisEventSectionName = "axis event";
    };
} // namespace ne
