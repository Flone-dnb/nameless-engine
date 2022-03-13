#define CATCH_CONFIG_RUNNER
#include "Catch2/catch.hpp"

#if defined(DEBUG) && defined(WIN32)
#include <Windows.h>
#endif

int main() {
// Enable run-time memory check for debug builds.
#if defined(DEBUG) && defined(WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#elif defined(WIN32) && !defined(DEBUG)
    OutputDebugStringA("Using release build configuration, memory checks are disabled.");
#endif

    Catch::Session().run();
}