// #if defined(DEBUG) || defined(_DEBUG)
//     #define _CRTDBG_MAP_ALLOC
//     #include <crtdbg.h>
//     #define new new(_CLIENT_BLOCK,__FILE__,__LINE__)
// #endif

#ifndef DEBUG
#include <Windows.h>
#endif

#include "Application.h"

int main() {
    // Enable run-time memory check for debug builds.
#if defined(DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#else
    OutputDebugStringA("Using release build configuration.");
#endif

    dxe::Application &app = dxe::Application::get();
    app.run();

    return 0;
}