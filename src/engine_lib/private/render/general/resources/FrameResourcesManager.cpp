#include "FrameResourcesManager.h"

// Custom.
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "render/directx/resources/DirectXFrameResource.h"
#endif
#include "render/vulkan/VulkanRenderer.h"
#include "render/vulkan/resources/VulkanFrameResource.h"

// External.
#include "fmt/core.h"

namespace ne {

    std::array<std::unique_ptr<FrameResource>, FrameResourcesManager::getFrameResourcesCount()>
    FrameResourcesManager::createRenderDependentFrameResources(Renderer* pRenderer) {
        std::array<std::unique_ptr<FrameResource>, FrameResourcesManager::getFrameResourcesCount()>
            vResources;

#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            for (unsigned int i = 0; i < getFrameResourcesCount(); i++) {
                vResources[i] = std::make_unique<DirectXFrameResource>();
            }
            return vResources;
        }
#endif

        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            for (unsigned int i = 0; i < getFrameResourcesCount(); i++) {
                vResources[i] = std::make_unique<VulkanFrameResource>();
            }
            return vResources;
        }

        Error error("unsupported renderer");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

    FrameResourcesManager::FrameResourcesManager(Renderer* pRenderer) {
        this->pRenderer = pRenderer;

        vFrameResources = createRenderDependentFrameResources(pRenderer);

        mtxCurrentFrameResource.second.iCurrentFrameResourceIndex = 0;
        mtxCurrentFrameResource.second.pResource =
            vFrameResources[mtxCurrentFrameResource.second.iCurrentFrameResourceIndex].get();
    }

    std::variant<std::unique_ptr<FrameResourcesManager>, Error>
    FrameResourcesManager::create(Renderer* pRenderer) {
        auto pManager = std::unique_ptr<FrameResourcesManager>(new FrameResourcesManager(pRenderer));

        for (unsigned int i = 0; i < getFrameResourcesCount(); i++) {
            // Create a constant buffer with frame-global data per frame.
            auto result = pRenderer->getResourceManager()->createResourceWithCpuWriteAccess(
                fmt::format("frame constants #{}", i),
                sizeof(FrameConstants),
                1,
                CpuVisibleShaderResourceUsageDetails(true));
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
        }

        return pManager;
    }

    std::pair<std::recursive_mutex, FrameResourcesManager::CurrentFrameResource>*
    FrameResourcesManager::getCurrentFrameResource() {
        return &mtxCurrentFrameResource;
    }

    void FrameResourcesManager::switchToNextFrameResource() {
        std::scoped_lock resourceGuard(mtxCurrentFrameResource.first);

        // Increment current frame resource index.
        if (mtxCurrentFrameResource.second.iCurrentFrameResourceIndex + 1 == getFrameResourcesCount()) {
            mtxCurrentFrameResource.second.iCurrentFrameResourceIndex = 0;
        } else {
            mtxCurrentFrameResource.second.iCurrentFrameResourceIndex += 1;
        }

        // Update current frame resource pointer.
        mtxCurrentFrameResource.second.pResource =
            vFrameResources[mtxCurrentFrameResource.second.iCurrentFrameResourceIndex].get();
    }

} // namespace ne
