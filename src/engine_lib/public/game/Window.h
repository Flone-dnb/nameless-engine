#pragma once

// Standard.
#include <variant>
#include <memory>
#include <optional>
#include <utility>

// Custom.
#include "misc/Error.h"
#include "game/Game.h"
#include "render/Renderer.h"
#include "window/GLFW.hpp"
#include "game/GameInstance.h"
#include "input/KeyboardKey.hpp"
#include "input/MouseButton.hpp"

namespace ne {
    class Error;
    class GameInstance;

    /**
     * Represents a custom window cursor.
     */
    class WindowCursor {
    public:
        WindowCursor() = delete;
        WindowCursor(const WindowCursor&) = delete;
        WindowCursor& operator=(const WindowCursor&) = delete;

        /** Checks if the created cursor was released and if not, logs an error. */
        ~WindowCursor();

    protected:
        // Only Window can directly create instances of this class.
        // For users we provide a separate function.
        friend class Window;

        /**
         * Loads image and creates a new cursor.
         *
         * @warning This function must only be called from the main thread.
         *
         * @remark You should call @ref releaseCursor when you no longer need this cursor.
         *
         * @param pathToIcon  Path to the image (.png).
         * The image data should be 32-bit, little-endian, non-premultiplied RGBA,
         * i.e. eight bits per channel with the red channel first.
         *
         * @return Error if something went wrong, otherwise created cursor.
         */
        static std::variant<std::unique_ptr<WindowCursor>, Error> create(std::filesystem::path pathToIcon);

        /**
         * Releases existing cursor.
         *
         * @warning This function must only be called from the main thread.
         */
        void releaseCursor();

        /**
         * Returns internal GLFW cursor.
         *
         * @return Internal GLFW cursor.
         */
        GLFWcursor* getCursor() const;

    private:
        /**
         * Initializes window cursor.
         *
         * @param pCursor Created GLFW cursor.
         */
        WindowCursor(GLFWcursor* pCursor);

        /** Created GLFW cursor. */
        GLFWcursor* pCursor = nullptr;
    };

    /**
     * Parameters needed to build a window.
     */
    struct WindowBuilderParameters {
        /** Width of a window. */
        int iWindowWidth = 800;
        /** Height of a window. */
        int iWindowHeight = 600;
        /** Title of a window. */
        std::string_view sWindowTitle;
        /** Icon of a window. */
        std::filesystem::path pathToWindowIcon;
        /** Whether to show window after it was created or not. */
        bool bShowWindow = true;
        /** Whether the window should be maximized after creation or not. */
        bool bMaximized = false;
        /** Whether to show window in fullscreen mode. */
        bool bFullscreen = false;
        /** Whether the window should have window decorations. */
        bool bIsSplashScreen = false;
    };

    class Window;

    /**
     * Builder pattern class for Window.
     */
    class WindowBuilder {
    public:
        WindowBuilder() = default;

        /**
         * Defines the size of a window that we will create.
         *
         * @param iWidth  Width of the window.
         * @param iHeight Height of the window.
         *
         * @return Builder.
         */
        WindowBuilder& withSize(int iWidth, int iHeight);

        /**
         * Defines the name of a window that we will create.
         *
         * @param sWindowTitle Title of the window.
         *
         * @return Builder.
         */
        WindowBuilder& withTitle(std::string_view sWindowTitle);

        /**
         * Defines the icon of a window that we will create.
         *
         * @param pathToIcon Path to the image (.png).
         *
         * @return Builder.
         */
        WindowBuilder& withIcon(std::filesystem::path pathToIcon);

        /**
         * Defines the visibility of a window that we will create.
         * Does nothing for fullscreen windows.
         *
         * @param bShow Visibility of the window.
         *
         * @return Builder.
         */
        WindowBuilder& withVisibility(bool bShow);

