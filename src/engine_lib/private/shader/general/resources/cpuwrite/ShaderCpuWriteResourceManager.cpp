#include "ShaderCpuWriteResourceManager.h"

// Custom.
#include "io/Logger.h"
#include "misc/Profiler.hpp"
#include "render/general/pipeline/Pipeline.h"

namespace ne {

    std::variant<ShaderCpuWriteResourceUniquePtr, Error>
    ShaderCpuWriteResourceManager::createShaderCpuWriteResource(
        const std::string& sShaderResourceName,
        const std::string& sResourceAdditionalInfo,
        size_t iResourceDataSizeInBytes,
        const std::unordered_set<Pipeline*>& pipelinesToUse,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource) {
        auto result = ShaderCpuWriteResource::create(
            sShaderResourceName,
            sResourceAdditionalInfo,
            iResourceDataSizeInBytes,
            pipelinesToUse,
            onStartedUpdatingResource,
            onFinishedUpdatingResource);
        return handleResourceCreation(std::move(result));
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

        // Get results.
        auto pResource = std::get<std::unique_ptr<ShaderCpuWriteResource>>(std::move(result));
        auto pRawResource = pResource.get();

        // Add to be considered.
        std::scoped_lock guard(mtxShaderCpuWriteResources.first);
        mtxShaderCpuWriteResources.second.all[pRawResource] = std::move(pResource);

        // Add to be updated for each frame resource.
        for (auto& set : mtxShaderCpuWriteResources.second.vToBeUpdated) {
            set.insert(pRawResource);
        }

        return ShaderCpuWriteResourceUniquePtr(this, pRawResource);
    }

    void ShaderCpuWriteResourceManager::updateResources(size_t iCurrentFrameResourceIndex) {
        PROFILE_FUNC;

        std::scoped_lock shaderRwResourceGuard(mtxShaderCpuWriteResources.first);

        auto& resourcesToUpdate = mtxShaderCpuWriteResources.second.vToBeUpdated[iCurrentFrameResourceIndex];

        if (resourcesToUpdate.empty()) {
            // Nothing to update.
            return;
        }

        // Copy new resource data to the GPU resources of the current frame.
        for (const auto& pResource : resourcesToUpdate) {
            pResource->updateResource(iCurrentFrameResourceIndex);
        }

        // Clear array since we updated all resources for the current frame.
        resourcesToUpdate.clear();
    }

    void ShaderCpuWriteResourceManager::markResourceAsNeedsUpdate(ShaderCpuWriteResource* pResource) {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxShaderCpuWriteResources.first);

        // Self check: check if this resource even exists.
        auto foundResourceIt = mtxShaderCpuWriteResources.second.all.find(pResource);
        if (foundResourceIt == mtxShaderCpuWriteResources.second.all.end()) [[unlikely]] {
            // don't use the pointer as it may reference a deleted memory
            Logger::get().error("failed to find the specified shader CPU write resource in the array "
                                "of alive resources to mark it as \"needs update\"");
            return;
        }

        // Add to be updated for each frame resource,
        // even if it's already marked as "to be updated" `std::set` guarantees element uniqueness
        // so there's no need to check if the resource already marked as "to be updated".
        for (auto& set : mtxShaderCpuWriteResources.second.vToBeUpdated) {
            set.insert(pResource);
        }
    }

    void ShaderCpuWriteResourceManager::destroyResource(ShaderCpuWriteResource* pResourceToDestroy) {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxShaderCpuWriteResources.first);

        // Remove raw pointer from "to be updated" array (if resource needed an update).
        for (auto& set : mtxShaderCpuWriteResources.second.vToBeUpdated) {
            set.erase(pResourceToDestroy);
        }

        // Destroy resource.
        const auto it = mtxShaderCpuWriteResources.second.all.find(pResourceToDestroy);
        if (it == mtxShaderCpuWriteResources.second.all.end()) [[unlikely]] {
            // Maybe the specified resource pointer is invalid.
            Logger::get().error("failed to find the specified shader CPU write resource to be destroyed");
            return;
        }
        mtxShaderCpuWriteResources.second.all.erase(it);
    }

    std::pair<std::recursive_mutex, ShaderCpuWriteResourceManager::Resources>*
    ShaderCpuWriteResourceManager::getResources() {
        return &mtxShaderCpuWriteResources;
    }

    ShaderCpuWriteResourceManager::ShaderCpuWriteResourceManager(Renderer* pRenderer) {
        this->pRenderer = pRenderer;
    }

    ShaderCpuWriteResourceManager::~ShaderCpuWriteResourceManager() {
        std::scoped_lock guard(mtxShaderCpuWriteResources.first);

        // Make sure there are no CPU write resources.
        if (!mtxShaderCpuWriteResources.second.all.empty()) [[unlikely]] {
            // Prepare their names and count.
            std::unordered_map<std::string, size_t> leftResources;
            for (const auto& [pRawResource, pResource] : mtxShaderCpuWriteResources.second.all) {
                const auto it = leftResources.find(pResource->getResourceName());
                if (it == leftResources.end()) {
                    leftResources[pResource->getResourceName()] = 1;
                } else {
                    it->second += 1;
                }
            }

            // Prepare output message.
            std::string sLeftResources;
            for (const auto& [sResourceName, iLeftCount] : leftResources) {
                sLeftResources += std::format("- {}, left: {}", sResourceName, iLeftCount);
            }

            // Show error.
            Error error(std::format(
                "shader CPU write resource manager is being destroyed but there are still {} shader CPU "
                "write resource(s) alive:\n{}",
                mtxShaderCpuWriteResources.second.all.size(),
                sLeftResources));
            error.showError();
            return; // don't throw in destructor
        }

        // Make sure there are no resource references in the "to update" array.
        for (const auto& set : mtxShaderCpuWriteResources.second.vToBeUpdated) {
            if (!set.empty()) [[unlikely]] {
                // Show error.
                Error error(std::format(
                    "shader CPU write resource manager is being destroyed but there are still {} raw "
                    "references to shader CPU write resource(s) stored in the manager in the "
                    "\"to be updated\" list",
                    set.size()));
                error.showError();
                return; // don't throw in destructor
            }
        }
    }
} // namespace ne
