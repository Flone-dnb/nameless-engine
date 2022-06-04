#if defined(DEBUG) && defined(WIN32)
#include <Windows.h>
#endif

// Std.
#include <stdexcept>

// Custom.
#include "EditorGameInstance.h"
#include "game/Window.h"

int main() {
// Enable run-time memory check for debug builds.
#if defined(DEBUG) && defined(WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#elif defined(WIN32) && !defined(DEBUG)
    OutputDebugStringA("Using release build configuration, memory checks are disabled.");
#endif

    using namespace ne;

    auto result = Window::getBuilder()
                      .withTitle("Nameless Editor")
                      .withMaximizedState(true)
                      .withIcon("res/editor/nameless_editor_icon.png")
                      .build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        error.showError();
        throw std::runtime_error(error.getError());
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));

    pMainWindow->processEvents<EditorGameInstance>();

    return 0;
}
