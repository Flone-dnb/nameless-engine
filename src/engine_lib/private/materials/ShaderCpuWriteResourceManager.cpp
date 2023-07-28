#include "ShaderCpuWriteResourceManager.h"

// Custom.
#include "io/Logger.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "materials/hlsl/HlslShaderResource.h"
#endif
#include "render/vulkan/VulkanRenderer.h"
#include "materials/glsl/GlslShaderResource.h"

namespace ne {

    std::variant<ShaderCpuWriteResourceUniquePtr, Error>
    ShaderCpuWriteResourceManager::createShaderCpuWriteResource(
        const std::string& sShaderResourceName,
        const std::string& sResourceAdditionalInfo,
        size_t iResourceSizeInBytes,
        Pso* pUsedPso,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource) {
        // Create new resource.
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            auto result = HlslShaderCpuWriteResource::create(
                sShaderResourceName,
                sResourceAdditionalInfo,
                iResourceSizeInBytes,
                pUsedPso,
                onStartedUpdatingResource,
                onFinishedUpdatingResource);
            return handleResourceCreation(std::move(result));
        }
#endif
        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            auto result = GlslShaderCpuWriteResource::create(
                sShaderResourceName,
                sResourceAdditionalInfo,
                iResourceSizeInBytes,
                pUsedPso,
                onStartedUpdatingResource,
                onFinishedUpdatingResource);
            return handleResourceCreation(std::move(result));
        }

        Error error("unexpected renderer");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

    std::variant<ShaderCpuWriteResourceUniquePtr, Error>
    ShaderCpuWriteResourceManager::handleResourceCreation(
        std::variant<std::unique_ptr<ShaderCpuWriteResource>, Error> result) {
        // Check if there was an error.
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }

        auto pResource = std::get<std::unique_ptr<ShaderCpuWriteResource>>(std::move(result));
        auto pRawResource = pResource.get();

        // Add to be considered.
        std::scoped_lock guard(mtxShaderCpuWriteResources.first);
        mtxShaderCpuWriteResources.second.vAll.push_back(std::move(pResource));
        mtxShaderCpuWriteResources.second.toBeUpdated.insert(pRawResource);

        return ShaderCpuWriteResourceUniquePtr(this, pRawResource);
    }

    void ShaderCpuWriteResourceManager::updateResources(size_t iCurrentFrameResourceIndex) {
        std::scoped_lock shaderRwResourceGuard(mtxShaderCpuWriteResources.first);

        // Update resources and erase ones that no longer need an update.
        for (auto it = mtxShaderCpuWriteResources.second.toBeUpdated.begin();
             it != mtxShaderCpuWriteResources.second.toBeUpdated.end();) {
            if ((*it)->updateResource(iCurrentFrameResourceIndex)) {
                it = mtxShaderCpuWriteResources.second.toBeUpdated.erase(it);
            } else {
                ++it;
            }
        }
    }

    void ShaderCpuWriteResourceManager::markResourceAsNeedsUpdate(ShaderCpuWriteResource* pResource) {
        std::scoped_lock guard(mtxShaderCpuWriteResources.first);

        // See if we need to move this resource to "to be updated" array.
        auto it = mtxShaderCpuWriteResources.second.toBeUpdated.find(pResource);
        if (it == mtxShaderCpuWriteResources.second.toBeUpdated.end()) {
            mtxShaderCpuWriteResources.second.toBeUpdated.insert(pResource);
        }

        pResource->markAsNeedsUpdate();
    }

    void ShaderCpuWriteResourceManager::destroyResource(ShaderCpuWriteResource* pResourceToDestroy) {
        std::scoped_lock guard(mtxShaderCpuWriteResources.first);

        // Remove from "all" array.
        for (auto it = mtxShaderCpuWriteResources.second.vAll.begin();
             it != mtxShaderCpuWriteResources.second.vAll.end();
             ++it) {
            if (it->get() == pResourceToDestroy) {
                // Destroy the object from the "all" array first.
                mtxShaderCpuWriteResources.second.vAll.erase(it);

                // Remove raw pointer from "to be updated" array (if resource needed an update).
                auto toBeUpdatedIt = mtxShaderCpuWriteResources.second.toBeUpdated.find(pResourceToDestroy);
                if (toBeUpdatedIt != mtxShaderCpuWriteResources.second.toBeUpdated.end()) {
                    mtxShaderCpuWriteResources.second.toBeUpdated.erase(toBeUpdatedIt);
                }

                return;
            }
        }

        // Maybe the specified resource pointer is invalid.
        Logger::get().error("unable to find the specified resource to be destroyed");
    }

    std::pair<std::recursive_mutex, ShaderCpuWriteResourceManager::Resources>*
    ShaderCpuWriteResourceManager::getResources() {
        return &mtxShaderCpuWriteResources;
    }

    ShaderCpuWriteResourceManager::ShaderCpuWriteResourceManager(Renderer* pRenderer) {
        this->pRenderer = pRenderer;
    }
} // namespace ne
