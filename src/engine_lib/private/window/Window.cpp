#include "game/Window.h"

// Standard.
#include <filesystem>

// Custom.
#include "misc/UniqueValueGenerator.h"
#include "io/Logger.h"

// External.
#include "fmt/core.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#if defined(WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include "GLFW/glfw3native.h"

namespace ne {
    WindowBuilder& WindowBuilder::withSize(int iWidth, int iHeight) {
        params.iWindowWidth = iWidth;
        params.iWindowHeight = iHeight;

        return *this;
    }

    WindowBuilder& WindowBuilder::withTitle(std::string_view sWindowTitle) {
        params.sWindowTitle = sWindowTitle;

        return *this;
    }

    WindowBuilder& WindowBuilder::withIcon(std::filesystem::path pathToIcon) {
        params.pathToWindowIcon = pathToIcon;

        return *this;
    }

    WindowBuilder& WindowBuilder::withVisibility(bool bShow) {
        params.bShowWindow = bShow;

        return *this;
    }

    WindowBuilder& WindowBuilder::withMaximizedState(bool bMaximized) {
        params.bMaximized = bMaximized;

        return *this;
    }

    WindowBuilder& WindowBuilder::withSplashScreenMode(bool bIsSplashScreen) {
        params.bIsSplashScreen = bIsSplashScreen;

        return *this;
    }

    WindowBuilder& WindowBuilder::withFullscreenMode(bool bEnableFullscreen) {
        params.bFullscreen = bEnableFullscreen;

        return *this;
    }

    std::variant<std::unique_ptr<Window>, Error> WindowBuilder::build() { return Window::create(params); }

    void Window::hide() const { glfwHideWindow(pGlfwWindow); }

    void Window::close() const { glfwSetWindowShouldClose(pGlfwWindow, 1); }

    std::pair<int, int> Window::getSize() const {
        showErrorIfNotOnMainThread();

        int iWidth, iHeight;

        glfwGetWindowSize(pGlfwWindow, &iWidth, &iHeight);

        return std::make_pair(iWidth, iHeight);
    }

    std::pair<float, float> Window::getCursorPosition() const {
        showErrorIfNotOnMainThread();

        double xPos, yPos;
        glfwGetCursorPos(pGlfwWindow, &xPos, &yPos);

        const auto size = getSize();
        if (size.first == 0 || size.second == 0) {
            Logger::get().error("failed to get window size", sWindowLogCategory);
            return std::make_pair(0.0f, 0.0f);
        }

        return std::make_pair(
            static_cast<float>(xPos) / static_cast<float>(size.first),
            static_cast<float>(yPos) / static_cast<float>(size.second));
    }

    std::string Window::getTitle() const { return sWindowTitle; }

    float Window::getOpacity() const { return glfwGetWindowOpacity(pGlfwWindow); }

    Renderer* Window::getRenderer() const {
        if (pGame && pGame->pRenderer) {
            return pGame->pRenderer.get();
        }

        return nullptr;
    }

#if defined(WIN32)
    HWND Window::getWindowHandle() const { return glfwGetWin32Window(pGlfwWindow); }
#endif

    void Window::onKeyboardInput(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) const {
        if (!pGame) {
            return;
        }
        pGame->onKeyboardInput(key, modifiers, bIsPressedDown);
    }

    void Window::onMouseInput(MouseButton button, KeyboardModifiers modifiers, bool bIsPressedDown) const {
        if (!pGame) {
            return;
        }

        pGame->onMouseInput(button, modifiers, bIsPressedDown);
    }

    void Window::onMouseMove(int iXPos, int iYPos) {
        if (pGame) {
            const int iDeltaX = iXPos - iLastMouseXPos;
            const int iDeltaY = iLastMouseYPos - iYPos;
            pGame->onMouseMove(iDeltaX, iDeltaY);
        }

        iLastMouseXPos = static_cast<int>(iXPos);
        iLastMouseYPos = static_cast<int>(iYPos);
    }

    void Window::onMouseScrollMove(int iOffset) const {
        if (!pGame) {
            return;
        }

        pGame->onMouseScrollMove(iOffset);
    }

    void Window::onWindowFocusChanged(bool bIsFocused) const {
        if (!pGame) {
            return;
        }
        pGame->onWindowFocusChanged(bIsFocused);
    }

