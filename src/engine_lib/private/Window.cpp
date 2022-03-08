#include "window.h"

// Custom.
#include "UniqueValueGenerator.h"
#include "IGameInstance.h"
#include "Game.h"

namespace ne {
    LRESULT CALLBACK GlobalWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        Window *pWindow = nullptr;

        // Get "this" pointer.
        if (msg == WM_NCCREATE) {
            // Recover the "this" pointer which was passed as a parameter to CreateWindow(Ex).
            const LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
            pWindow = static_cast<Window *>(lpcs->lpCreateParams);

            // Put the value in a safe place for future use.
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));
        } else {
            // Recover the "this" pointer from where our WM_NCCREATE handler stashed it.
            pWindow = reinterpret_cast<Window *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        }

        if (pWindow == nullptr) {
            return DefWindowProc(hWnd, msg, wParam, lParam);
        }
        return pWindow->windowProc(hWnd, msg, wParam, lParam);
    }

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

    WindowBuilder &WindowBuilder::withFullscreenMode(bool bEnableFullscreen) {
        params.bFullscreen = bEnableFullscreen;

        return *this;
    }

    std::variant<std::unique_ptr<Window>, Error> WindowBuilder::build() const {
        return Window::newInstance(params);
    }

    std::variant<std::unique_ptr<Window>, Error> Window::newInstance(const WindowBuilderParameters &params) {
        std::string sNewWindowTitle = params.sWindowTitle;
        const std::string sNewWindowClass = UniqueValueGenerator::get().getUniqueWindowClassName();

        // Check window name.
        if (sNewWindowTitle.empty()) {
            sNewWindowTitle = sNewWindowClass;
        }

        // Register window class.
        WNDCLASSEX wc = {0};
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = GlobalWindowProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        wc.lpszMenuName = nullptr;
        wc.lpszClassName = sNewWindowClass.c_str();
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
        wc.cbSize = sizeof(wc);

        if (!RegisterClassExA(&wc)) {
            return Error(GetLastError());
        }

        // Create window.
        RECT rect = {0, 0, params.iWindowWidth, params.iWindowHeight};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
        const int iWidth = rect.right - rect.left;
        const int iHeight = rect.bottom - rect.top;

        const HWND hNewWindow =
            CreateWindow(sNewWindowClass.data(), sNewWindowTitle.data(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                         CW_USEDEFAULT, iWidth, iHeight, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        if (hNewWindow == nullptr) {
            return Error(GetLastError());
        }

        if (params.bFullscreen) {
            SetWindowLong(hNewWindow, GWL_STYLE, WS_POPUP);
        }

        // Setup raw input.
        RAWINPUTDEVICE
        rid[2];

        // ... for mouse.
        rid[0].usUsagePage = 0x01;
        rid[0].usUsage = 0x02;
        rid[0].dwFlags = 0;
        rid[0].hwndTarget = hNewWindow;

        // ... for keyboard.
        // rid[1].usUsagePage = 0x01;
        // rid[1].usUsage = 0x06;
        // rid[1].dwFlags = 0;
        // rid[1].hwndTarget = pWindow->hWindow;

        if (RegisterRawInputDevices(rid, 1, sizeof(rid[0])) == FALSE)
        // if (RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE)
        {
            UnregisterClassA(sNewWindowClass.c_str(), GetModuleHandle(nullptr));
            return Error(GetLastError());
        }

        // Initialize instance now when we won't fail anymore.
        std::unique_ptr<Window> pWindow = std::unique_ptr<Window>(
            new Window(hNewWindow, sNewWindowTitle, sNewWindowClass, params.iWindowWidth,
                       params.iWindowHeight, params.bFullscreen));

        SetWindowLongPtr(hNewWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow.get()));

        // Show window if needed.
        if (params.bShowWindow) {
            pWindow->show();
        }

        return pWindow;
    }

    Window::~Window() { UnregisterClassA(sWindowClass.c_str(), GetModuleHandle(nullptr)); }

    WindowBuilder Window::getBuilder() { return WindowBuilder(); }

    void Window::show(bool bMaximized) const {
        if (bFullscreen) {
            bMaximized = true;
        }
        ShowWindow(hWindow, bMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
        UpdateWindow(hWindow);
    }

    void Window::minimize() const {
        if (!IsWindowVisible(hWindow)) {
            show();
        }
        ShowWindow(hWindow, SW_SHOWMINIMIZED);
        UpdateWindow(hWindow);
    }

    void Window::hide() const {
        ShowWindow(hWindow, SW_HIDE);
        UpdateWindow(hWindow);
    }

    std::string Window::getTitle() const { return sWindowTitle; }

    Window::Window(HWND hWindow, const std::string &sWindowTitle, const std::string &sWindowClass,
                   int iWindowWidth, int iWindowHeight, bool bFullscreen) {
        this->hWindow = hWindow;
        this->sWindowTitle = sWindowTitle;
        this->sWindowClass = sWindowClass;
        this->iWindowWidth = iWindowWidth;
        this->iWindowHeight = iWindowHeight;
        this->bFullscreen = bFullscreen;
    }

    LRESULT Window::windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_NCDESTROY: {
            bDestroyReceived = true;
            break;
        }
        }

        // Sometimes needs to be called for cleanup.
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    void Window::processNextWindowMessage() const {
        if (bDestroyReceived) {
            return;
        }

        MSG msg = {};

        // Don't use GetMessage() as it puts the thread to sleep until new message.
        if (PeekMessage(&msg, hWindow, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // TODO: add delta time here
            pGame->pGameInstance->onBeforeNewFrame(-1.0f);
            // TODO: update()
            // TODO: draw()
        }
    }
} // namespace ne