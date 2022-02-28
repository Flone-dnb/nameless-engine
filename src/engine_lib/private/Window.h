#pragma once

// STL.
#include <string_view>
#include <variant>
#include <memory>

// OS.
#include <Windows.h>

namespace dxe {
    class Error;

    class Window {
    public:
        /**
         * Creates a new window.
         *
         * @param iWindowWidth   Width of a window.
         * @param iWindowHeight  Height of a window.
         * @param sWindowName    Optional parameter that specifies a window name,
         * if empty the name will be automatically generated.
         * @param bHideTitleBar  Whether to hide or show window title bar.
         * @param bShowWindow    Whether to show window after it was created or not.
         *
         * @return Returns error if something went wrong or created window otherwise.
         */
        static std::variant<std::unique_ptr<Window>, Error>
        newInstance(int iWindowWidth = 800, int iWindowHeight = 600,
                    const std::string &sWindowName = std::string(), bool bHideTitleBar = false,
                    bool bShowWindow = false);

        /**
         * Function to handle window messages.
         * Should not be called explicitly.
         */
        virtual LRESULT CALLBACK windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;
        virtual ~Window();

        /**
         * Shows the window on screen.
         *
         * @param bMaximized  Whether to show window in the maximized state or not.
         */
        void show(bool bMaximized = false) const;

        /**
         * Hides the windows (makes it invisible).
         */
        void hide() const;

        /**
         * Returns a name of this window.
         *
         * @return Name of the window.
         */
        std::string getName() const;

    private:
        friend class Application;

        Window() = default;

        /**
         * Handles next message to this window.
         *
         * @return 'true' if 'WM_NCDESTROY' message was received, this message indicates that
         * the window and all of its child windows have been destroyed (closed), returns 'false'
         * otherwise.
         */
        bool processNextWindowMessage() const;

        /** Handle to the window. */
        HWND hWindow;

        /** Name of the window. */
        std::string sWindowName;

        /** Width of the window. */
        int iWindowWidth;
        /** Height of the window. */
        int iWindowHeight;

        /** Will be 'true' when this window receives 'WM_NCDESTROY' window message. */
        bool bDestroyReceived = false;

        /** Whether the window was successfully created or not. */
        bool bIsInitialized = false;
    };
} // namespace dxe