    void Window::glfwWindowKeyboardCallback(
        GLFWwindow* pGlfwWindow, int iKey, int iScancode, int iAction, int iMods) {
        if (iAction == GLFW_REPEAT) {
            return;
        }

        const Window* pWindow = static_cast<Window*>(glfwGetWindowUserPointer(pGlfwWindow));
        if (!pWindow) {
            return;
        }

        pWindow->onKeyboardInput(
            static_cast<KeyboardKey>(iKey), KeyboardModifiers(iMods), iAction == GLFW_PRESS ? true : false);
    }

    void Window::glfwWindowMouseCallback(GLFWwindow* pGlfwWindow, int iButton, int iAction, int iMods) {
        const Window* pWindow = static_cast<Window*>(glfwGetWindowUserPointer(pGlfwWindow));
        if (!pWindow) {
            return;
        }

        pWindow->onMouseInput(
            static_cast<MouseButton>(iButton),
            KeyboardModifiers(iMods),
            iAction == GLFW_PRESS ? true : false);
    }

    void Window::glfwWindowFocusCallback(GLFWwindow* pGlfwWindow, int iFocused) {
        const Window* pWindow = static_cast<Window*>(glfwGetWindowUserPointer(pGlfwWindow));
        if (!pWindow) {
            return;
        }

        pWindow->onWindowFocusChanged(static_cast<bool>(iFocused));
    }

    void Window::glfwWindowMouseCursorPosCallback(GLFWwindow* pGlfwWindow, double xPos, double yPos) {
        auto* pWindow = static_cast<Window*>(glfwGetWindowUserPointer(pGlfwWindow));
        if (!pWindow) {
            return;
        }

        pWindow->onMouseMove(static_cast<int>(xPos), static_cast<int>(yPos));
    }

    void Window::glfwWindowMouseScrollCallback(GLFWwindow* pGlfwWindow, double xOffset, double yOffset) {
        const Window* pWindow = static_cast<Window*>(glfwGetWindowUserPointer(pGlfwWindow));
        if (!pWindow) {
            return;
        }

        pWindow->onMouseScrollMove(static_cast<int>(yOffset));
    }

    void Window::showErrorIfNotOnMainThread() const {
        const auto currentThreadId = std::this_thread::get_id();
        if (currentThreadId != mainThreadId) {
            std::stringstream currentThreadIdString;
            currentThreadIdString << currentThreadId;

            std::stringstream mainThreadIdString;
            mainThreadIdString << mainThreadId;

            Error err(fmt::format(
                "an attempt was made to call a function that should only be called on the main thread "
                "in a non main thread (main thread ID: {}, current thread ID: {})",
                mainThreadIdString.str(),
                currentThreadIdString.str()));
            err.showError();
            throw std::runtime_error(err.getError());
        }
    }

    std::variant<std::unique_ptr<Window>, Error> Window::create(WindowBuilderParameters& params) {
        GLFW::get(); // initialize GLFW

        std::string sNewWindowTitle(params.sWindowTitle);

        // Check window name.
        if (sNewWindowTitle.empty()) {
            sNewWindowTitle = UniqueValueGenerator::get().getUniqueWindowName();
        }

        // Check fullscreen mode (windowed fullscreen).
        GLFWmonitor* pMonitor = nullptr;
        if (params.bFullscreen) {
            pMonitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(pMonitor);

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
        GLFWwindow* pGLFWWindow = glfwCreateWindow(
            params.iWindowWidth, params.iWindowHeight, sNewWindowTitle.c_str(), pMonitor, nullptr);
        if (!pGLFWWindow) {
            return Error("failed to create window");
        }

        auto pWindow = std::unique_ptr<Window>(new Window(pGLFWWindow, sNewWindowTitle));

        // Set icon.
        if (std::filesystem::exists(params.pathToWindowIcon)) {
            auto error = pWindow->setIcon(params.pathToWindowIcon);
            if (error.has_value()) {
                error->addEntry();
                error->showError();
                // don't throw here, not a critical error.
            }
        }

        // Add Window pointer.
        glfwSetWindowUserPointer(pGLFWWindow, pWindow.get());

        // Bind to keyboard input.
        glfwSetKeyCallback(pGLFWWindow, Window::glfwWindowKeyboardCallback);

        // Bind to mouse input.
        glfwSetMouseButtonCallback(pGLFWWindow, Window::glfwWindowMouseCallback);

        // Bind to mouse move.
        glfwSetCursorPosCallback(pGLFWWindow, Window::glfwWindowMouseCursorPosCallback);

        // Bind to mouse scroll move.
        glfwSetScrollCallback(pGLFWWindow, Window::glfwWindowMouseScrollCallback);

        // Bind to focus change event.
        glfwSetWindowFocusCallback(pGLFWWindow, Window::glfwWindowFocusCallback);

        return pWindow;
    }

    Window::~Window() {
        // Destroy created cursors.
        for (const auto& pCursor : vCreatedCursors) {
            pCursor->releaseCursor();
        }
        vCreatedCursors.clear();

        // Destroy window.
        glfwDestroyWindow(pGlfwWindow);
    }

    WindowBuilder Window::getBuilder() { return {}; }

    void Window::show() const { glfwShowWindow(pGlfwWindow); }

    void Window::setOpacity(float fOpacity) const { glfwSetWindowOpacity(pGlfwWindow, fOpacity); }

    void Window::setTitle(const std::string& sNewTitle) {
        glfwSetWindowTitle(pGlfwWindow, sNewTitle.c_str());
        sWindowTitle = sNewTitle;
    }

    std::optional<Error> Window::setIcon(std::filesystem::path pathToIcon) const {
        showErrorIfNotOnMainThread();

        if (!std::filesystem::exists(pathToIcon)) {
            return Error(fmt::format("the specified file \"{}\" does not exist.", pathToIcon.string()));
        }

        // Load image.
        GLFWimage images[1];
        images[0].pixels =
            stbi_load(pathToIcon.string().data(), &images[0].width, &images[0].height, nullptr, 4);

        // Set icon.
        glfwSetWindowIcon(pGlfwWindow, 1, images);

        // Free loaded image.
        stbi_image_free(images[0].pixels);

        return {};
    }

    std::variant<WindowCursor*, Error> Window::createCursor(std::filesystem::path pathToIcon) {
        showErrorIfNotOnMainThread();

        // Create new cursor.
        auto result = WindowCursor::create(pathToIcon);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(result);
            error.addEntry();
            return error;
        }

        // Save cursor.
        auto pCursor = std::get<std::unique_ptr<WindowCursor>>(std::move(result));
        auto pRawCursor = pCursor.get();
        vCreatedCursors.push_back(std::move(pCursor));

        return pRawCursor;
    }

