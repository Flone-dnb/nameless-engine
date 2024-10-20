#include "ShadowMapArrayIndexManager.h"

// Custom.
#include "render/general/resource/shadow/ShadowMapHandle.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "render/directx/resource/shadow/DirectXShadowMapArrayIndexManager.h"
#endif
#include "render/vulkan/resource/shadow/VulkanShadowMapArrayIndexManager.h"
#include "render/vulkan/VulkanRenderer.h"

namespace ne {

    ShadowMapArrayIndexManager::ShadowMapArrayIndexManager(
        Renderer* pRenderer, const std::string& sShaderArrayResourceName)
        : pRenderer(pRenderer), sShaderArrayResourceName(sShaderArrayResourceName) {}

    std::variant<std::unique_ptr<ShadowMapArrayIndexManager>, Error> ShadowMapArrayIndexManager::create(
        Renderer* pRenderer,
        GpuResourceManager* pResourceManager,
        const std::string& sShaderArrayResourceName) {
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            // Create DirectX manager.
            auto result = DirectXShadowMapArrayIndexManager::create(
                pRenderer, pResourceManager, sShaderArrayResourceName);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            return std::get<std::unique_ptr<DirectXShadowMapArrayIndexManager>>(std::move(result));
        }
#endif

        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            // Create Vulkan manager.
            return std::unique_ptr<VulkanShadowMapArrayIndexManager>(
                new VulkanShadowMapArrayIndexManager(pRenderer, sShaderArrayResourceName));
        }

        return Error("unsupported renderer");
    }

    std::string_view ShadowMapArrayIndexManager::getShaderArrayResourceName() {
        return sShaderArrayResourceName;
    }

    Renderer* ShadowMapArrayIndexManager::getRenderer() const { return pRenderer; }

    void ShadowMapArrayIndexManager::changeShadowMapArrayIndex(
        ShadowMapHandle* pShadowMapHandle, unsigned int iNewArrayIndex) {
        pShadowMapHandle->changeArrayIndex(iNewArrayIndex);
    }

}
