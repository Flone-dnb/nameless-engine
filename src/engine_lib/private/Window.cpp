#include "window.h"

// STL.
#include <stdexcept>

// Custom.
#include "Application.h"
#include "UniqueValueGenerator.h"
#include "Error.h"

namespace dxe {
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

    std::variant<std::unique_ptr<Window>, Error> Window::newInstance(int iWindowWidth, int iWindowHeight,
                                                                     const std::string &sWindowName,
                                                                     bool bHideTitleBar, bool bShowWindow) {
        std::unique_ptr<Window> pWindow = std::unique_ptr<Window>(new Window());

        // Check window name.
        if (sWindowName.empty()) {
            pWindow->sWindowName = UniqueValueGenerator::get().getUniqueWindowName();
        } else {
            // Check if this window name is not used.
            if (Application::get().getWindowByName(sWindowName) != nullptr) {
                // A window with this name already exists.
                return Error("a window with this name already exists");
            }
            pWindow->sWindowName = sWindowName;
        }

        // Save window size.
        pWindow->iWindowWidth = iWindowWidth;
        pWindow->iWindowHeight = iWindowHeight;

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
        wc.lpszClassName = pWindow->sWindowName.c_str();
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
        wc.cbSize = sizeof(wc);

        if (!RegisterClassExA(&wc)) {
            return Error(GetLastError());
        }

        // Create window.
        RECT rect = {0, 0, iWindowWidth, iWindowHeight};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
        const int iWidth = rect.right - rect.left;
        const int iHeight = rect.bottom - rect.top;

        pWindow->hWindow =
            CreateWindow(pWindow->sWindowName.data(), pWindow->sWindowName.data(),
                         bHideTitleBar ? WS_POPUP : WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, iWidth,
                         iHeight, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        if (pWindow->hWindow == nullptr) {
            return Error(GetLastError());
        }

        SetWindowLongPtr(pWindow->hWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow.get()));

        SetWindowText(pWindow->hWindow, pWindow->sWindowName.data());

        // Setup raw input.
        RAWINPUTDEVICE rid[2];

        // ... for mouse.
        rid[0].usUsagePage = 0x01;
        rid[0].usUsage = 0x02;
        rid[0].dwFlags = 0;
        rid[0].hwndTarget = pWindow->hWindow;

        // ... for keyboard.
        // rid[1].usUsagePage = 0x01;
        // rid[1].usUsage = 0x06;
        // rid[1].dwFlags = 0;
        // rid[1].hwndTarget = pWindow->hWindow;

        if (RegisterRawInputDevices(rid, 1, sizeof(rid[0])) == FALSE)
        // if (RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE)
        {
            return Error(GetLastError());
        }

        // Show window if needed.
        if (bShowWindow) {
            pWindow->show();
        }

        return pWindow;
    }

    Window::~Window() { UnregisterClassA(sWindowName.c_str(), GetModuleHandle(nullptr)); }

    void Window::show(const bool bMaximized) const {
        ShowWindow(hWindow, bMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
        UpdateWindow(hWindow);
    }

    void Window::hide() const {
        ShowWindow(hWindow, SW_HIDE);
        UpdateWindow(hWindow);
    }

    std::string Window::getName() const { return sWindowName; }

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

    bool Window::processNextWindowMessage() const {
        if (bDestroyReceived) {
            return true;
        }

        MSG msg = {};

        // Don't use GetMessage() as it puts the thread to sleep until new message.
        if (PeekMessage(&msg, hWindow, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        return false;
    }
} // namespace dxe