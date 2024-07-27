#pragma once

// Standard.
#include <memory>
#include <optional>

// Custom.
#include "misc/Error.h"
#include "render/general/resources/UploadBuffer.h"

namespace ne {
    class Renderer;

    /** Stores objects used by one frame. */
    struct FrameResource {
        // Only frame resource manager can initialize us.
        friend class FrameResourceManager;

        virtual ~FrameResource() = default;

        /** Stores frame-global constants, such as camera position, time, various matrices, etc. */
        std::unique_ptr<UploadBuffer> pFrameConstantBuffer;

    private:
        /**
         * Called by frame resource manager after a frame resource was constructed to initialize its fields.
         *
         * @param pRenderer Used renderer.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> initialize(Renderer* pRenderer) = 0;
    };
} // namespace ne
