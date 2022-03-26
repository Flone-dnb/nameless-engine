#include "game/window.h"

// Std.
#include <filesystem>
#include <format>

// Custom.
#include "misc/UniqueValueGenerator.h"

// External.
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace ne {
    void GLFWWindowKeyCallback(GLFWwindow *pGLFWWindow, int iKey, int iScancode, int iAction, int iMods) {
        if (iAction == GLFW_REPEAT) {
            return;
        }

        const Window *pWindow = static_cast<Window *>(glfwGetWindowUserPointer(pGLFWWindow));
        if (!pWindow) {
            return;
        }

        pWindow->internalOnKeyboardInput(static_cast<KeyboardKey>(iKey), KeyboardModifiers(iMods),
                                         iAction == GLFW_PRESS ? true : false);
    }

    void GLFWWindowFocusCallback(GLFWwindow *pGLFWWindow, int iFocused) {
        const Window *pWindow = static_cast<Window *>(glfwGetWindowUserPointer(pGLFWWindow));
        if (!pWindow) {
            return;
        }

        pWindow->internalOnWindowFocusChanged(static_cast<bool>(iFocused));
    }

    WindowBuilder &WindowBuilder::withSize(int iWidth, int iHeight) {
        params.iWindowWidth = iWidth;
        params.iWindowHeight = iHeight;

        return *this;
    }

    WindowBuilder &WindowBuilder::withTitle(std::string_view sWindowTitle) {
        params.sWindowTitle = sWindowTitle;

        return *this;
    }

    WindowBuilder &WindowBuilder::withIcon(std::string_view sPathToIcon) {
        params.sPathToWindowIcon = sPathToIcon;

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

    std::variant<std::unique_ptr<Window>, Error> WindowBuilder::build() {
        return Window::newInstance(params);
    }

    void Window::hide() const { glfwHideWindow(pGLFWWindow); }

    void Window::close() const { glfwSetWindowShouldClose(pGLFWWindow, 1); }

    std::string Window::getTitle() const { return sWindowTitle; }

    float Window::getOpacity() const { return glfwGetWindowOpacity(pGLFWWindow); }

    IRenderer *Window::getRenderer() const {
        if (pGame && pGame->pRenderer) {
            return pGame->pRenderer.get();
        }

        return nullptr;
    }

    void Window::internalOnKeyboardInput(KeyboardKey key, KeyboardModifiers modifiers,
                                         bool bIsPressedDown) const {
        if (!pGame) {
            return;
        }
        pGame->onKeyInput(key, modifiers, bIsPressedDown);
    }

    void Window::internalOnWindowFocusChanged(bool bIsFocused) const {
        if (!pGame || !pGame->pGameInstance) {
            return;
        }
        pGame->pGameInstance->onWindowFocusChanged(bIsFocused);
    }

    std::variant<std::unique_ptr<Window>, Error> Window::newInstance(WindowBuilderParameters &params) {
        GLFW::get(); // initialize GLFW

        std::string sNewWindowTitle(params.sWindowTitle);

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

            params.iWindowWidth = mode->width;
            params.iWindowHeight = mode->height;
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

        auto pWindow = std::unique_ptr<Window>(new Window(pGLFWWindow, sNewWindowTitle));

        // Set icon.
        if (!params.sPathToWindowIcon.empty()) {
            auto error = pWindow->setIcon(params.sPathToWindowIcon);
            if (error.has_value()) {
                error->addEntry();
                error->showError();
                // don't throw here, not a critical error.
            }
        }

        // Add Window pointer.
        glfwSetWindowUserPointer(pGLFWWindow, pWindow.get());

        // Bind to keyboard input.
        glfwSetKeyCallback(pGLFWWindow, GLFWWindowKeyCallback);

        // Bind to focus change event.
        glfwSetWindowFocusCallback(pGLFWWindow, GLFWWindowFocusCallback);

        return std::move(pWindow);
    }

    Window::~Window() { glfwDestroyWindow(pGLFWWindow); }

    WindowBuilder Window::getBuilder() { return WindowBuilder(); }

    void Window::show() const { glfwShowWindow(pGLFWWindow); }

    void Window::setOpacity(float fOpacity) const { glfwSetWindowOpacity(pGLFWWindow, fOpacity); }

    void Window::setTitle(const std::string &sNewTitle) {
        glfwSetWindowTitle(pGLFWWindow, sNewTitle.c_str());
        sWindowTitle = sNewTitle;
    }

    std::optional<Error> Window::setIcon(std::string_view sPathToIcon) const {
        if (!std::filesystem::exists(sPathToIcon)) {
            return Error(std::format("the specified file \"{}\" does not exist.", sPathToIcon));
        }

        GLFWimage images[1];
        images[0].pixels = stbi_load(sPathToIcon.data(), &images[0].width, &images[0].height, nullptr, 4);

        glfwSetWindowIcon(pGLFWWindow, 1, images);

        stbi_image_free(images[0].pixels);

        return {};
    }

    void Window::minimize() const { glfwIconifyWindow(pGLFWWindow); }

    void Window::maximize() const { glfwMaximizeWindow(pGLFWWindow); }

    void Window::restore() const { glfwRestoreWindow(pGLFWWindow); }

    Window::Window(GLFWwindow *pGLFWWindow, const std::string &sWindowTitle) {
        this->pGLFWWindow = pGLFWWindow;
        this->sWindowTitle = sWindowTitle;
    }
} // namespace ne