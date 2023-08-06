#include "DirectXFrameResource.h"

// Custom.
#include "render/directx/DirectXRenderer.h"

namespace ne {

    std::optional<Error> DirectXFrameResource::initialize(Renderer* pRenderer) {
        // Convert renderer.
        const auto pDirectXRenderer = dynamic_cast<DirectXRenderer*>(pRenderer);
        if (pDirectXRenderer == nullptr) [[unlikely]] {
            return Error("expected a DirectX renderer");
        }

        // Create command allocator.
        auto hResult = pDirectXRenderer->getD3dDevice()->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(pCommandAllocator.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        return {};
    }

} // namespace ne
