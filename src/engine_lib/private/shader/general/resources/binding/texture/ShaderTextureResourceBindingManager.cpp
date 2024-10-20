#include "ShaderTextureResourceBindingManager.h"

// Custom.
#include "io/Logger.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "shader/hlsl/resources/binding/texture/HlslShaderTextureResourceBinding.h"
#endif
#include "render/vulkan/VulkanRenderer.h"
#include "shader/glsl/resources/binding/texture/GlslShaderTextureResourceBinding.h"

namespace ne {

    std::variant<ShaderTextureResourceBindingUniquePtr, Error>
    ShaderTextureResourceBindingManager::createShaderTextureResource(
        const std::string& sShaderResourceName,
        const std::string& sResourceAdditionalInfo,
        const std::unordered_set<Pipeline*>& pipelinesToUse,
        std::unique_ptr<TextureHandle> pTextureToUse) {
        // Create new resource.
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            auto result = HlslShaderTextureResourceBinding::create(
                sShaderResourceName, pipelinesToUse, std::move(pTextureToUse));
            return handleResourceCreation(std::move(result));
        }
#endif
        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            auto result = GlslShaderTextureResourceBinding::create(
                sShaderResourceName, pipelinesToUse, std::move(pTextureToUse));
            return handleResourceCreation(std::move(result));
        }

        Error error("unexpected renderer");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

    std::variant<ShaderTextureResourceBindingUniquePtr, Error>
    ShaderTextureResourceBindingManager::handleResourceCreation(
        std::variant<std::unique_ptr<ShaderTextureResourceBinding>, Error> result) {
        // Check if there was an error.
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Get results.
        auto pResource = std::get<std::unique_ptr<ShaderTextureResourceBinding>>(std::move(result));
        auto pRawResource = pResource.get();

        // Add to be considered.
        std::scoped_lock guard(mtxShaderTextureResources.first);
        mtxShaderTextureResources.second[pRawResource] = std::move(pResource);

        return ShaderTextureResourceBindingUniquePtr(this, pRawResource);
    }

    void
    ShaderTextureResourceBindingManager::destroyResource(ShaderTextureResourceBinding* pResourceToDestroy) {
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
        std::unordered_map<ShaderTextureResourceBinding*, std::unique_ptr<ShaderTextureResourceBinding>>>*
    ShaderTextureResourceBindingManager::getResources() {
        return &mtxShaderTextureResources;
    }

    ShaderTextureResourceBindingManager::ShaderTextureResourceBindingManager(Renderer* pRenderer) {
        this->pRenderer = pRenderer;
    }

    ShaderTextureResourceBindingManager::~ShaderTextureResourceBindingManager() {
        std::scoped_lock guard(mtxShaderTextureResources.first);

        // Make sure there are no CPU write resources.
        if (!mtxShaderTextureResources.second.empty()) [[unlikely]] {
            // Prepare their names and count.
            std::unordered_map<std::string, size_t> leftResources;
            for (const auto& [pRawResource, pResource] : mtxShaderTextureResources.second) {
                const auto it = leftResources.find(pResource->getShaderResourceName());
                if (it == leftResources.end()) {
                    leftResources[pResource->getShaderResourceName()] = 1;
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
