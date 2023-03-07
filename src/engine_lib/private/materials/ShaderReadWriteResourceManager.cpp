#include "ShaderReadWriteResourceManager.h"

// Custom.
#include "io/Logger.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "materials/hlsl/HlslShaderResource.h"
#endif

namespace ne {

    std::variant<ShaderCpuReadWriteResourceUniquePtr, Error>
    ShaderCpuReadWriteResourceManager::createShaderCpuReadWriteResource(
        const std::string& sShaderResourceName,
        const std::string& sResourceAdditionalInfo,
        size_t iResourceSizeInBytes,
        Pso* pUsedPso,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource) {
        // Create new resource.
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            auto result = HlslShaderCpuReadWriteResource::create(
                sShaderResourceName,
                sResourceAdditionalInfo,
                iResourceSizeInBytes,
                pUsedPso,
                onStartedUpdatingResource,
                onFinishedUpdatingResource);
            return handleResourceCreation(std::move(result));
        }
#endif
        // TODO:
        // if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr){
        //     ...
        // }

        throw std::runtime_error("not implemented");
    }

    std::variant<ShaderCpuReadWriteResourceUniquePtr, Error>
    ShaderCpuReadWriteResourceManager::handleResourceCreation(
        std::variant<std::unique_ptr<ShaderCpuReadWriteResource>, Error> result) {
        // Check if there was an error.
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }

        auto pResource = std::get<std::unique_ptr<ShaderCpuReadWriteResource>>(std::move(result));
        auto pRawResource = pResource.get();

        // Add to be considered.
        std::scoped_lock guard(mtxShaderCpuReadWriteResources.first);
        mtxShaderCpuReadWriteResources.second.vAll.push_back(std::move(pResource));
        mtxShaderCpuReadWriteResources.second.toBeUpdated.insert(pRawResource);

        return ShaderCpuReadWriteResourceUniquePtr(this, pRawResource);
    }

    void ShaderCpuReadWriteResourceManager::updateResources(size_t iCurrentFrameResourceIndex) {
        std::scoped_lock shaderRwResourceGuard(mtxShaderCpuReadWriteResources.first);

        // Update resources and erase ones that no longer need an update.
        for (auto it = mtxShaderCpuReadWriteResources.second.toBeUpdated.begin();
             it != mtxShaderCpuReadWriteResources.second.toBeUpdated.end();) {
            if ((*it)->updateResource(iCurrentFrameResourceIndex)) {
                it = mtxShaderCpuReadWriteResources.second.toBeUpdated.erase(it);
            } else {
                ++it;
            }
        }
    }

    void ShaderCpuReadWriteResourceManager::markResourceAsNeedsUpdate(ShaderCpuReadWriteResource* pResource) {
        std::scoped_lock guard(mtxShaderCpuReadWriteResources.first);

        // See if we need to move this resource to "to be updated" array.
        auto it = mtxShaderCpuReadWriteResources.second.toBeUpdated.find(pResource);
        if (it == mtxShaderCpuReadWriteResources.second.toBeUpdated.end()) {
            mtxShaderCpuReadWriteResources.second.toBeUpdated.insert(pResource);
        }

        pResource->markAsNeedsUpdate();
    }

    void ShaderCpuReadWriteResourceManager::destroyResource(ShaderCpuReadWriteResource* pResourceToDestroy) {
        std::scoped_lock guard(mtxShaderCpuReadWriteResources.first);

        // Remove from "to be updated" array (if resource needed an update).
        auto toBeUpdatedIt = mtxShaderCpuReadWriteResources.second.toBeUpdated.find(pResourceToDestroy);
        if (toBeUpdatedIt != mtxShaderCpuReadWriteResources.second.toBeUpdated.end()) {
            mtxShaderCpuReadWriteResources.second.toBeUpdated.erase(toBeUpdatedIt);
        }

        // Remove from "all" array.
        for (auto it = mtxShaderCpuReadWriteResources.second.vAll.begin();
             it != mtxShaderCpuReadWriteResources.second.vAll.end();
             ++it) {
            if (it->get() == pResourceToDestroy) {
                mtxShaderCpuReadWriteResources.second.vAll.erase(it);
                return;
            }
        }

        // Maybe the specified resource pointer is invalid.
        Logger::get().error(
            "unable to find the specified resource to be destroyed",
            sShaderCpuReadWriteResourceManagerLogCategory);
    }

    std::pair<std::recursive_mutex, ShaderCpuReadWriteResourceManager::Resources>*
    ShaderCpuReadWriteResourceManager::getResources() {
        return &mtxShaderCpuReadWriteResources;
    }

    ShaderCpuReadWriteResourceManager::ShaderCpuReadWriteResourceManager(Renderer* pRenderer) {
        this->pRenderer = pRenderer;
    }
} // namespace ne
