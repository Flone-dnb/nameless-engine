#include "GpuResourceManager.h"

#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "render/directx/resources/DirectXResourceManager.h"
#endif
#include "render/vulkan/VulkanRenderer.h"
#include "render/vulkan/resources/VulkanResourceManager.h"

namespace ne {

    std::variant<std::unique_ptr<GpuResourceManager>, Error> GpuResourceManager::create(Renderer* pRenderer) {
#if defined(WIN32)
        if (auto pDirectXRenderer = dynamic_cast<DirectXRenderer*>(pRenderer)) {
            // Create DirectX resource manager.
            auto result = DirectXResourceManager::create(pDirectXRenderer);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addEntry();
                return error;
            }

            return std::get<std::unique_ptr<DirectXResourceManager>>(std::move(result));
        }
#endif

        if (auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pRenderer)) {
            // Create Vulkan resource manager.
            auto result = VulkanResourceManager::create(pVulkanRenderer);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addEntry();
                return error;
            }

            return std::get<std::unique_ptr<VulkanResourceManager>>(std::move(result));
        }

        return Error("found not supported renderer");
    }

} // namespace ne
