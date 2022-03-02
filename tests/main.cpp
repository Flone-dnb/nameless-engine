#define CATCH_CONFIG_RUNNER
#include "Catch2/catch.hpp"

#ifndef DEBUG
#include <Windows.h>
#endif

int main() {
    // Enable run-time memory check for debug builds.
#if defined(DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#else
    OutputDebugStringA("Using release build configuration.");
#endif
    Catch::Session().run();
}