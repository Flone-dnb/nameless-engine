#include "Game.h"

// Custom.
#include "render/DirectXRenderer.h"
#include "IGameInstance.h"

namespace ne {
    Game::Game() {
        // Currently, we have only 1 renderer, when we will have
        // more renderers we will automatically pick one here.
        pRenderer = std::make_unique<DirectXRenderer>();
    }

} // namespace ne
