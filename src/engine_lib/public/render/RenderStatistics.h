#pragma once

// Standard.
#include <chrono>
#include <optional>
#include <atomic>
#include <mutex>

namespace ne {
    /** Stores various statistics about rendering (FPS for example). */
    class RenderStatistics {
        // Renderer will update statistics.
        friend class Renderer;

    public:
        RenderStatistics() = default;

        RenderStatistics(const RenderStatistics&) = delete;
        RenderStatistics& operator=(const RenderStatistics&) = delete;

        /**
         * Returns the total number of frames that the renderer produced in the last second.
         *
         * @return Zero if not calculated yet (wait at least 1 second), otherwise FPS count.
         */
        size_t getFramesPerSecond() const;

        /**
         * Returns the total number of draw calls made last frame.
         *
         * @return Draw call count.
         */
        size_t getLastFrameDrawCallCount() const;

        /**
         * Returns the total number of meshes that were discarded from submitting to the
         * rendering during the last frame.
         *
         * @return Mesh count.
         */
        size_t getLastFrameCulledMeshCount() const;

        /**
         * Returns the total number of lights that were discarded from submitting to the
         * rendering during the last frame.
         *
         * @return Mesh count.
         */
        size_t getLastFrameCulledLightCount() const;

        /**
         * Returns time in milliseconds that was spent last frame waiting for GPU to catch up to the CPU.
         *
         * @remark If returned value is constantly bigger than zero then this might mean that you are GPU
         * bound, if constantly zero (0.0F) then this might mean that you are CPU bound.
         *
         * @return Time in milliseconds.
         */
        float getTimeSpentLastFrameWaitingForGpu() const;

        /**
         * Returns time in milliseconds that was spent last frame doing frustum culling for meshes.
         *
         * @return Time in milliseconds.
         */
        float getTimeSpentLastFrameOnFrustumCullingMeshes() const;

        /**
         * Returns time in milliseconds that was spent last frame doing frustum culling for lights.
         *
         * @return Time in milliseconds.
         */
        float getTimeSpentLastFrameOnFrustumCullingLights() const;

    private:
        /** Groups info related to measuring frame count per second. */
        struct FramesPerSecondInfo {
            /**
             * Time when the renderer has finished initializing or when
             * @ref iFramesPerSecond was updated.
             */
            std::chrono::steady_clock::time_point timeAtLastFpsUpdate;

            /**
             * The number of times the renderer presented a new image since the last time we updated
             * @ref iFramesPerSecond.
             */
            size_t iPresentCountSinceFpsUpdate = 0;

            /** The number of frames that the renderer produced in the last second. */
            size_t iFramesPerSecond = 0;
        };

        /** Groups info related to FPS limiting. */
        struct FpsLimitInfo {
            /** Time when last frame was started to be processed. */
            std::chrono::steady_clock::time_point frameStartTime;

            /** Not empty if FPS limit is set, defines time in nanoseconds that one frame should take. */
            std::optional<double> optionalTargetTimeToRenderFrameInNs;
        };

        /** Groups info related to measuring time for specific tasks. */
        struct TaskTimeInfo {
            /**
             * Time (in milliseconds) that was spent last frame waiting for the GPU to finish using
             * the new frame resource.
             *
             * @remark If constantly bigger than zero then this might mean that you are GPU bound,
             * if constantly zero then this might mean that you are CPU bound.
             */
            float timeSpentLastFrameWaitingForGpuInMs = 0.0F;

            /**
             * Total time that was spent last frame doing frustum culling for meshes.
             *
             * @remark Updated only after a frame is submitted.
             */
            float timeSpentLastFrameOnFrustumCullingMeshesInMs = 0.0F;

            /**
             * Total time that was spent last frame doing frustum culling for lights.
             *
             * @remark Updated only after a frame is submitted.
             */
            float timeSpentLastFrameOnFrustumCullingLightsInMs = 0.0F;
        };

        /** Groups counters for various tasks. */
        struct Counters {
            /**
             * Total number of meshes discarded from submitting to the rendering due to frustum culling.
             *
             * @remark Updated only after a frame is submitted.
             */
            size_t iLastFrameCulledMeshesCount = 0;

            /**
             * Total number of lights discarded from submitting to the rendering due to frustum culling.
             *
             * @remark Updated only after a frame is submitted.
             */
            size_t iLastFrameCulledLightsCount = 0;

            /** The total number of draw calls made during the last frame. */
            size_t iLastFrameDrawCallCount = 0;
        };

        /**
         * Groups variables to continuously modify in the process of submitting a new frame.
         * Variables here will be reset and saved to other struct as resulting values after a frame was
         * submitted.
         */
        struct FrameTemporaryStatistics {
            FrameTemporaryStatistics();

            /**
             * Time in milliseconds spent last frame on frustum culling meshes.
             *
             * @remark Use mutex to update/read the value.
             */
            std::pair<std::mutex, float> mtxFrustumCullingMeshesTimeInMs;

            /**
             * Time in milliseconds spent last frame on frustum culling lights.
             *
             * @remark Use mutex to update/read the value.
             */
            std::pair<std::mutex, float> mtxFrustumCullingLightsTimeInMs;

            /** Total number of meshes discarded from submitting due to frustum culling. */
            std::atomic<size_t> iCulledMeshCount{0};

            /** Total number of lights discarded from submitting due to frustum culling. */
            std::atomic<size_t> iCulledLightCount{0};

            /** Stores the total number of draw calls made last frame. */
            std::atomic<size_t> iDrawCallCount{0};
        };

        /**
         * Saves all temporary frame statistics as resulting values in non-temporary structs
         * and resets all temporary statistics (variables in this struct).
         */
        void saveAndResetTemporaryFrameStatistics();

        /** Info related to measuring frame count per second. */
        FramesPerSecondInfo fpsInfo;

        /** Info related to FPS limiting. */
        FpsLimitInfo fpsLimitInfo;

        /** Info related to measuring time for specific tasks. */
        TaskTimeInfo taskTimeInfo;

        /** Counters for various tasks. */
        Counters counters;

        /** Temporary counters for a single frame. */
        FrameTemporaryStatistics frameTemporaryStatistics;
    };
}
