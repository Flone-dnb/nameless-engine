#include "GlobalShaderResourceBindingManager.h"

// Custom.
#include "render/general/pipeline/PipelineManager.h"
#include "shader/general/resources/GlobalShaderResourceBinding.h"
#include "misc/Profiler.hpp"

namespace ne {

    GlobalShaderResourceBindingManager::GlobalShaderResourceBindingManager(PipelineManager* pPipelineManager)
        : pPipelineManager(pPipelineManager) {}

    std::optional<Error>
    GlobalShaderResourceBindingManager::createGlobalShaderResourceBindingResourcePerFrame(
        const std::string& sShaderResourceName,
        std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()> vResourcesToBind) {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxActiveBindings.first);

        // Create new binding.
        auto optionalError = GlobalShaderResourceBinding::create(
            pPipelineManager->getRenderer(), this, sShaderResourceName, vResourcesToBind);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> GlobalShaderResourceBindingManager::createGlobalShaderResourceBindingSingleResource(
        const std::string& sShaderResourceName, GpuResource* pResourceToBind) {
        std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()> vResourcesToBind;
        for (size_t i = 0; i < vResourcesToBind.size(); i++) {
            vResourcesToBind[i] = pResourceToBind;
        }
        return createGlobalShaderResourceBindingResourcePerFrame(sShaderResourceName, vResourcesToBind);
    }

    std::optional<Error>
    GlobalShaderResourceBindingManager::onNewGraphicsPipelineCreated(Pipeline* pNewPipeline) {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxActiveBindings.first);

        for (const auto& pBinding : mtxActiveBindings.second) {
            auto optionalError = pBinding->bindToPipelines(pNewPipeline);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        return {};
    }

    std::optional<Error>
    GlobalShaderResourceBindingManager::onAllGraphicsPipelinesRecreatedInternalResources() {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxActiveBindings.first);

        for (const auto& pBinding : mtxActiveBindings.second) {
            auto optionalError = pBinding->bindToPipelines(nullptr);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        return {};
    }

    GlobalShaderResourceBindingManager::~GlobalShaderResourceBindingManager() {
        std::scoped_lock guard(mtxActiveBindings.first);

        if (mtxActiveBindings.second.empty()) [[likely]] {
            return;
        }

        Error error(std::format(
            "binding manager is being destroyed but there are still {} binding(s) exist",
            mtxActiveBindings.second.size()));
        error.showError();
        // don't throw in destructor
    }

    std::optional<Error>
    GlobalShaderResourceBindingManager::registerNewBinding(GlobalShaderResourceBinding* pBinding) {
        PROFILE_FUNC;

        const auto pMtxGraphicsPipelines = pPipelineManager->getGraphicsPipelines();

        // Lock both graphics pipelines and active bindings to avoid a possible AB-BA deadlock.
        std::scoped_lock guard(pMtxGraphicsPipelines->first, mtxActiveBindings.first);

        // Add binding.
        mtxActiveBindings.second.insert(pBinding);

        // Bind to all pipelines.
        auto optionalError = pBinding->bindToPipelines(nullptr);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    void GlobalShaderResourceBindingManager::unregisterBinding(GlobalShaderResourceBinding* pBinding) {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxActiveBindings.first);

        // Find binding.
        const auto bindingIt = mtxActiveBindings.second.find(pBinding);
        if (bindingIt == mtxActiveBindings.second.end()) [[unlikely]] {
            Error error("a binding tried to unregister itself but it didn't existed in the set of registered "
                        "bindings");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Remove.
        mtxActiveBindings.second.erase(bindingIt);
    }

}
