#include "ShaderCpuWriteResourceManager.h"

// Custom.
#include "io/Logger.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "materials/hlsl/HlslShaderResource.h"
#endif
#include "render/vulkan/VulkanRenderer.h"
#include "materials/glsl/GlslShaderResource.h"
#include "misc/Profiler.hpp"
#include "render/general/pipeline/Pipeline.h"

namespace ne {

    std::variant<ShaderCpuWriteResourceUniquePtr, Error>
    ShaderCpuWriteResourceManager::createShaderCpuWriteResource(
        const std::string& sShaderResourceName,
        const std::string& sResourceAdditionalInfo,
        size_t iResourceSizeInBytes,
        Pipeline* pUsedPipeline,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource) {
        // Create new resource.
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            auto result = HlslShaderCpuWriteResource::create(
                sShaderResourceName,
                sResourceAdditionalInfo,
                iResourceSizeInBytes,
                pUsedPipeline,
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
                pUsedPipeline,
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
            error.addCurrentLocationToErrorStack();
            return error;
        }

        auto pResource = std::get<std::unique_ptr<ShaderCpuWriteResource>>(std::move(result));
        auto pRawResource = pResource.get();

        // Add to be considered.
        std::scoped_lock guard(mtxShaderCpuWriteResources.first);
        mtxShaderCpuWriteResources.second.all.vector.push_back(std::move(pResource));
        mtxShaderCpuWriteResources.second.all.set.insert(pRawResource);

        // Add to be updated for each frame resource.
        for (auto& set : mtxShaderCpuWriteResources.second.toBeUpdated) {
            set.insert(pRawResource);
        }

        return ShaderCpuWriteResourceUniquePtr(this, pRawResource);
    }

    void ShaderCpuWriteResourceManager::updateResources(size_t iCurrentFrameResourceIndex) {
        PROFILE_FUNC;

        std::scoped_lock shaderRwResourceGuard(mtxShaderCpuWriteResources.first);

        if (mtxShaderCpuWriteResources.second.toBeUpdated[iCurrentFrameResourceIndex].empty()) {
            // Nothing to update.
            return;
        }

        // Copy new resource data to the GPU resources of the current frame resource.

        static_assert(
            std::is_same_v<
                decltype(mtxShaderCpuWriteResources.second.toBeUpdated),
                std::array<
                    std::unordered_set<ShaderCpuWriteResource*>,
                    FrameResourcesManager::getFrameResourcesCount()>>,
            "update reinterpret_casts");

        // TODO: ugly but helps to avoid usage of virtual functions in this loop as it's executed
        // every frame with potentially lots of resources to be updated.
        // Prepare lambda to update resources.
        auto updateGlslResources = [&]() {
            const auto pGlslResources = reinterpret_cast<std::unordered_set<GlslShaderCpuWriteResource*>*>(
                &mtxShaderCpuWriteResources.second.toBeUpdated[iCurrentFrameResourceIndex]);
            for (const auto& pResource : *pGlslResources) {
                pResource->updateResource(iCurrentFrameResourceIndex);
            }
        };

#if !defined(WIN32)
        updateGlslResources();
#else
        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            updateGlslResources();
        } else {
            const auto pHlslResources = reinterpret_cast<std::unordered_set<HlslShaderCpuWriteResource*>*>(
                &mtxShaderCpuWriteResources.second.toBeUpdated[iCurrentFrameResourceIndex]);
            for (const auto& pResource : *pHlslResources) {
                pResource->updateResource(iCurrentFrameResourceIndex);
            }
        }
#endif

        // Clear array of resources to be updated for the current frame resource since
        // we updated all resources for the current frame resource.
        mtxShaderCpuWriteResources.second.toBeUpdated[iCurrentFrameResourceIndex].clear();
    }

    void ShaderCpuWriteResourceManager::markResourceAsNeedsUpdate(ShaderCpuWriteResource* pResource) {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxShaderCpuWriteResources.first);

        // Self check: check if this resource even exists.
        auto foundResourceIt = mtxShaderCpuWriteResources.second.all.set.find(pResource);
        if (foundResourceIt == mtxShaderCpuWriteResources.second.all.set.end()) [[unlikely]] {
            // don't use the pointer as it may reference a deleted memory
            Logger::get().error("failed to find the specified shader CPU write resource in the array "
                                "of alive resources to mark it as \"needs update\"");
            return;
        }

        // Add to be updated for each frame resource,
        // even if it's already marked as "to be updated" `std::set` guarantees element uniqueness
        // so there's no need to check if the resource already marked as "to be updated".
        for (auto& set : mtxShaderCpuWriteResources.second.toBeUpdated) {
            set.insert(pResource);
        }
    }

    void ShaderCpuWriteResourceManager::destroyResource(ShaderCpuWriteResource* pResourceToDestroy) {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxShaderCpuWriteResources.first);

        // Remove from "all" array.
        for (auto it = mtxShaderCpuWriteResources.second.all.vector.begin();
             it != mtxShaderCpuWriteResources.second.all.vector.end();
             ++it) {
            if (it->get() == pResourceToDestroy) {
                // Destroy the object from the "all" array first.
                mtxShaderCpuWriteResources.second.all.vector.erase(it);

                // Remove raw pointer from the set.
                mtxShaderCpuWriteResources.second.all.set.erase(pResourceToDestroy);

                // Remove raw pointer from "to be updated" array (if resource needed an update).
                for (auto& set : mtxShaderCpuWriteResources.second.toBeUpdated) {
                    set.erase(pResourceToDestroy);
                }

                return;
            }
        }

        // Maybe the specified resource pointer is invalid.
        Logger::get().error("failed to find the specified shader CPU write resource to be destroyed");
    }

    std::pair<std::recursive_mutex, ShaderCpuWriteResourceManager::Resources>*
    ShaderCpuWriteResourceManager::getResources() {
        return &mtxShaderCpuWriteResources;
    }

    ShaderCpuWriteResourceManager::ShaderCpuWriteResourceManager(Renderer* pRenderer) {
        this->pRenderer = pRenderer;
    }
} // namespace ne
