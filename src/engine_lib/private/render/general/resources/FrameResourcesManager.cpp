#include "FrameResourcesManager.h"

// Custom.
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif

// External.
#include "fmt/core.h"

namespace ne {

    FrameResourcesManager::FrameResourcesManager(Renderer* pRenderer) {
        this->pRenderer = pRenderer;

        for (unsigned int i = 0; i < getFrameResourcesCount(); i++) {
            vFrameResources[i] = std::make_unique<FrameResource>();
        }

        mtxCurrentFrameResource.second.iCurrentFrameResourceIndex = 0;
        mtxCurrentFrameResource.second.pCurrentFrameResource = vFrameResources[0].get();
    }

    std::variant<std::unique_ptr<FrameResourcesManager>, Error>
    FrameResourcesManager::create(Renderer* pRenderer) {
        auto pManager = std::unique_ptr<FrameResourcesManager>(new FrameResourcesManager(pRenderer));

        // Create a constant buffer (with frame-global data) per frame.
        for (unsigned int i = 0; i < getFrameResourcesCount(); i++) {
            auto result = pRenderer->getResourceManager()->createResourceWithCpuAccess(
                fmt::format("frame constants #{}", i), sizeof(FrameConstants), 1, true);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addEntry();
                return error;
            }
            pManager->vFrameResources[i]->pFrameConstantBuffer =
                std::get<std::unique_ptr<UploadBuffer>>(std::move(result));
        }

        // Create renderer-specific data per frame.
#if defined(WIN32)
        if (auto pDirectXRenderer = dynamic_cast<DirectXRenderer*>(pRenderer)) {
            // Create a command allocator per frame.
            for (unsigned int i = 0; i < getFrameResourcesCount(); i++) {
                auto hResult = pDirectXRenderer->getD3dDevice()->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(pManager->vFrameResources[i]->pCommandAllocator.GetAddressOf()));
                if (FAILED(hResult)) {
                    return Error(hResult);
                }
            }
        }
#elif __linux__
        static_assert(false, "not implemented");
//        if (auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pRenderer)) {
//            // TODO: vulkan
//        } else {
//            return Error("not implemented");
//        }
#endif

        return pManager;
    }

    std::pair<std::recursive_mutex*, FrameResource*> FrameResourcesManager::getCurrentFrameResource() {
        return std::make_pair(
            &mtxCurrentFrameResource.first, mtxCurrentFrameResource.second.pCurrentFrameResource);
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
        mtxCurrentFrameResource.second.pCurrentFrameResource =
            vFrameResources[mtxCurrentFrameResource.second.iCurrentFrameResourceIndex].get();
    }

} // namespace ne
