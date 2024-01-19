#pragma once

namespace ne {
    /** Stores unique IDs of input events. */
    struct EditorInputEventIds {
        /** Groups action events. */
        enum class Action : unsigned int {
            CAPTURE_MOUSE_CURSOR = 0, //< Capture mouse cursor.
            INCREASE_CAMERA_SPEED,    //< Increase editor camera's speed.
            DECREASE_CAMERA_SPEED,    //< Decrease editor camera's speed.
        };

        /** Groups axis events. */
        enum class Axis : unsigned int {
            MOVE_CAMERA_FORWARD = 0, //< Move editor's camera forward/back.
            MOVE_CAMERA_RIGHT,       //< Move editor's camera right/left.
            MOVE_CAMERA_UP,          //< Move editor's camera up/down.
        };
    };
}
