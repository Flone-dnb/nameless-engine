#if defined(WIN32)
#include <Windows.h>
#include <crtdbg.h>
#undef MessageBox
#undef IGNORE
#endif

// Std.
#include <stdexcept>

// Custom.
#include "EditorGameInstance.h"
#include "game/Window.h"

#include "game/nodes/Node.h"

int main() {
// Enable run-time memory check for debug builds.
#if defined(DEBUG) && defined(WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#elif defined(WIN32) && !defined(DEBUG)
    OutputDebugStringA("Using release build configuration, memory checks are disabled.");
#endif

    // Test code for serialization and deserialization.
    const std::filesystem::path pathToFile = "MyCoolNode.toml";

    // Serialize.
    ne::Node node("My Cool Node");
    auto optionalError = node.serialize(pathToFile, true);
    if (optionalError.has_value()) {
        auto err = optionalError.value();
        err.addEntry();
        err.showError();
        throw std::runtime_error(err.getError());
    }

    // Deserialize.
    auto deserializeResult = ne::Serializable::deserialize<ne::Node>(pathToFile);
    if (std::holds_alternative<ne::Error>(deserializeResult)) {
        ne::Error error = std::get<ne::Error>(std::move(deserializeResult));
        error.addEntry();
        error.showError();
        throw std::runtime_error(error.getError());
    }
    const auto pDeserializedObject = std::get<std::shared_ptr<ne::Node>>(std::move(deserializeResult));

    // End.

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
