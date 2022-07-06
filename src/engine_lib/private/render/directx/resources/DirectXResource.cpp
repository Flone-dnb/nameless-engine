#include "DirectXResource.h"

// Custom.
#include "render/directx/descriptors/DirectXDescriptorHeapManager.h"

namespace ne {

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResource::create(
        DirectXDescriptorHeapManager* pHeap,
        D3D12MA::Allocator* pMemoryAllocator,
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState,
        std::optional<D3D12_CLEAR_VALUE> resourceClearValue) {
        auto pCreatedResource = std::unique_ptr<DirectXResource>(new DirectXResource());

        const D3D12_CLEAR_VALUE* pClearValue = nullptr;
        if (resourceClearValue.has_value()) {
            pClearValue = &resourceClearValue.value();
        }

        const HRESULT hResult = pMemoryAllocator->CreateResource(
            &allocationDesc,
            &resourceDesc,
            initialResourceState,
            pClearValue,
            pCreatedResource->pAllocatedResource.ReleaseAndGetAddressOf(),
            IID_NULL,
            nullptr);
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        auto optionalError = pHeap->assignDescriptor(pCreatedResource.get());
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        return pCreatedResource;
    }

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResource::makeResourceFromSwapChainBuffer(
        DirectXDescriptorHeapManager* pRtvHeap, const ComPtr<ID3D12Resource>& pSwapChainBuffer) {
        auto pCreatedResource = std::unique_ptr<DirectXResource>(new DirectXResource());

        pCreatedResource->pSwapChainBuffer = pSwapChainBuffer;

        auto optionalError = pRtvHeap->assignDescriptor(pCreatedResource.get());
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        return pCreatedResource;
    }

    ID3D12Resource* DirectXResource::getResource() const {
        if (pAllocatedResource) {
            return pAllocatedResource->GetResource();
        } else {
            return pSwapChainBuffer.Get();
        }
    }

    DirectXResource::DirectXResource() { vHeapDescriptors.resize(static_cast<int>(DescriptorHeapType::END)); }
} // namespace ne
