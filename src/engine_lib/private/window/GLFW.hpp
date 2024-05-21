#pragma once

// Standard.
#include <stdexcept>
#include <string>

// Custom.
#include "misc/Error.h"
#include "io/Logger.h"

// External.
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#undef MessageBox
#undef IGNORE

namespace ne {
    inline void glfwErrorCallback(int iErrorCode, const char* pDescription) {
        const auto sMessage = "GLFW error (" + std::to_string(iErrorCode) + "): " + std::string(pDescription);

        if (iErrorCode == GLFW_FEATURE_UNAVAILABLE) {
            // Just log an error, this is probably some platform-specific limitation like window icons.
            Logger::get().error(sMessage);
            return;
        }

        const Error error(sMessage);
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

    /** Singleton helper class to globally initialize/terminate GLFW. */
    class GLFW {
    public:
        GLFW(const GLFW&) = delete;
        GLFW& operator=(const GLFW&) = delete;

        /** Terminates GLFW. */
        ~GLFW() { glfwTerminate(); }

        /**
         * Creates a static GLFW instance and return instance if not created before.
         *
         * @return Singleton.
         */
        static GLFW& get() {
            static GLFW glfw;
            return glfw;
        }

    private:
        /** Initializes GLFW. */
        GLFW() {
            glfwSetErrorCallback(ne::glfwErrorCallback);

            if (glfwInit() != GLFW_TRUE) {
                const Error error("failed to initialize GLFW");
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
        }
    };
} // namespace ne
