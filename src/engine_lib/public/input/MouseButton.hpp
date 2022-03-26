#pragma once

// Custom.
#include "window/GLFW.hpp"

namespace ne {
    enum class MouseButton {
        LEFT = GLFW_MOUSE_BUTTON_LEFT,
        RIGHT = GLFW_MOUSE_BUTTON_RIGHT,
        MIDDLE = GLFW_MOUSE_BUTTON_MIDDLE,
        X1 = GLFW_MOUSE_BUTTON_4,
        X2 = GLFW_MOUSE_BUTTON_5,
        X3 = GLFW_MOUSE_BUTTON_6,
        X4 = GLFW_MOUSE_BUTTON_7,
    };
}