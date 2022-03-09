#pragma once

// STL.
#include <variant>
#include <memory>

// Custom.
#include "misc/Error.h"
#include "game/Game.h"
#include "window/GLFW.hpp"
#include "IGameInstance.h"
#include "input/KeyboardKey.hpp"

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
        std::string sWindowTitle = "";
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
        WindowBuilder &withSize(int iWidth, int iHeight);
        /**
         * Defines the name of a window that we will create.
         *
         * @param sWindowTitle Title of the window.
         *
         * @return Builder.
         */
        WindowBuilder &withTitle(const std::string &sWindowTitle);
        /**
         * Defines the visibility of a window that we will create.
         * Does nothing for fullscreen windows.
         *
         * @param bShow Visibility of the window.
         *
         * @return Builder.
         */
        WindowBuilder &withVisibility(bool bShow);
        /**
         * Whether the window should be maximized after creation or not.
         * Does nothing for fullscreen windows.
         *
         * @param bMaximized Should window be maximized or not.
         *
         * @return Builder.
         */
        WindowBuilder &withMaximizedState(bool bMaximized);
        /**
         * Whether the window should look like a splash screen or not
         * (no border, title, buttons, etc).
         * Does nothing for fullscreen windows.
         *
         * @param bIsSplashScreen Should window look like a splash screen or not.
         *
         * @return Builder.
         */
        WindowBuilder &withSplashScreenMode(bool bIsSplashScreen);
        /**
         * Whether a window should be shown in the fullscreen mode or not.
         *
         * @param bEnableFullscreen Fullscreen mode.
         *
         * @return Builder.
         */
        WindowBuilder &withFullscreenMode(bool bEnableFullscreen);
        /**
         * Builds/creates a new window with the configured parameters.
         *
         * @return Returns error if something went wrong or created window otherwise.
         *
         * @warning This function should only be called from the main thread.
         */
        std::variant<std::unique_ptr<Window>, Error> build() const;

    private:
        /** Configured window parameters. */
        WindowBuilderParameters params;
    };

    /**
     * Describes a window.
     */
    class Window {
    public:
        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;

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
        void processEvents() {
            pGame = std::unique_ptr<Game>(new Game());

            // initialize other Game fields here

            // Finally create Game Instance when engine (Game) is fully initialized.
            // So that the user can call engine functions in Game Instance constructor.
            pGame->setGameInstance<GameInstance>(this);

            // Used for tick.
            float fCurrentTimeInSec = 0.0f;
            float fPrevTimeInSec = 0.0f;

            while (!glfwWindowShouldClose(pGLFWWindow)) {
                glfwPollEvents();

                // Tick.
                fCurrentTimeInSec = static_cast<float>(glfwGetTime());
                pGame->pGameInstance->onBeforeNewFrame(fCurrentTimeInSec - fPrevTimeInSec);
                fPrevTimeInSec = fCurrentTimeInSec;

                // TODO: update()
                // TODO: drawFrame()
            }

            pGame->pGameInstance->onWindowClose();
        }

        /**
         * Sets the window opacity (1.0f for opaque, 0.0f for transparent).
         * Does nothing if the OS does not support transparent windows.
         */
        void setOpacity(float fOpacity) const;

        /**
         * Sets new window title.
         */
        void setTitle(const std::string &sNewTitle);

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
         * Used internally, should not be called from user code.
         * Use IGameInstance::onKeyInput() from user code.
         *
         * @param key        Keyboard key.
         * @param modifiers  Keyboard modifier keys.
         * @param action     Type of keyboard action.
         */
        void internalOnKeyInput(KeyboardKey key, KeyboardModifiers modifiers, KeyboardAction action) const;

        /**
         * Used internally, should not be called from user code.
         * Use IGameInstance::onWindowFocusChanged() from user code.
         *
         * @param bIsFocused  Whether the window has gained or lost the focus.
         */
        void internalOnWindowFocusChanged(bool bIsFocused) const;

    private:
        friend class WindowBuilder;

        /**
         * Creates a new window.
         *
         * @param params Parameters that define initial window state.
         *
         * @return Returns error if something went wrong or created window otherwise.
         */
        static std::variant<std::unique_ptr<Window>, Error>
        newInstance(const WindowBuilderParameters &params);

        /**
         * Default constructor.
         *
         * @param pGLFWWindow   Created GLFW window.
         * @param sWindowTitle  Title of this window.
         */
        Window(GLFWwindow *pGLFWWindow, const std::string &sWindowTitle);

        /**
         * Holds main game objects.
         */
        std::unique_ptr<Game> pGame;

        /**
         * GLFW window.
         */
        GLFWwindow *pGLFWWindow;

        /**
         * Title of the window.
         */
        std::string sWindowTitle;
    };
} // namespace ne