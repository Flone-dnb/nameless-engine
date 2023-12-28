#include "render/RenderStatistics.h"

namespace ne {

    RenderStatistics::FrameTemporaryStatistics::FrameTemporaryStatistics() {
        // Initialize some variables.
        mtxFrustumCullingTimeInMs.second = 0.0F;
    }

    void RenderStatistics::FrameTemporaryStatistics::reset() {
        {
            std::scoped_lock guard(mtxFrustumCullingTimeInMs.first);
            mtxFrustumCullingTimeInMs.second = 0.0F;
        }

        iCulledObjectCount.store(0);
        iDrawCallCount.store(0);

#if defined(DEBUG)
        static_assert(sizeof(FrameTemporaryStatistics) == 104, "reset new statistics here"); // NOLINT
#endif
    }

    size_t RenderStatistics::getFramesPerSecond() const { return fpsInfo.iFramesPerSecond; }

    size_t RenderStatistics::getLastFrameDrawCallCount() const { return counters.iLastFrameDrawCallCount; }

    size_t RenderStatistics::getLastFrameCulledObjectCount() const {
        return counters.iLastFrameCulledObjectCount;
    }

    float RenderStatistics::getTimeSpentLastFrameWaitingForGpu() const {
        return taskTimeInfo.timeSpentLastFrameWaitingForGpuInMs;
    }

    float RenderStatistics::getTimeSpentLastFrameOnFrustumCulling() const {
        return taskTimeInfo.timeSpentLastFrameOnFrustumCullingInMs;
    }

}
