#include "Game.h"

// Custom.
#include "DirectXRenderer.h"
#include "IGameInstance.h"

dxe::Game::Game() {
    // Currently, we have only 1 renderer, when we will have
    // more renderers we will automatically pick one here.
    pRenderer = std::make_unique<DirectXRenderer>();
}
