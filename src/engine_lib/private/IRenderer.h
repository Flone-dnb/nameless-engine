#pragma once

namespace ne {
    /**
     * Defines an interface for renderers to implement.
     */
    class IRenderer {
    public:
        IRenderer() = default;

        IRenderer(const IRenderer &) = delete;
        IRenderer &operator=(const IRenderer &) = delete;

        virtual ~IRenderer() = default;

        virtual void update() = 0;
        virtual void drawFrame() = 0;
    };
} // namespace ne
