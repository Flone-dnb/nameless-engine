#include "shaders/ShaderManager.h"

// Custom.
#include "shaders/Shader.h"

namespace ne {
    std::optional<Error> ShaderManager::compileShaders(
        const std::vector<ShaderDescription> &vShadersToCompile,
        std::function<void(size_t, size_t)> onProgress,
        std::function<void()> onCompleted) {
        // TODO: check bIsShuttingDown with std::memory_order_seq

        std::scoped_lock<std::mutex> guard(mtxRwShaders);

        // TODO: for each shader in vShadersToCompile: check that we don't have shader with this name
        // TODO: cycle through std::vector<std::promise<bool>> and try_get() remove finished.

        if (vShadersToCompile.empty()) {
            return Error("the specified array of shaders to compile is empty");
        }

        // TODO: do all work in another thread
        // TODO: use mutex to sync access to shaders map.

        // TODO: add std::atomic_flag bIsShuttingDown;
        // TODO: add std::vector<std::promise<bool>> vRunningCompilationThreads;

        // TODO: each compilation thread will have its own promise pointer
        // TODO: in ~ShaderManager():
        // TODO: - switch bIsShuttingDown with std::memory_order_seq
        // TODO: - lock mutex
        // TODO: - wait for each thread to finish EARLY!

        // TODO: on compilation thread:
        // TODO: - use mtxRwShaders to add new shaders
        // TODO: - call onProgress (+ write to log with shader name)
        // TODO: - call onCompleted and onProgress (+ write to log with shader name)
        // TODO: - set promise on exit
    }
} // namespace ne
