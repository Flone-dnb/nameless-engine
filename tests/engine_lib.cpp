#if defined(WIN32)
#include <Windows.h>
#include <crtdbg.h>
#undef MessageBox
#undef IGNORE
#endif

// Standard.
#include <filesystem>

// Custom.
#include "misc/Globals.h"
#include "misc/ProjectPaths.h"

// External.
#include "catch2/catch_session.hpp"

int main() {
// Enable run-time memory check for debug builds.
#if defined(DEBUG) && defined(WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#elif defined(WIN32) && !defined(DEBUG)
    OutputDebugStringA("Using release build configuration, memory checks are disabled.");
#endif

    // Clear any config files.
    const std::filesystem::path baseConfigPath =
        ne::ProjectPaths::getPathToBaseConfigDirectory() / ne::Globals::getApplicationName();
    const auto baseTempPath =
        ne::ProjectPaths::getPathToResDirectory(ne::ResourceDirectory::ROOT) / "test" / "temp";

    if (std::filesystem::exists(baseConfigPath)) {
        std::filesystem::remove_all(baseConfigPath);
    }
    if (std::filesystem::exists(baseTempPath)) {
        std::filesystem::remove_all(baseTempPath);
    }

    Catch::Session().run();
}
