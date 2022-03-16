﻿#include "Game.h"

// Custom.
#include "render/IRenderer.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif

namespace ne {
    Game::Game(Window *pWindow) {
        this->pWindow = pWindow;

#if defined(WIN32)
        pRenderer = std::make_unique<DirectXRenderer>();
#elif __linux__
        static_assert(false, "need to assign renderer here");
#endif
    }

} // namespace ne
