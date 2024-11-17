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

        // Make sure the resource is created.
        if (pFrameConstantBuffer == nullptr) [[unlikely]] {
            return Error("expected the resource to be created at this point");
        }

        // Cast to DirectX resource.
        const auto pDirectXResource =
            dynamic_cast<DirectXResource*>(pFrameConstantBuffer->getInternalResource());
        if (pDirectXResource == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }

        // Bind CBV so that when the base class creates a global shader resource binding
        // the binding type will be determined as a constant buffer.
        auto optionalError = pDirectXResource->bindDescriptor(DirectXDescriptorType::CBV);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

} // namespace ne
