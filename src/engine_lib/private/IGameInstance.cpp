#include "IGameInstance.h"

// Custom.
#include "GLFW.hpp"

namespace ne {
    float IGameInstance::getTotalApplicationTimeInSec() { return static_cast<float>(glfwGetTime()); }
} // namespace ne
