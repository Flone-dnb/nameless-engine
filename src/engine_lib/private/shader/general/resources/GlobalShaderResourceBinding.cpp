#include "GlobalShaderResourceBinding.h"

// Custom.
#include "render/vulkan/VulkanRenderer.h"
#include "shader/glsl/resources/GlslGlobalShaderResourceBinding.h"
#include "shader/general/resources/GlobalShaderResourceBindingManager.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "shader/hlsl/resources/HlslGlobalShaderResourceBinding.h"
#endif

namespace ne {

    GlobalShaderResourceBinding::GlobalShaderResourceBinding(
        GlobalShaderResourceBindingManager* pManager,
        const std::string& sShaderResourceName,
        const std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()>& vResourcesToBind)
        : sShaderResourceName(sShaderResourceName), vBindedResources(vResourcesToBind), pManager(pManager) {}

    void GlobalShaderResourceBinding::unregisterBinding() { pManager->unregisterBinding(this); }

    const std::string& GlobalShaderResourceBinding::getShaderResourceName() const {
        return sShaderResourceName;
    }

    std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()>
    GlobalShaderResourceBinding::getBindedResources() const {
        return vBindedResources;
    }

    std::optional<Error> GlobalShaderResourceBinding::create(
        Renderer* pRenderer,
        GlobalShaderResourceBindingManager* pManager,
        const std::string& sShaderResourceName,
        const std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()>& vResourcesToBind) {
        std::shared_ptr<GlobalShaderResourceBinding> pBinding;

        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            pBinding = std::shared_ptr<GlslGlobalShaderResourceBinding>(
                new GlslGlobalShaderResourceBinding(pManager, sShaderResourceName, vResourcesToBind));
        } else if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            pBinding = std::shared_ptr<HlslGlobalShaderResourceBinding>(
                new HlslGlobalShaderResourceBinding(pManager, sShaderResourceName, vResourcesToBind));
        } else {
            return Error("unsupported renderer");
        }

        // DON'T bind to all pipelines here, this will be done in the manager during the registration.

        auto optionalError = pManager->registerNewBinding(pBinding.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Assign to resources so that once these resources are destroyed the binding is unregistered.
        for (const auto& pResource : vResourcesToBind) {
            pResource->pGlobalShaderResourceBinding = pBinding;
        }

        return {};
    }

}
