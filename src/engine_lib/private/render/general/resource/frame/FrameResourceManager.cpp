#include "FrameResourceManager.h"

// Standard.
#include <format>
#include <atomic>

// Custom.
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "render/directx/resource/DirectXFrameResource.h"
#endif
#include "shader/general/Shader.h"
#include "shader/general/resource/binding/global/GlobalShaderResourceBindingManager.h"
#include "render/general/resource/GpuResourceManager.h"
#include "render/vulkan/VulkanRenderer.h"
#include "render/vulkan/resource/VulkanFrameResource.h"

namespace ne {

    std::array<std::unique_ptr<FrameResource>, FrameResourceManager::getFrameResourceCount()>
    FrameResourceManager::createRenderDependentFrameResources(Renderer* pRenderer) {
        std::array<std::unique_ptr<FrameResource>, FrameResourceManager::getFrameResourceCount()> vResources;

#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            for (unsigned int i = 0; i < getFrameResourceCount(); i++) {
                vResources[i] = std::make_unique<DirectXFrameResource>();
            }
            return vResources;
        }
#endif

        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            for (unsigned int i = 0; i < getFrameResourceCount(); i++) {
                vResources[i] = std::make_unique<VulkanFrameResource>();
            }
            return vResources;
        }

        Error error("unsupported renderer");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

    FrameResourceManager::FrameResourceManager(Renderer* pRenderer) {
        static_assert(iFrameResourceCount == 2, "too much frames in-flight will introduce input latency");

        this->pRenderer = pRenderer;

        vFrameResources = createRenderDependentFrameResources(pRenderer);

        mtxCurrentFrameResource.second.iIndex = 0;
        mtxCurrentFrameResource.second.pResource =
            vFrameResources[mtxCurrentFrameResource.second.iIndex].get();
    }

    std::variant<std::unique_ptr<FrameResourceManager>, Error>
    FrameResourceManager::create(Renderer* pRenderer) {
        auto pManager = std::unique_ptr<FrameResourceManager>(new FrameResourceManager(pRenderer));

        std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()> vFrameResources;

        for (unsigned int i = 0; i < getFrameResourceCount(); i++) {
            // Create a constant buffer with frame-global data per frame.
            auto result = pRenderer->getResourceManager()->createResourceWithCpuWriteAccess(
                std::format("frame constants #{}", i), sizeof(FrameConstants), 1, false);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            pManager->vFrameResources[i]->pFrameConstantBuffer =
                std::get<std::unique_ptr<UploadBuffer>>(std::move(result));

            // Initialize render-specific data.
            auto optionalError = pManager->vFrameResources[i]->initialize(pRenderer);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }

            // Save to bind to pipelines later.
            vFrameResources[i] = pManager->vFrameResources[i]->pFrameConstantBuffer->getInternalResource();
        }

        // Bind frame data to all pipelines.
        auto optionalError = pRenderer->getGlobalShaderResourceBindingManager()
                                 ->createGlobalShaderResourceBindingResourcePerFrame(
                                     Shader::getFrameConstantsShaderResourceName(), vFrameResources);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return pManager;
    }

    std::pair<std::recursive_mutex, FrameResourceManager::CurrentFrameResource>*
    FrameResourceManager::getCurrentFrameResource() {
        return &mtxCurrentFrameResource;
    }

    void FrameResourceManager::switchToNextFrameResource() {
        std::scoped_lock resourceGuard(mtxCurrentFrameResource.first);

        // Switch to the next frame resource index.
        mtxCurrentFrameResource.second.iIndex =
            (mtxCurrentFrameResource.second.iIndex + 1) % getFrameResourceCount();

        // Update current frame resource pointer.
        mtxCurrentFrameResource.second.pResource =
            vFrameResources[mtxCurrentFrameResource.second.iIndex].get();
    }

    std::pair<std::recursive_mutex*, std::vector<FrameResource*>>
    FrameResourceManager::getAllFrameResources() {
        // Collect all resources to vector.
        std::vector<FrameResource*> vOutFrameResources(iFrameResourceCount);
        for (size_t i = 0; i < vFrameResources.size(); i++) {
            vOutFrameResources[i] = vFrameResources[i].get();
        }

        return {&mtxCurrentFrameResource.first, vOutFrameResources};
    }

} // namespace ne