        /**
         * Whether the window should be maximized after creation or not.
         * Does nothing for fullscreen windows.
         *
         * @param bMaximized Should window be maximized or not.
         *
         * @return Builder.
         */
        WindowBuilder& withMaximizedState(bool bMaximized);

        /**
         * Whether the window should look like a splash screen or not
         * (no border, title, buttons, etc).
         * Does nothing for fullscreen windows.
         *
         * @param bIsSplashScreen Should window look like a splash screen or not.
         *
         * @return Builder.
         */
        WindowBuilder& withSplashScreenMode(bool bIsSplashScreen);

        /**
         * Whether a window should be shown in the fullscreen mode or not.
         *
         * @param bEnableFullscreen Fullscreen mode.
         *
         * @return Builder.
         */
        WindowBuilder& withFullscreenMode(bool bEnableFullscreen);

        /**
         * Builds/creates a new window with the configured parameters.
         *
         * @return Returns error if something went wrong or created window otherwise.
         *
         * @warning This function should only be called from the main thread.
         */
        std::variant<std::unique_ptr<Window>, Error> build();

    private:
        /** Configured window parameters. */
        WindowBuilderParameters params;
    };

    /**
     * Describes a window.
     */
    class Window {
    public:
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        virtual ~Window();

        /**
         * Returns a builder for a new window.
         * Use can use this static function to receive a \ref WindowBuilder
         * or create an instance of \ref WindowBuilder manually.
         *
         * @return Builder for a new window.
         */
        static WindowBuilder getBuilder();

        /**
         * Starts the message queue, rendering and game logic.
         * Set GameInstance derived class to react to
         * user inputs, window events and etc.
         *
         * Will return control after the window was closed.
         */
        template <typename MyGameInstance>
        requires std::derived_from<MyGameInstance, GameInstance>
        void processEvents();

        /**
         * Sets the window opacity (1.0f for opaque, 0.0f for transparent).
         * Does nothing if the OS does not support transparent windows.
         *
         * @param fOpacity Opacity value between 0.0 and 1.0.
         */
        void setOpacity(float fOpacity) const;

        /**
         * Sets new window title.
         *
         * @param sNewTitle New window title.
         */
        void setTitle(const std::string& sNewTitle);

        /**
         * Sets new window icon.
         *
         * @warning This function must only be called from the main thread. If this function is called
         * outside of the main thread an error will be shown.
         *
         * @param pathToIcon Path to the image (.png).
         * The image data should be 32-bit, little-endian, non-premultiplied RGBA,
         * i.e. eight bits per channel with the red channel first.
         *
         * @return Error if file not found.
         */
        std::optional<Error> setIcon(std::filesystem::path pathToIcon) const;

        /**
         * Loads the image and creates a new cursor, note that in order for this new
         * cursor to be visible you have to call @ref setCursor.
         *
         * @warning This function must only be called from the main thread. If this function is called
         * outside of the main thread an error will be shown.
         *
         * @param pathToIcon Path to the image (.png).
         * The image data should be 32-bit, little-endian, non-premultiplied RGBA,
         * i.e. eight bits per channel with the red channel first.
         *
         * @return Error if something went wrong, otherwise a non-owning pointer to
         * the created cursor that is managed by the window. Do not delete returned pointer,
         * the window owns this cursor and will automatically delete it when the window is closed.
         */
        std::variant<WindowCursor*, Error> createCursor(std::filesystem::path pathToIcon);

        /**
         * Changes the cursor.
         *
         * Use @ref createCursor to create a new cursor.
         *
         * @warning This function must only be called from the main thread. If this function is called
         * outside of the main thread an error will be shown.
         *
         * @param pCursor Cursor to use.
         */
        void setCursor(WindowCursor* pCursor);

        /**
         * Changes the cursor icon to be system default cursor icon.
         *
         * This function can be used to revert changes made by @ref setCursor function.
         */
        void setDefaultCursor();

