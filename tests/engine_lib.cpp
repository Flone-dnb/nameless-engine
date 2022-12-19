#if defined(WIN32)
#include <Windows.h>
#include <crtdbg.h>
#undef MessageBox
#undef IGNORE
#endif

// Standard.
#include <filesystem>

// Custom.
#include "catch2/catch_session.hpp"
#include "misc/Globals.h"

int main() {
// Enable run-time memory check for debug builds.
#if defined(DEBUG) && defined(WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#elif defined(WIN32) && !defined(DEBUG)
    OutputDebugStringA("Using release build configuration, memory checks are disabled.");
#endif

    // Clear any config files.
    const std::filesystem::path basePath = ne::getBaseDirectoryForConfigs() / ne::getApplicationName();

    if (std::filesystem::exists(basePath)) {
        std::filesystem::remove_all(basePath);
    }

    Catch::Session().run();
}
