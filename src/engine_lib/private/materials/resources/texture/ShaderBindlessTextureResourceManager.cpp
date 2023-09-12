#include "ShaderBindlessTextureResourceManager.h"

// Custom.
#include "io/Logger.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif
#include "render/vulkan/VulkanRenderer.h"
#include "materials/glsl/resources/GlslShaderBindlessTextureResource.h"
#include "misc/Profiler.hpp"

namespace ne {

    std::variant<ShaderBindlessTextureResourceUniquePtr, Error>
    ShaderBindlessTextureResourceManager::createShaderBindlessTextureResource(
        const std::string& sShaderResourceName,
        const std::string& sResourceAdditionalInfo,
        Pipeline* pUsedPipeline,
        std::unique_ptr<TextureHandle> pTextureToUse) {
        // Create new resource.
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            Error error("unsupported renderer");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
            //            auto result = HlslShaderBindlessTextureResource::create(
            //                sShaderResourceName,
            //                sResourceAdditionalInfo,
            //                iResourceSizeInBytes,
            //                pUsedPipeline,
            //                onStartedUpdatingResource,
            //                onFinishedUpdatingResource);
            //            return handleResourceCreation(std::move(result));
        }
#endif
        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            auto result = GlslShaderBindlessTextureResource::create(
                sShaderResourceName, sResourceAdditionalInfo, pUsedPipeline, std::move(pTextureToUse));
            return handleResourceCreation(std::move(result));
        }

        Error error("unexpected renderer");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

    std::variant<ShaderBindlessTextureResourceUniquePtr, Error>
    ShaderBindlessTextureResourceManager::handleResourceCreation(
        std::variant<std::unique_ptr<ShaderBindlessTextureResource>, Error> result) {
        // Check if there was an error.
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Get results.
        auto pResource = std::get<std::unique_ptr<ShaderBindlessTextureResource>>(std::move(result));
        auto pRawResource = pResource.get();

        // Add to be considered.
        std::scoped_lock guard(mtxShaderBindlessTextureResources.first);
        mtxShaderBindlessTextureResources.second[pRawResource] = std::move(pResource);

        return ShaderBindlessTextureResourceUniquePtr(this, pRawResource);
    }

    void
    ShaderBindlessTextureResourceManager::destroyResource(ShaderBindlessTextureResource* pResourceToDestroy) {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxShaderBindlessTextureResources.first);

        // Find this resource.
        const auto it = mtxShaderBindlessTextureResources.second.find(pResourceToDestroy);
        if (it == mtxShaderBindlessTextureResources.second.end()) [[unlikely]] {
            // Maybe the specified resource pointer is invalid.
            Logger::get().error(
                "failed to find the specified shader bindless texture resource to be destroyed");
            return;
        }

        // Destroy resource.
        mtxShaderBindlessTextureResources.second.erase(it);
    }

    std::pair<
        std::recursive_mutex,
        std::unordered_map<ShaderBindlessTextureResource*, std::unique_ptr<ShaderBindlessTextureResource>>>*
    ShaderBindlessTextureResourceManager::getResources() {
        return &mtxShaderBindlessTextureResources;
    }

    ShaderBindlessTextureResourceManager::ShaderBindlessTextureResourceManager(Renderer* pRenderer) {
        this->pRenderer = pRenderer;
    }

    ShaderBindlessTextureResourceManager::~ShaderBindlessTextureResourceManager() {
        std::scoped_lock guard(mtxShaderBindlessTextureResources.first);

        // Make sure there are no CPU write resources.
        if (!mtxShaderBindlessTextureResources.second.empty()) [[unlikely]] {
            // Prepare their names and count.
            std::unordered_map<std::string, size_t> leftResources;
            for (const auto& [pRawResource, pResource] : mtxShaderBindlessTextureResources.second) {
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
                "shader bindless texture resource manager is being destroyed but there are still {} shader "
                "bindless texture resource(s) alive:\n{}",
                mtxShaderBindlessTextureResources.second.size(),
                sLeftResources));
            error.showError();
            return; // don't throw in destructor, just quit
        }
    }
} // namespace ne
