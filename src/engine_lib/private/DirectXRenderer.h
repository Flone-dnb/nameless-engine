#pragma once

// Custom.
#include "IRenderer.h"

namespace ne {
    class DirectXRenderer : public IRenderer {
    public:
        DirectXRenderer() = default;

        DirectXRenderer(const DirectXRenderer &) = delete;
        DirectXRenderer &operator=(const DirectXRenderer &) = delete;

        virtual ~DirectXRenderer() override {}

        virtual void update() override;
        virtual void drawFrame() override;

    private:
    };
} // namespace ne