        /**
         * Whether the cursor is visible or not (locked in this window).
         *
         * @param bIsVisible 'true' shows cursor (normal behavior),
         * 'false' will hide the cursor and lock it to the window.
         */
        void setCursorVisibility(bool bIsVisible) const;

        /**
         * Minimizes the window.
         */
        void minimize() const;

        /**
         * Maximizes the window.
         */
        void maximize() const;

        /**
         * Restores the window (makes it visible with normal size).
         * Does nothing for fullscreen windows.
         */
        void restore() const;

        /**
         * Hides the windows (makes it invisible).
         * Does nothing for fullscreen windows.
         */
        void hide() const;

        /**
         * Shows the hidden window on screen.
         */
        void show() const;

        /**
         * Closes this window causing game instance,
         * audio engine and etc to be destroyed.
         */
        void close() const;

        /**
         * Returns the current window size in pixels.
         *
         * @warning This function must only be called from the main thread. If this function is called
         * outside of the main thread an error will be shown.
         *
         * @return A pair of width and height in pixels.
         */
        std::pair<int, int> getSize() const;

        /**
         * Returns the current cursor position on window.
         *
         * @warning This function must only be called from the main thread. If this function is called
         * outside of the main thread an error will be shown.
         *
         * @return A pair of X and Y coordinates in range [0.0; 1.0]
         * relative to the upper-left corner of the window.
         */
        std::pair<float, float> getCursorPosition() const;

        /**
         * Returns the title of this window.
         *
         * @return Title of the window.
         */
        std::string getTitle() const;

        /**
         * Returns window opacity.
         *
         * @return Window opacity.
         */
        float getOpacity() const;

        /**
         * Returns renderer used for this window.
         *
         * @return nullptr if renderer was not created yet, valid pointer otherwise.
         */
        Renderer* getRenderer() const;

#if defined(WIN32)
        /**
         * Returns native Windows handle to this window.
         *
         * @return Window handle or NULL if an error occurred.
         */
        HWND getWindowHandle() const;
#endif

        /**
         * Called when the window receives keyboard input.
         *
         * @remark Made public so you can simulate input in your tests.
         *
         * @param key            Keyboard key.
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the key down event occurred or key up.
         */
        void onKeyboardInput(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) const;

        /**
         * Called when the window receives mouse input.
         *
         * @remark Made public so you can simulate input in your tests.
         *
         * @param button         Mouse button.
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the button down event occurred or button up.
         */
        void onMouseInput(MouseButton button, KeyboardModifiers modifiers, bool bIsPressedDown) const;

        /**
         * Called when the window receives mouse scroll movement.
         *
         * @remark Made public so you can simulate input in your tests.
         *
         * @param iOffset Movement offset.
         */
        void onMouseScrollMove(int iOffset) const;

    private:
        friend class WindowBuilder;

        /**
         * GLFW callback.
         *
         * @param pGlfwWindow The window that received the event.
         * @param iKey        The keyboard key that was pressed or released.
         * @param iScancode   The system-specific scancode of the key.
         * @param iAction     GLFW_PRESS, GLFW_RELEASE or GLFW_REPEAT.
         * @param iMods       Bit field describing which modifier keys were held down.
         */
        static void
        glfwWindowKeyboardCallback(GLFWwindow* pGlfwWindow, int iKey, int iScancode, int iAction, int iMods);

        /**
         * GLFW callback.
         *
         * @param pGlfwWindow The window that received the event.
         * @param iButton     The mouse button that was pressed or released.
         * @param iAction     One of GLFW_PRESS or GLFW_RELEASE.
         * @param iMods 	     Bit field describing which modifier keys were held down.
         */
        static void glfwWindowMouseCallback(GLFWwindow* pGlfwWindow, int iButton, int iAction, int iMods);

        /**
         * GLFW callback.
         *
         * @param pGlfwWindow The window that received the event.
         * @param iFocused    GLFW_TRUE if the window was given input focus, or GLFW_FALSE if it lost it.
         */
        static void glfwWindowFocusCallback(GLFWwindow* pGlfwWindow, int iFocused);

