#include "render/RenderStatistics.h"

namespace ne {

    RenderStatistics::FrameTemporaryStatistics::FrameTemporaryStatistics() {
        // Initialize some variables.
        mtxFrustumCullingMeshesTimeInMs.second = 0.0F;
    }

    size_t RenderStatistics::getFramesPerSecond() const { return fpsInfo.iFramesPerSecond; }

    size_t RenderStatistics::getLastFrameDrawCallCount() const { return counters.iLastFrameDrawCallCount; }

    size_t RenderStatistics::getLastFrameCulledMeshCount() const {
        return counters.iLastFrameCulledMeshesCount;
    }

    size_t RenderStatistics::getLastFrameCulledLightCount() const {
        return counters.iLastFrameCulledLightsCount;
    }

    float RenderStatistics::getTimeSpentLastFrameWaitingForGpu() const {
        return taskTimeInfo.timeSpentLastFrameWaitingForGpuInMs;
    }

    float RenderStatistics::getTimeSpentLastFrameOnFrustumCullingMeshes() const {
        return taskTimeInfo.timeSpentLastFrameOnFrustumCullingMeshesInMs;
    }

    float RenderStatistics::getTimeSpentLastFrameOnFrustumCullingLights() const {
        return taskTimeInfo.timeSpentLastFrameOnFrustumCullingLightsInMs;
    }

    void RenderStatistics::saveAndResetTemporaryFrameStatistics() {
        // Update frustum culling meshes time.
        {
            std::scoped_lock guard(frameTemporaryStatistics.mtxFrustumCullingMeshesTimeInMs.first);

            // Save.
            taskTimeInfo.timeSpentLastFrameOnFrustumCullingMeshesInMs =
                frameTemporaryStatistics.mtxFrustumCullingMeshesTimeInMs.second;

            // Reset.
            frameTemporaryStatistics.mtxFrustumCullingMeshesTimeInMs.second = 0.0F;
        }

        // Update frustum culling lights time.
        {
            std::scoped_lock guard(frameTemporaryStatistics.mtxFrustumCullingLightsTimeInMs.first);

            // Save.
            taskTimeInfo.timeSpentLastFrameOnFrustumCullingLightsInMs =
                frameTemporaryStatistics.mtxFrustumCullingLightsTimeInMs.second;

            // Reset.
            frameTemporaryStatistics.mtxFrustumCullingLightsTimeInMs.second = 0.0F;
        }

        // Save and reset culled mesh count.
        counters.iLastFrameCulledMeshesCount = frameTemporaryStatistics.iCulledMeshCount.load();
        frameTemporaryStatistics.iCulledMeshCount.store(0);

        // Save and reset culled light count.
        counters.iLastFrameCulledLightsCount = frameTemporaryStatistics.iCulledLightCount.load();
        frameTemporaryStatistics.iCulledLightCount.store(0);

        // Save and reset draw call count.
        counters.iLastFrameDrawCallCount = frameTemporaryStatistics.iDrawCallCount.load();
        frameTemporaryStatistics.iDrawCallCount.store(0);

#if defined(DEBUG)
        static_assert(
            sizeof(FrameTemporaryStatistics) ==
#if defined(WIN32)
                200, // NOLINT: current size
#else
                120, // NOLINT: current size
#endif
            "save and reset new statistics here");
#endif
    }

}
