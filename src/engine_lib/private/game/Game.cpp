#include "Game.h"

// Custom.
#if defined(WIN32)
#include "render/DirectXRenderer.h"
#endif

namespace ne {
    Game::Game() {
#if defined(WIN32)
        pRenderer = std::make_unique<DirectXRenderer>();
#elif __linux__
        static_assert(false, "need to assign renderer here");
#endif
    }

} // namespace ne
