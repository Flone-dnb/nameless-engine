#pragma once

// Std.
#include <stdexcept>
#include <string>

// Custom.
#include "Error.h"

// External.
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

namespace ne {
    inline void GLFWErrorCallback(int iErrorCode, const char *pDescription) {
        const Error error("GLFW error (" + std::to_string(iErrorCode) + "): " + std::string(pDescription));
        error.showError();
        throw std::runtime_error(error.getError());
    }

    /**
     * Singleton helper class to globally initialize/terminate GLFW.
     */
    class GLFW {
    public:
        GLFW(const GLFW &) = delete;
        GLFW &operator=(const GLFW &) = delete;

        /**
         * Terminates GLFW.
         */
        ~GLFW() { glfwTerminate(); }

        /**
         * Will create a static class instance and return instance if not created before.
         */
        static GLFW &get() {
            static GLFW glfw;
            return glfw;
        }

    private:
        /**
         * Initialized GLFW.
         */
        GLFW() {
            glfwSetErrorCallback(ne::GLFWErrorCallback);

            if (!glfwInit()) {
                const Error error("failed to initialize GLFW");
                error.showError();
                throw std::runtime_error(error.getError());
            }
        }
    };
} // namespace ne
