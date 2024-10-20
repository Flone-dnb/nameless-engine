#include "GpuResourceManager.h"

#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "render/directx/resource/DirectXResourceManager.h"
#endif
#include "render/vulkan/VulkanRenderer.h"
#include "render/vulkan/resource/VulkanResourceManager.h"
#include "shader/general/resource/cpuwrite/DynamicCpuWriteShaderResourceArrayManager.h"

namespace ne {

    Renderer* GpuResourceManager::getRenderer() const { return pRenderer; }

    TextureManager* GpuResourceManager::getTextureManager() const { return pTextureManager.get(); }

    ShadowMapManager* GpuResourceManager::getShadowMapManager() const { return pShadowMapManager.get(); }

    DynamicCpuWriteShaderResourceArrayManager*
    GpuResourceManager::getDynamicCpuWriteShaderResourceArrayManager() const {
        return pDynamicCpuWriteShaderResourceArrayManager.get();
    }

    size_t GpuResourceManager::getTotalAliveResourceCount() { return iAliveResourceCount.load(); }

    std::variant<std::unique_ptr<GpuResourceManager>, Error>
    GpuResourceManager::createRendererSpecificManager(Renderer* pRenderer) {
#if defined(WIN32)
        if (auto pDirectXRenderer = dynamic_cast<DirectXRenderer*>(pRenderer)) {
            // Create DirectX resource manager.
            auto result = DirectXResourceManager::create(pDirectXRenderer);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            return std::get<std::unique_ptr<DirectXResourceManager>>(std::move(result));
        }
#endif

        if (auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pRenderer)) {
            // Create Vulkan resource manager.
            auto result = VulkanResourceManager::create(pVulkanRenderer);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            return std::get<std::unique_ptr<VulkanResourceManager>>(std::move(result));
        }

        return Error("unsupported renderer");
    }

    std::variant<std::unique_ptr<GpuResourceManager>, Error> GpuResourceManager::create(Renderer* pRenderer) {
        // Create render-specific object.
        auto resourceManagerResult = createRendererSpecificManager(pRenderer);
        if (std::holds_alternative<Error>(resourceManagerResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(resourceManagerResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto pResourceManager =
            std::get<std::unique_ptr<GpuResourceManager>>(std::move(resourceManagerResult));

        // Create texture manager.
        pResourceManager->pTextureManager = std::make_unique<TextureManager>(pResourceManager.get());

        {
            // Create shadow map manager.
            auto result = ShadowMapManager::create(pResourceManager.get());
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            pResourceManager->pShadowMapManager =
                std::get<std::unique_ptr<ShadowMapManager>>(std::move(result));
        }

        // Create dynamic CPU-write shader array manager.

        return pResourceManager;
    }

    GpuResourceManager::GpuResourceManager(Renderer* pRenderer) : pRenderer(pRenderer) {
        // Create CPU-write shader array manager.
        pDynamicCpuWriteShaderResourceArrayManager =
            std::unique_ptr<DynamicCpuWriteShaderResourceArrayManager>(
                new DynamicCpuWriteShaderResourceArrayManager(this));
    }

    void GpuResourceManager::resetManagers() {
        pDynamicCpuWriteShaderResourceArrayManager = nullptr;
        pShadowMapManager = nullptr;

        // Lastly destroy texture manager.
        pTextureManager = nullptr;
    }
} // namespace ne
