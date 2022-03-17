#pragma once

// STL.
#include <variant>
#include <vector>
#include <string>

// Custom.
#include "misc/Error.h"

namespace ne {
    /**
     * Contains width, height and refresh rate.
     */
    struct RenderMode {
        int iWidth;
        int iHeight;
        int iRefreshRateNumerator;
        int iRefreshRateDenominator;
    };
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

        /**
         * Looks for video adapters (GPUs) that support this renderer.
         *
         * @return Error if can't find any GPU that supports this renderer,
         * vector with GPU names if successful.
         */
        virtual std::variant<std::vector<std::wstring>, Error> getSupportedVideoAdapters() const = 0;

        /**
         * Returns a list of supported render resolution.
         *
         * @return Error if something went wrong, otherwise render mode.
         */
        virtual std::variant<std::vector<RenderMode>, Error> getSupportedRenderResolutions() const = 0;

        /**
         * Returns backbuffer resolution.
         *
         * @return A pair of X and Y pixels.
         */
        virtual std::pair<int, int> getRenderResolution() const = 0;
    };
} // namespace ne
