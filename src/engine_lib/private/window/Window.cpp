#include "game/Window.h"

// Standard.
#include <format>
#include <filesystem>

// Custom.
#include "misc/UniqueValueGenerator.h"
#include "io/Logger.h"

// External.
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

    WindowBuilder& WindowBuilder::withIcon(const std::filesystem::path& pathToIcon) {
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

    void Window::hide() const {
        showErrorIfNotOnMainThread();
        glfwHideWindow(pGlfwWindow);
    }

    void Window::close() const { glfwSetWindowShouldClose(pGlfwWindow, 1); }

    std::pair<int, int> Window::getSize() const {
        showErrorIfNotOnMainThread();

        int iWidth = -1;
        int iHeight = -1;

        glfwGetWindowSize(pGlfwWindow, &iWidth, &iHeight);

        return std::make_pair(iWidth, iHeight);
    }

    std::pair<float, float> Window::getCursorPosition() const {
        showErrorIfNotOnMainThread();

        double xPos = 0.0;
        double yPos = 0.0;
        glfwGetCursorPos(pGlfwWindow, &xPos, &yPos);

        const auto size = getSize();
        if (size.first == 0 || size.second == 0) {
            Logger::get().error("failed to get window size");
            return std::make_pair(0.0F, 0.0F);
        }

        return std::make_pair(
            static_cast<float>(xPos) / static_cast<float>(size.first),
            static_cast<float>(yPos) / static_cast<float>(size.second));
    }

    std::string Window::getTitle() const { return sWindowTitle; }

    float Window::getOpacity() const { return glfwGetWindowOpacity(pGlfwWindow); }

    Renderer* Window::getRenderer() const {
        if (pGameManager != nullptr && pGameManager->pRenderer != nullptr) {
            return pGameManager->pRenderer.get();
        }

        return nullptr;
    }

#if defined(WIN32)
    HWND Window::getWindowHandle() const { return glfwGetWin32Window(pGlfwWindow); }
#endif

    GLFWwindow* Window::getGlfwWindow() const { return pGlfwWindow; }

    void Window::onKeyboardInput(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) const {
        pGameManager->onKeyboardInput(key, modifiers, bIsPressedDown);
    }

    void Window::onMouseInput(MouseButton button, KeyboardModifiers modifiers, bool bIsPressedDown) const {
        pGameManager->onMouseInput(button, modifiers, bIsPressedDown);
    }

    void Window::onMouseMove(int iXPos, int iYPos) {
        const int iDeltaX = iXPos - iLastMouseXPos;
        const int iDeltaY = iLastMouseYPos - iYPos;
        pGameManager->onMouseMove(iDeltaX, iDeltaY);

        iLastMouseXPos = static_cast<int>(iXPos);
        iLastMouseYPos = static_cast<int>(iYPos);
    }

    void Window::onMouseScrollMove(int iOffset) const { pGameManager->onMouseScrollMove(iOffset); }

    void Window::onWindowFocusChanged(bool bIsFocused) const {
        pGameManager->onWindowFocusChanged(bIsFocused);
    }

    void Window::onFramebufferSizeChanged(int iWidth, int iHeight) const {
        getRenderer()->onFramebufferSizeChanged(iWidth, iHeight);
        pGameManager->onFramebufferSizeChanged(iWidth, iHeight);
    }

    void Window::glfwWindowKeyboardCallback(
        GLFWwindow* pGlfwWindow, int iKey, int iScancode, int iAction, int iMods) {
        if (iAction == GLFW_REPEAT) {
            return;
        }

        const Window* pWindow = static_cast<Window*>(glfwGetWindowUserPointer(pGlfwWindow));
        if (pWindow == nullptr) [[unlikely]] {
            return;
        }

        pWindow->onKeyboardInput(
            static_cast<KeyboardKey>(iKey), KeyboardModifiers(iMods), iAction == GLFW_PRESS);
    }

    void Window::glfwWindowMouseCallback(GLFWwindow* pGlfwWindow, int iButton, int iAction, int iMods) {
        const Window* pWindow = static_cast<Window*>(glfwGetWindowUserPointer(pGlfwWindow));
        if (pWindow == nullptr) [[unlikely]] {
            return;
        }

        pWindow->onMouseInput(
            static_cast<MouseButton>(iButton), KeyboardModifiers(iMods), iAction == GLFW_PRESS);
    }

    void Window::glfwWindowFocusCallback(GLFWwindow* pGlfwWindow, int iFocused) {
        const Window* pWindow = static_cast<Window*>(glfwGetWindowUserPointer(pGlfwWindow));
        if (pWindow == nullptr) [[unlikely]] {
            return;
        }

        pWindow->onWindowFocusChanged(static_cast<bool>(iFocused));
    }

    void Window::glfwWindowMouseCursorPosCallback(GLFWwindow* pGlfwWindow, double xPos, double yPos) {
        auto* pWindow = static_cast<Window*>(glfwGetWindowUserPointer(pGlfwWindow));
        if (pWindow == nullptr) [[unlikely]] {
            return;
        }

        pWindow->onMouseMove(static_cast<int>(xPos), static_cast<int>(yPos));
    }

    void Window::glfwWindowMouseScrollCallback(GLFWwindow* pGlfwWindow, double xOffset, double yOffset) {
        const Window* pWindow = static_cast<Window*>(glfwGetWindowUserPointer(pGlfwWindow));
        if (pWindow == nullptr) [[unlikely]] {
            return;
        }

        pWindow->onMouseScrollMove(static_cast<int>(yOffset));
    }

    void Window::glfwFramebufferResizeCallback(GLFWwindow* pGlfwWindow, int iWidth, int iHeight) {
        const Window* pWindow = static_cast<Window*>(glfwGetWindowUserPointer(pGlfwWindow));
        if (pWindow == nullptr) [[unlikely]] {
            return;
        }

        pWindow->onFramebufferSizeChanged(iWidth, iHeight);
    }

    void Window::bindToWindowEvents() {
        // Make sure window is created.
        if (pGlfwWindow == nullptr) [[unlikely]] {
            Error error("expected window to be created at this point");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure game manager is created because input callbacks will use game manager.
        if (pGameManager == nullptr) [[unlikely]] {
            Error error("expected game manager to be created at this point");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure game instance is created because input callbacks will use game instance.
        if (pGameManager->getGameInstance() == nullptr) [[unlikely]] {
            Error error("expected game instance to be created at this point");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Bind to keyboard input.
        glfwSetKeyCallback(pGlfwWindow, Window::glfwWindowKeyboardCallback);

        // Bind to mouse input.
        glfwSetMouseButtonCallback(pGlfwWindow, Window::glfwWindowMouseCallback);

        // Bind to mouse move.
        glfwSetCursorPosCallback(pGlfwWindow, Window::glfwWindowMouseCursorPosCallback);

        // Bind to mouse scroll move.
        glfwSetScrollCallback(pGlfwWindow, Window::glfwWindowMouseScrollCallback);

        // Bind to focus change event.
        glfwSetWindowFocusCallback(pGlfwWindow, Window::glfwWindowFocusCallback);

        // Bind to framebuffer size change event.
        glfwSetFramebufferSizeCallback(pGlfwWindow, Window::glfwFramebufferResizeCallback);

        // ... add new callbacks here ...
        // ... unbind from new callbacks in `unbindFromWindowEvents` ...
    }

    void Window::unbindFromWindowEvents() {
        // Make sure window is created.
        if (pGlfwWindow == nullptr) [[unlikely]] {
            Error error("expected window to be created at this point");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure game manager is created because input callbacks will use game manager.
        if (pGameManager == nullptr) [[unlikely]] {
            Error error("expected game manager to be created at this point");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure game instance is created because input callbacks will use game instance.
        if (pGameManager->getGameInstance() == nullptr) [[unlikely]] {
            Error error("expected game instance to be created at this point");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Unbind from keyboard input.
        glfwSetKeyCallback(pGlfwWindow, nullptr);

        // Unbind from mouse input.
        glfwSetMouseButtonCallback(pGlfwWindow, nullptr);

        // Unbind from mouse move.
        glfwSetCursorPosCallback(pGlfwWindow, nullptr);

        // Unbind from mouse scroll move.
        glfwSetScrollCallback(pGlfwWindow, nullptr);

        // Unbind from focus change event.
        glfwSetWindowFocusCallback(pGlfwWindow, nullptr);

        // Unbind from framebuffer size change event.
        glfwSetFramebufferSizeCallback(pGlfwWindow, nullptr);
    }

    void Window::showErrorIfNotOnMainThread() const {
        const auto currentThreadId = std::this_thread::get_id();
        if (currentThreadId != mainThreadId) {
            std::stringstream currentThreadIdString;
            currentThreadIdString << currentThreadId;

            std::stringstream mainThreadIdString;
            mainThreadIdString << mainThreadId;

            Error err(std::format(
                "an attempt was made to call a function that should only be called on the main thread "
                "in a non main thread (main thread ID: {}, current thread ID: {})",
                mainThreadIdString.str(),
                currentThreadIdString.str()));
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
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
            const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);

            // Use monitor size for window.
            params.iWindowWidth = pMode->width;
            params.iWindowHeight = pMode->height;

            glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

#if defined(WIN32)
            // We want to specify `nullptr` as monitor on Windows to have windowed fullscreen mode,
            // it will look exactly as a regular fullscreen window.
            // On Linux we need to specify the monitor to make the window look fullscreen.
            pMonitor = nullptr;
#endif
        } else if (params.bIsSplashScreen) {
            glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        }

        if (params.bMaximized) {
            glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
        }

        if (!params.bShowWindow) {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        }

        // Don't create any OpenGL contexts.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        // Create GLFW window.
        auto pGLFWWindow = glfwCreateWindow(
            params.iWindowWidth, params.iWindowHeight, sNewWindowTitle.c_str(), pMonitor, nullptr);
        if (pGLFWWindow == nullptr) {
            return Error("failed to create window");
        }

#if defined(WIN32)
        if (params.bFullscreen) {
            // Make window overlap taskbar.
            HWND pHWindow = glfwGetWin32Window(pGLFWWindow);
            SetWindowPos(pHWindow, HWND_TOP, 0, 0, params.iWindowWidth, params.iWindowHeight, 0);
        }
#endif

        auto pWindow = std::unique_ptr<Window>(new Window(pGLFWWindow, sNewWindowTitle));

        // Set icon.
        if (std::filesystem::exists(params.pathToWindowIcon)) {
            auto error = pWindow->setIcon(params.pathToWindowIcon);
            if (error.has_value()) {
                error->addCurrentLocationToErrorStack();
                error->showError();
                // don't throw here, not a critical error.
            }
        }

        // Add Window pointer.
        glfwSetWindowUserPointer(pGLFWWindow, pWindow.get());

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

    void Window::setPreferredRenderer(const std::optional<RendererType>& preferredRenderer) {
        this->preferredRenderer = preferredRenderer;
    }

    void Window::show() const { glfwShowWindow(pGlfwWindow); }

    void Window::setOpacity(float opacity) const { glfwSetWindowOpacity(pGlfwWindow, opacity); }

    void Window::setTitle(const std::string& sNewTitle) {
        glfwSetWindowTitle(pGlfwWindow, sNewTitle.c_str());
        sWindowTitle = sNewTitle;
    }

    std::optional<Error> Window::setIcon(const std::filesystem::path& pathToIcon) const {
        showErrorIfNotOnMainThread();

        if (!std::filesystem::exists(pathToIcon)) {
            return Error(std::format("the specified file \"{}\" does not exist.", pathToIcon.string()));
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

    std::variant<WindowCursor*, Error> Window::createCursor(const std::filesystem::path& pathToIcon) {
        showErrorIfNotOnMainThread();

        // Create new cursor.
        auto result = WindowCursor::create(pathToIcon);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(result);
            error.addCurrentLocationToErrorStack();
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
            if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
                glfwSetInputMode(pGlfwWindow, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            }
            glfwSetInputMode(pGlfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(pGlfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
                glfwSetInputMode(pGlfwWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            } else {
                Logger::get().warn("raw mouse motion is not supported");
            }
        }
    }

    void Window::minimize() const {
        showErrorIfNotOnMainThread();
        glfwIconifyWindow(pGlfwWindow);
    }

    void Window::maximize() const {
        showErrorIfNotOnMainThread();
        glfwMaximizeWindow(pGlfwWindow);
    }

    void Window::restore() const {
        showErrorIfNotOnMainThread();
        glfwRestoreWindow(pGlfwWindow);
    }

    Window::Window(GLFWwindow* pGlfwWindow, const std::string& sWindowTitle) {
        this->pGlfwWindow = pGlfwWindow;
        this->sWindowTitle = sWindowTitle;

        // Save ID of this thread (should be main thread).
        mainThreadId = std::this_thread::get_id();
    }

    WindowCursor::~WindowCursor() {
        if (pCursor != nullptr) {
            Logger::get().error(
                "previously created window cursor is being destroyed but internal GLFW cursor object was not "
                "released (you should release it manually)");
        }
    }

    std::variant<std::unique_ptr<WindowCursor>, Error>
    WindowCursor::create(const std::filesystem::path& pathToIcon) {
        if (!std::filesystem::exists(pathToIcon)) {
            return Error(std::format("the specified file \"{}\" does not exist.", pathToIcon.string()));
        }

        // Load image.
        GLFWimage image;
        image.pixels = stbi_load(pathToIcon.string().data(), &image.width, &image.height, nullptr, 4);

        // Create cursor.
        auto pGlfwCursor = glfwCreateCursor(&image, 0, 0);
        if (pGlfwCursor == nullptr) {
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
