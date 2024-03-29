#if defined(WIN32)
#include <Windows.h>
#include <crtdbg.h>
#undef MessageBox
#undef IGNORE
#endif

// Standard.
#include <stdexcept>

// Custom.
#include "EditorGameInstance.h"
#include "game/Window.h"
#include "misc/ProjectPaths.h"

int main() {
// Enable run-time memory check for debug builds.
#if defined(DEBUG) && defined(WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#elif defined(WIN32) && !defined(DEBUG)
    OutputDebugStringA("Using release build configuration, memory checks are disabled.");
#endif
    using namespace ne;

    auto result =
        Window::getBuilder()
            .withTitle(EditorGameInstance::getEditorWindowTitle())
            .withMaximizedState(true)
            .withIcon(
                ProjectPaths::getPathToResDirectory(ResourceDirectory::EDITOR) / "nameless_editor_icon.png")
            .build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));

    pMainWindow->processEvents<EditorGameInstance>();

    return 0;
}
