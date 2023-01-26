#include "GpuResourceManager.h"

#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "render/directx/resources/DirectXResourceManager.h"
#endif

namespace ne {

    std::variant<std::unique_ptr<GpuResourceManager>, Error> GpuResourceManager::create(Renderer* pRenderer) {
#if defined(WIN32)
        if (auto pDirectXRenderer = dynamic_cast<DirectXRenderer*>(pRenderer)) {
            auto result = DirectXResourceManager::create(pDirectXRenderer);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addEntry();
                return error;
            }
            return std::get<std::unique_ptr<DirectXResourceManager>>(std::move(result));
        }
#elif __linux__
        // TODO: vulkan
        static_assert(false, "not implemented");
#endif

        return Error("not implemented");
    }

} // namespace ne
