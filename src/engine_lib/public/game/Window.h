#pragma once

// STL.
#include <variant>
#include <memory>
#include <optional>
#include <utility>

// Custom.
#include "misc/Error.h"
#include "game/Game.h"
#include "window/GLFW.hpp"
#include "IGameInstance.h"
#include "input/KeyboardKey.hpp"
#include "input/MouseButton.hpp"

namespace ne {
    class Error;
    class IGameInstance;

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
        std::string_view sPathToWindowIcon;
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
         * @param sPathToIcon  Path to the image (.png)
         *
         * @return Builder.
         */
        WindowBuilder& withIcon(std::string_view sPathToIcon);
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
         * Set IGameInstance derived class to react to
         * user inputs, window events and etc.
         * Will return control after the window was closed.
         */
        template <typename GameInstance>
        requires std::derived_from<GameInstance, IGameInstance>
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
         * @param sPathToIcon Path to the image (.png).
         * The image data should be 32-bit, little-endian, non-premultiplied RGBA,
         * i.e. eight bits per channel with the red channel first.
         *
         * @warning This function must only be called from the main thread.
         *
         * @return Returns error if file not found.
         */
        std::optional<Error> setIcon(std::string_view sPathToIcon) const;

        /**
         * Whether the cursor is visible or not (locked in this window).
         *
         * @param bIsVisible
         * @arg true Shows cursor (normal behavior).
         * @arg false This will hide the cursor and lock it to the window.
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
         * @warning This function must only be called from the main thread.
         *
         * @return A pair of width and height in pixels.
         */
        std::pair<int, int> getSize() const;

        /**
         * Returns the current cursor position on window.
         *
         * @warning This function must only be called from the main thread.
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
        IRenderer* getRenderer() const;

#if defined(WIN32)
        /**
         * Returns native Windows handle to this window.
         *
         * @return Window handle or NULL if an error occurred.
         */
        HWND getWindowHandle() const;
#endif

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
         * Called when the window receives keyboard input.
         *
         * @param key            Keyboard key.
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the key down event occurred or key up.
         */
        void onKeyboardInput(KeyboardKey key, KeyboardModifiers modifiers, bool bIsPressedDown) const;

        /**
         * Called when the window receives mouse input.
         *
         * @param button         Mouse button.
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the button down event occurred or button up.
         */
        void onMouseInput(MouseButton button, KeyboardModifiers modifiers, bool bIsPressedDown) const;

        /**
         * Called when the window receives mouse movement.
         *
         * @param iXPos    Mouse X position in pixels.
         * @param iYPos    Mouse Y position in pixels.
         */
        void onMouseMove(int iXPos, int iYPos);

        /**
         * Called when the window receives mouse scroll movement.
         *
         * @param iOffset Movement offset.
         */
        void onMouseScrollMove(int iOffset) const;

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

        /**
         * Holds main game objects.
         */
        std::unique_ptr<Game> pGame;

        /**
         * GLFW window.
         */
        GLFWwindow* pGlfwWindow;

        /**
         * Title of the window.
         */
        std::string sWindowTitle;

        /** Last mouse X position, used for calculating delta movement. */
        int iLastMouseXPos = 0;
        /** Last mouse Y position, used for calculating delta movement. */
        int iLastMouseYPos = 0;

        /** Name of the category used for logging. */
        inline static const char* sWindowLogCategory = "Window";
    };

    template <typename GameInstance>
    requires std::derived_from<GameInstance, IGameInstance>
    void Window::processEvents() {
        pGame = std::unique_ptr<Game>(new Game(this));

        // ... initialize other Game fields here ...

        // Finally create Game Instance when engine (Game) is fully initialized.
        // So that the user can call engine functions in Game Instance constructor.
        pGame->setGameInstance<GameInstance>();

        // Used for tick.
        float fCurrentTimeInSec = 0.0f;
        float fPrevTimeInSec = static_cast<float>(glfwGetTime());

        while (!glfwWindowShouldClose(pGlfwWindow)) {
            glfwPollEvents();

            // Tick.
            fCurrentTimeInSec = static_cast<float>(glfwGetTime());
            pGame->onBeforeNewFrame(fCurrentTimeInSec - fPrevTimeInSec);
            fPrevTimeInSec = fCurrentTimeInSec;

            // TODO: update()
            // TODO: drawFrame()
            // TODO: put update() into drawFrame()?
        }

        pGame->onWindowClose();
    }
} // namespace ne