#pragma once

// Standard.
#include <array>
#include <memory>

static constexpr unsigned int frameResourcesCount = 3;

namespace ne {
    class GpuResource;

    /**
     * Controls GPU resources (mostly constant buffers) that contain frame-specific data.
     *
     * Used to not wait for the GPU to finish drawing a frame on the CPU side
     * and continue drawing another frame (on the CPU side) without touching resources
     * that can be in use by the GPU because its drawing a previous frame.
     */
    class FrameResourcesManager {
    public:
        /**
         * Returns the number of used frame resources.
         *
         * @return Number of frame resources being used.
         */
        static inline unsigned int getFrameResourcesCount() { return frameResourcesCount; }

    private:
        /** Array of frame-specific resources, all contain the same data. */
        std::array<std::unique_ptr<GpuResource>, frameResourcesCount> vFrameResources;
    };
} // namespace ne
