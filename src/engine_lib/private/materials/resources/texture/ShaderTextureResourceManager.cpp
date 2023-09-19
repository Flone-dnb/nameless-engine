#include "ShaderTextureResourceManager.h"

// Custom.
#include "io/Logger.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "materials/hlsl/resources/HlslShaderTextureResource.h"
#endif
#include "render/vulkan/VulkanRenderer.h"
#include "materials/glsl/resources/GlslShaderTextureResource.h"
#include "misc/Profiler.hpp"

namespace ne {

    std::variant<ShaderTextureResourceUniquePtr, Error>
    ShaderTextureResourceManager::createShaderTextureResource(
        const std::string& sShaderResourceName,
        const std::string& sResourceAdditionalInfo,
        std::unordered_set<Pipeline*> pipelinesToUse,
        std::unique_ptr<TextureHandle> pTextureToUse) {
        // Create new resource.
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            auto result = HlslShaderTextureResource::create(
                sShaderResourceName, pipelinesToUse, std::move(pTextureToUse));
            return handleResourceCreation(std::move(result));
        }
#endif
        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            auto result = GlslShaderTextureResource::create(
                sShaderResourceName, pipelinesToUse, std::move(pTextureToUse));
            return handleResourceCreation(std::move(result));
        }

        Error error("unexpected renderer");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

    std::variant<ShaderTextureResourceUniquePtr, Error> ShaderTextureResourceManager::handleResourceCreation(
        std::variant<std::unique_ptr<ShaderTextureResource>, Error> result) {
        // Check if there was an error.
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Get results.
        auto pResource = std::get<std::unique_ptr<ShaderTextureResource>>(std::move(result));
        auto pRawResource = pResource.get();

        // Add to be considered.
        std::scoped_lock guard(mtxShaderTextureResources.first);
        mtxShaderTextureResources.second[pRawResource] = std::move(pResource);

        return ShaderTextureResourceUniquePtr(this, pRawResource);
    }

    void ShaderTextureResourceManager::destroyResource(ShaderTextureResource* pResourceToDestroy) {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxShaderTextureResources.first);

        // Find this resource.
        const auto it = mtxShaderTextureResources.second.find(pResourceToDestroy);
        if (it == mtxShaderTextureResources.second.end()) [[unlikely]] {
            // Maybe the specified resource pointer is invalid.
            Logger::get().error("failed to find the specified shader texture resource to be destroyed");
            return;
        }

        // Destroy resource.
        mtxShaderTextureResources.second.erase(it);
    }

    std::pair<
        std::recursive_mutex,
        std::unordered_map<ShaderTextureResource*, std::unique_ptr<ShaderTextureResource>>>*
    ShaderTextureResourceManager::getResources() {
        return &mtxShaderTextureResources;
    }

    ShaderTextureResourceManager::ShaderTextureResourceManager(Renderer* pRenderer) {
        this->pRenderer = pRenderer;
    }

    ShaderTextureResourceManager::~ShaderTextureResourceManager() {
        std::scoped_lock guard(mtxShaderTextureResources.first);

        // Make sure there are no CPU write resources.
        if (!mtxShaderTextureResources.second.empty()) [[unlikely]] {
            // Prepare their names and count.
            std::unordered_map<std::string, size_t> leftResources;
            for (const auto& [pRawResource, pResource] : mtxShaderTextureResources.second) {
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
                "shader texture resource manager is being destroyed but there are still {} shader "
                "texture resource(s) alive:\n{}",
                mtxShaderTextureResources.second.size(),
                sLeftResources));
            error.showError();
            return; // don't throw in destructor, just quit
        }
    }
} // namespace ne
