#include "window.h"

// Custom.
#include "UniqueValueGenerator.h"

namespace ne {
    WindowBuilder &WindowBuilder::withSize(int iWidth, int iHeight) {
        params.iWindowWidth = iWidth;
        params.iWindowHeight = iHeight;

        return *this;
    }

    WindowBuilder &WindowBuilder::withTitle(const std::string &sWindowTitle) {
        params.sWindowTitle = sWindowTitle;

        return *this;
    }

    WindowBuilder &WindowBuilder::withVisibility(bool bShow) {
        params.bShowWindow = bShow;

        return *this;
    }

    WindowBuilder &WindowBuilder::withMaximizedState(bool bMaximized) {
        params.bMaximized = bMaximized;

        return *this;
    }

    WindowBuilder &WindowBuilder::withSplashScreenMode(bool bIsSplashScreen) {
        params.bIsSplashScreen = bIsSplashScreen;

        return *this;
    }

    WindowBuilder &WindowBuilder::withFullscreenMode(bool bEnableFullscreen) {
        params.bFullscreen = bEnableFullscreen;

        return *this;
    }

    std::variant<std::unique_ptr<Window>, Error> WindowBuilder::build() const {
        return Window::newInstance(params);
    }

    void Window::hide() const { glfwHideWindow(pGLFWWindow); }

    void Window::close() const { glfwSetWindowShouldClose(pGLFWWindow, 1); }

    std::string Window::getTitle() const { return sWindowTitle; }

    float Window::getOpacity() const { return glfwGetWindowOpacity(pGLFWWindow); }

    std::variant<std::unique_ptr<Window>, Error> Window::newInstance(const WindowBuilderParameters &params) {
        GLFW::get(); // initialize GLFW

        std::string sNewWindowTitle = params.sWindowTitle;

        // Check window name.
        if (sNewWindowTitle.empty()) {
            sNewWindowTitle = UniqueValueGenerator::get().getUniqueWindowName();
        }

        // Check fullscreen mode (windowed fullscreen).
        GLFWmonitor *pMonitor = nullptr;
        if (params.bFullscreen) {
            pMonitor = glfwGetPrimaryMonitor();
            const GLFWvidmode *mode = glfwGetVideoMode(pMonitor);

            glfwWindowHint(GLFW_RED_BITS, mode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        } else {
            if (!params.bShowWindow) {
                glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            }

            if (params.bIsSplashScreen) {
                glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
            }

            if (params.bMaximized) {
                glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
            }
        }

        // Create GLFW window.
        GLFWwindow *pGLFWWindow = glfwCreateWindow(params.iWindowWidth, params.iWindowHeight,
                                                   sNewWindowTitle.c_str(), pMonitor, nullptr);
        if (!pGLFWWindow) {
            return Error("failed to create window");
        }

        return std::unique_ptr<Window>(new Window(pGLFWWindow, sNewWindowTitle));
    }

    Window::~Window() { glfwDestroyWindow(pGLFWWindow); }

    WindowBuilder Window::getBuilder() { return WindowBuilder(); }

    void Window::show() const { glfwShowWindow(pGLFWWindow); }

    void Window::setOpacity(float fOpacity) const { glfwSetWindowOpacity(pGLFWWindow, fOpacity); }

    void Window::minimize() const { glfwIconifyWindow(pGLFWWindow); }

    void Window::maximize() const { glfwMaximizeWindow(pGLFWWindow); }

    void Window::restore() const { glfwRestoreWindow(pGLFWWindow); }

    Window::Window(GLFWwindow *pGLFWWindow, const std::string &sWindowTitle) {
        this->pGLFWWindow = pGLFWWindow;
        this->sWindowTitle = sWindowTitle;
    }
} // namespace ne