        /**
         * GLFW callback.
         *
         * @param pGlfwWindow The window that received the event.
         * @param xPos        The new x-coordinate, in screen coordinates, of the cursor.
         * @param yPos        The new y-coordinate, in screen coordinates, of the cursor.
         */
        static void glfwWindowMouseCursorPosCallback(GLFWwindow* pGlfwWindow, double xPos, double yPos);

        /**
         * GLFW callback.
         *
         * @param pGlfwWindow The window that received the event.
         * @param xOffset     The scroll offset along the x-axis.
         * @param yOffset     The scroll offset along the y-axis.
         */
        static void glfwWindowMouseScrollCallback(GLFWwindow* pGlfwWindow, double xOffset, double yOffset);

        /**
         * Checks whether the current thread is the main thread or not and if not
         * shows an error.
         */
        void showErrorIfNotOnMainThread() const;

        /**
         * Called when the window receives mouse movement.
         *
         * @param iXPos    Mouse X position in pixels.
         * @param iYPos    Mouse Y position in pixels.
         */
        void onMouseMove(int iXPos, int iYPos);

        /**
         * Called when the window focus was changed.
         *
         * @param bIsFocused  Whether the window has gained or lost the focus.
         */
        void onWindowFocusChanged(bool bIsFocused) const;

        /**
         * Creates a new window.
         *
         * @param params Parameters that define initial window state.
         *
         * @return Returns error if something went wrong or created window otherwise.
         */
        static std::variant<std::unique_ptr<Window>, Error> newInstance(WindowBuilderParameters& params);

        /**
         * Default constructor.
         *
         * @param pGlfwWindow   Created GLFW window.
         * @param sWindowTitle  Title of this window.
         */
        Window(GLFWwindow* pGlfwWindow, const std::string& sWindowTitle);

        /** Holds main game objects. */
        std::unique_ptr<Game> pGame;

        /** GLFW window. */
        GLFWwindow* pGlfwWindow = nullptr;

        /** Array of created cursors using @ref createCursor. Should be used with the mutex. */
        std::vector<std::unique_ptr<WindowCursor>> vCreatedCursors;

        /** ID of the main thread. */
        std::thread::id mainThreadId;

        /** Title of the window. */
        std::string sWindowTitle;

        /** Last mouse X position, used for calculating delta movement. */
        int iLastMouseXPos = 0;
        /** Last mouse Y position, used for calculating delta movement. */
        int iLastMouseYPos = 0;

        /** Name of the category used for logging. */
        inline static const char* sWindowLogCategory = "Window";
    };

    template <typename MyGameInstance>
    requires std::derived_from<MyGameInstance, GameInstance>
    void Window::processEvents() {
        pGame = std::unique_ptr<Game>(new Game(this));

        // ... initialize other Game fields here ...

        // Finally create Game Instance when engine (Game) is fully initialized.
        // So that the user can call engine functions in Game Instance constructor.
        pGame->setGameInstance<MyGameInstance>();

        // Used for tick.
        float fCurrentTimeInSec = 0.0f;
        float fPrevTimeInSec = static_cast<float>(glfwGetTime());

        while (!glfwWindowShouldClose(pGlfwWindow)) {
            // Process window events.
            glfwPollEvents();

            // Tick.
            fCurrentTimeInSec = static_cast<float>(glfwGetTime());
            pGame->onBeforeNewFrame(fCurrentTimeInSec - fPrevTimeInSec);
            fPrevTimeInSec = fCurrentTimeInSec;

            // Draw next frame.
            getRenderer()->drawNextFrame();

            // Notify finish.
            pGame->onTickFinished();
        }

        pGame->onWindowClose();
        pGame = nullptr; // explicitly destroy here to run GC for the last time (before everything else is
                         // destroyed)
    }
} // namespace ne
