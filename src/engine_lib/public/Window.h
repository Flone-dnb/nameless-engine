#pragma once

// STL.
#include <string_view>
#include <variant>
#include <memory>

// Custom.
#include "Error.h"
#include "Game.h"

// OS.
#include <Windows.h>

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
        /** Whether to show window in fullscreen mode. */
        bool bFullscreen = false;
    };

    class Window;

    /**
     * Builder pattern class for Window.
     */
    class WindowBuilder {
    public:
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
         *
         * @param bShow Visibility of the window.
         *
         * @return Builder.
         */
        WindowBuilder &withVisibility(bool bShow);
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
        /**
         * Function to handle window messages.
         * Should not be called explicitly.
         *
         * @param hWnd   Window handle.
         * @param msg    Window message.
         * @param wParam wParam.
         * @param lParam lParam.
         *
         * @return LRESULT.
         */
        virtual LRESULT CALLBACK windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
            pGame->setGameInstance<GameInstance>();
            do {
                processNextWindowMessage();
            } while (!bDestroyReceived);
        }

        /**
         * Shows the window on screen.
         *
         * @param bMaximized  Whether to show window in the maximized state or
         * in the normal (usual) state.
         */
        void show(bool bMaximized = false) const;

        /**
         * Minimizes the window.
         * If the window was hidden, it will be shown.
         */
        void minimize() const;

        /**
         * Hides the windows (makes it invisible).
         */
        void hide() const;

        /**
         * Returns the title of this window.
         *
         * @return Title of the window.
         */
        std::string getTitle() const;

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
         * @param hWindow         Handle of the created window.
         * @param sWindowTitle    Title of the created window class.
         * @param sWindowClass    Class of the created window class.
         * @param iWindowWidth    Width of the window.
         * @param iWindowHeight   Height of the window.
         * @param bFullscreen     Whether the window should be shown in the fullscreen mode or not.
         */
        Window(HWND hWindow, const std::string &sWindowTitle, const std::string &sWindowClass,
               int iWindowWidth, int iWindowHeight, bool bFullscreen);

        /**
         * Handles next message to this window.
         *
         * If 'WM_NCDESTROY' message was previously received
         * (window and all of its child windows have been destroyed (closed)), does nothing.
         */
        void processNextWindowMessage() const;

        /**
         * Holds main game objects.
         */
        std::unique_ptr<Game> pGame;

        /** Handle to the window. */
        HWND hWindow;

        /** Title of the window. */
        std::string sWindowTitle;

        /** Class of the window. */
        std::string sWindowClass;

        /** Width of the window. */
        int iWindowWidth;
        /** Height of the window. */
        int iWindowHeight;

        /** Whether the window should be shown in the fullscreen mode or not. */
        bool bFullscreen = false;

        /** Will be 'true' when this window receives 'WM_NCDESTROY' window message. */
        bool bDestroyReceived = false;
    };
} // namespace ne