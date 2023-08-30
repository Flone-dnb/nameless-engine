#pragma once

// Standard.
#include <unordered_map>
#include <string>
#include <variant>
#include <optional>
#include <mutex>
#include <vector>

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

    /** Holds current axis event state. */
    class AxisState {
    public:
        AxisState() = delete;

        /**
         * Initializes axis event state.
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

        /** Plus key (triggers input value '+1'). */
        KeyboardKey plusKey;

        /** Plus key (triggers input value '-1'). */
        KeyboardKey minusKey;

        /** Whether @ref plusKey is currently pressed or not. */
        bool bIsPlusKeyPressed;

        /** Whether @ref minusKey is currently pressed or not. */
        bool bIsMinusKeyPressed;
    };

    /**
     * Allows binding IDs with multiple input keys.
     *
     * Stored in GameInstance object.
     */
    class InputManager {
        // Triggers input events.
        friend class GameManager;

    public:
        InputManager() = default;
        InputManager(const InputManager&) = delete;
        InputManager& operator=(const InputManager&) = delete;

        /**
         * Adds a new action event.
         *
         * Action event allows binding mouse button(s) and/or keyboard key(s)
         * with an ID. When one of the specified buttons is pressed you will receive
         * an action event with the specified ID.
         *
         * This way you can have an action "jump" with a space bar button
         * and can easily change input key space bar to something else if
         * the user wants to. For this, just call @ref modifyActionEventKey
         * to change one button of the action.
         *
         * @param iActionId   Unique ID of the new action event.
         * @param vKeys       Keyboard/mouse keys/buttons associated with this action.
         * If empty, no event will be added.
         *
         * @return Returns an error if passed 'vKeys' argument is empty or if an
         * action event with this ID is already registered.
         */
        [[nodiscard]] std::optional<Error> addActionEvent(
            unsigned int iActionId, const std::vector<std::variant<KeyboardKey, MouseButton>>& vKeys);

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
         * @param iAxisEventId  Unique ID of the new axis event.
         * @param vAxis         A pair of keyboard buttons associated with this axis,
         * first button will be associated with '+1' input and the second with '-1' input.
         *
         * @return Returns an error if passed 'vAxis' argument is empty or if an
         * axis event with this ID is already registered.
         */
        [[nodiscard]] std::optional<Error> addAxisEvent(
            unsigned int iAxisEventId, const std::vector<std::pair<KeyboardKey, KeyboardKey>>& vAxis);

        /**
         * Change action event's key.
         *
         * @param iActionId Unique ID of the action event to modify.
         * @param oldKey    Key/button of the specified action event that you want to replace.
         * @param newKey    New key/button that should replace the old key.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> modifyActionEventKey(
            unsigned int iActionId,
            std::variant<KeyboardKey, MouseButton> oldKey,
            std::variant<KeyboardKey, MouseButton> newKey);

        /**
         * Change axis event's key.
         *
         * @param iAxisEventId Unique ID of the axis event to modify.
         * @param oldPair      A pair of buttons of the specified axis event that you want to replace.
         * @param newPair      A new pair of buttons that should replace the old pair.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> modifyAxisEventKey(
            unsigned int iAxisEventId,
            std::pair<KeyboardKey, KeyboardKey> oldPair,
            std::pair<KeyboardKey, KeyboardKey> newPair);

        /**
         * Saves added action/axis events to a file.
         *
         * @param sFileName   Name of the file to save, prefer to have only ASCII characters in the
         * file name. We will save it to a predefined directory using SETTINGS category,
         * the .toml extension will be added if the passed name does not have it.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> saveToFile(std::string_view sFileName);

        /**
         * Loads action/axis events from a file.
         *
         * @warning This function will only read action/axis events that exist
         * in this InputManager. File's keys for action/axis event will replace
         * the keys of existing action/axis event.
         *
         * The usual workflow for working with input goes like this:
         * - add your action/axis events with some default keys,
         * - the user may change the keys of action/axis events during game,
         * - save changed events using @ref saveToFile,
         * - on next startup add your action/axis events with some default keys,
         * - use @ref loadFromFile to load previously changed keys.
         *
         * @param sFileName   Name of the file to load, prefer to have only ASCII characters in the
         * file name. We will load it from a predefined directory using SETTINGS category,
         * the .toml extension will be added if the passed name does not have it.
         *
         * @return Error if something went wrong, this usually means that the file was corrupted.
         */
        [[nodiscard]] std::optional<Error> loadFromFile(std::string_view sFileName);

        /**
         * Returns action and axis event IDs that the specified key is used in.
         *
         * You can use this function to detect conflicting keys. For example:
         * when the user wants to modify some event and you receive a 'newKey',
         * look if this 'newKey' is already used somewhere and if it's,
         * show a message to the user like: "this key is already used somewhere else
         * and cannot be assigned twice".
         *
         * @param key A key to see where it's used.
         *
         * @return A pair of action and axis event IDs that the specified key is used in.
         */
        std::pair<std::vector<unsigned int>, std::vector<unsigned int>>
        isKeyUsed(const std::variant<KeyboardKey, MouseButton>& key);

        /**
         * Looks for an action event with the specified ID, if one is found a copy of this action's
         * keys will be returned. Changes made to the returned vector will not be applied to the
         * action, use @ref addActionEvent for this purpose.
         *
         * @param iActionId Unique ID of an action to look for.
         *
         * @return Empty if no keys were associated with this event, otherwise keys associated with the
         * action event.
         */
        std::vector<std::variant<KeyboardKey, MouseButton>> getActionEvent(unsigned int iActionId);

        /**
         * Looks for an axis event with the specified ID, if one is found a copy of this axis's
         * keys will be returned. Changes made to the returned vector will not be applied to the
         * axis, use @ref addAxisEvent for this purpose.
         *
         * @param iAxisEventId Unique ID of the axis event to look for.
         *
         * @return Empty if no keys were associated with this event, otherwise keys associated with the
         * axis event.
         */
        std::vector<std::pair<KeyboardKey, KeyboardKey>> getAxisEvent(unsigned int iAxisEventId);

        /**
         * Returns the current value of an axis event.
         * This value is equal to the last value passed to GameInstance::onInputAxisEvent.
         *
         * @param iAxisEventId Unique ID of the axis event that you used in @ref addAxisEvent.
         *
         * @return Zero if axis event with this ID does not exist, last input value otherwise.
         */
        float getCurrentAxisEventState(unsigned int iAxisEventId);

        /**
         * Removes an action event with the specified ID.
         *
         * @param iActionId Unique ID of the action to remove.
         *
         * @return `false` if the action was found and removed, `true` if not.
         */
        bool removeActionEvent(unsigned int iActionId);

        /**
         * Removes an axis event with the specified ID.
         *
         * @param iAxisEventId Unique ID of the axis event to remove.
         *
         * @return `false` if the axis was found and removed, `true` if not.
         */
        bool removeAxisEvent(unsigned int iAxisEventId);

        /**
         * Returns all action events.
         *
         * @return A copy of all action events.
         */
        std::unordered_map<unsigned int, std::vector<std::variant<KeyboardKey, MouseButton>>>
        getAllActionEvents();

        /**
         * Returns all axis events.
         *
         * @return A copy of all axis events.
         */
        std::unordered_map<unsigned int, std::vector<std::pair<KeyboardKey, KeyboardKey>>> getAllAxisEvents();

        /**
         * Splits the string using a delimiter.
         *
         * @param sStringToSplit String to split.
         * @param sDelimiter     Delimiter.
         *
         * @return Splitted string.
         */
        static std::vector<std::string>
        splitString(const std::string& sStringToSplit, const std::string& sDelimiter);

    private:
        /**
         * Adds a new action event. If an action with this ID already exists it will be removed
         * to register this new action event.
         *
         * @warning If this action is triggered with an old key right now
         * (when you call this function), there is a chance that this action will be triggered
         * using old keys for the last time (even if after you removed this action).
         * This is because when we receive input key we make a copy of all actions
         * associated with the key and then call these actions, because we operate
         * on a copy, removed elements will be reflected only on the next user input.
         *
         * @param iActionId   Unique ID of an action event to overwrite.
         * @param vKeys       Keyboard/mouse keys/buttons associated with this action.
         * If empty, no event will be added.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> overwriteActionEvent(
            unsigned int iActionId, const std::vector<std::variant<KeyboardKey, MouseButton>>& vKeys);

        /**
         * Adds a new axis event. If an axis event with this ID already exists it will be removed
         * to register this new axis event.
         *
         * @warning If this axis event is triggered with an old key right now
         * (when you call this function), there is a chance that this axis event will be triggered
         * using old keys for the last time (even if after you removed this axis event).
         * This is because when we receive input key we make a copy of all axes
         * associated with the key and then call these axes, because we operate
         * on a copy, removed elements will be reflected only on the next user input.
         *
         * @param iAxisEventId  Unique ID of an axis event to overwrite.
         * @param vAxis         A pair of keyboard buttons associated with this axis,
         * first button will be associated with '+1' input and the second with '-1' input.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> overwriteAxisEvent(
            unsigned int iAxisEventId, const std::vector<std::pair<KeyboardKey, KeyboardKey>>& vAxis);

        /**
         * Map that stores pairs of "key that triggers event" - "registered events".
         *
         * TODO: add GamepadKey into variant when gamepad will be supported and save/load them in
         * TODO: saveToFile/loadFromFile (+ add tests).
         */
        std::unordered_map<std::variant<KeyboardKey, MouseButton>, std::vector<unsigned int>> actionEvents;

        /**
         * Stores pairs of "action event ID" and a pair of "registered keys" - "last input
         * (pressed/released)".
         */
        std::unordered_map<unsigned int, std::pair<std::vector<ActionState>, bool /* action state */>>
            actionState;

        /**
         * Map that stores pair of "keyboard key that triggers event" - "array of registered events
         * with an input value that should be triggered (either +1 (`true`) or -1 (`false`))".
         *
         * TODO: add GamepadAxis when gamepad will be supported and save/load
         * TODO: them in saveToFile/loadFromFile (+ add tests).
         */
        std::unordered_map<KeyboardKey, std::vector<std::pair<unsigned int, bool>>> axisEvents;

        /**
         * Stores pairs of "axis event ID" and a pair of "registered keys" - "last input in
         * range [-1; 1]".
         */
        std::unordered_map<unsigned int, std::pair<std::vector<AxisState>, int /* axis state */>> axisState;

        /** Mutex for actions editing. */
        std::recursive_mutex mtxActionEvents;

        /** Mutex for axis editing. */
        std::recursive_mutex mtxAxisEvents;

        /** Section name to store action events, used in .toml files. */
        static inline const std::string_view sActionEventSectionName = "action event";

        /** Section name to store axis events, used in .toml files. */
        static inline const std::string_view sAxisEventSectionName = "axis event";
    };
} // namespace ne