    void Window::setCursor(WindowCursor* pCursor) {
        showErrorIfNotOnMainThread();

        glfwSetCursor(pGlfwWindow, pCursor->getCursor());
    }

    void Window::setDefaultCursor() { glfwSetCursor(pGlfwWindow, nullptr); }

    void Window::setCursorVisibility(const bool bIsVisible) const {
        if (bIsVisible) {
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(pGlfwWindow, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            }
            glfwSetInputMode(pGlfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(pGlfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(pGlfwWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            } else {
                Logger::get().warn("raw mouse motion is not supported", sWindowLogCategory);
            }
        }
    }

    void Window::minimize() const { glfwIconifyWindow(pGlfwWindow); }

    void Window::maximize() const { glfwMaximizeWindow(pGlfwWindow); }

    void Window::restore() const { glfwRestoreWindow(pGlfwWindow); }

    Window::Window(GLFWwindow* pGlfwWindow, const std::string& sWindowTitle) {
        this->pGlfwWindow = pGlfwWindow;
        this->sWindowTitle = sWindowTitle;

        // Save ID of this thread (should be main thread).
        mainThreadId = std::this_thread::get_id();
    }

    WindowCursor::~WindowCursor() {
        if (pCursor) {
            Logger::get().error(
                "previously created window cursor is being destroyed but internal GLFW cursor object was not "
                "released (you should release it manually)",
                "");
        }
    }

    std::variant<std::unique_ptr<WindowCursor>, Error>
    WindowCursor::create(std::filesystem::path pathToIcon) {
        if (!std::filesystem::exists(pathToIcon)) {
            return Error(fmt::format("the specified file \"{}\" does not exist.", pathToIcon.string()));
        }

        // Load image.
        GLFWimage image;
        image.pixels = stbi_load(pathToIcon.string().data(), &image.width, &image.height, nullptr, 4);

        // Create cursor.
        auto pGlfwCursor = glfwCreateCursor(&image, 0, 0);
        if (!pGlfwCursor) {
            return Error("failed to create a cursor object");
        }

        auto pCursor = std::unique_ptr<WindowCursor>(new WindowCursor(pGlfwCursor));

        // Free loaded image.
        stbi_image_free(image.pixels);

        return pCursor;
    }

    void WindowCursor::releaseCursor() {
        glfwDestroyCursor(pCursor);
        pCursor = nullptr;
    }

    GLFWcursor* WindowCursor::getCursor() const { return pCursor; }

    WindowCursor::WindowCursor(GLFWcursor* pCursor) { this->pCursor = pCursor; }

} // namespace ne
