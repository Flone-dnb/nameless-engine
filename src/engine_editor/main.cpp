// #if defined(DEBUG) || defined(_DEBUG)
//     #define _CRTDBG_MAP_ALLOC
//     #include <crtdbg.h>
//     #define new new(_CLIENT_BLOCK,__FILE__,__LINE__)
// #endif

#ifndef DEBUG
#include <Windows.h>
#endif

// Std.
#include <stdexcept>

// Custom.
#include "EditorGameInstance.h"
#include "Window.h"

int main() {
    // Enable run-time memory check for debug builds.
#if defined(DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#else
    OutputDebugStringA("Using release build configuration.");
#endif

    using namespace ne;

    std::variant<std::unique_ptr<Window>, Error> result = Window::getBuilder().withTitle("Editor").build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        error.showError();
        throw std::runtime_error(error.getError());
    }

    std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));

    pMainWindow->processEvents<EditorGameInstance>();

    return 0;
}