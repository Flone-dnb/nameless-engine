#include "DirectXResource.h"

// Custom.
#include "render/directx/descriptors/DirectXDescriptorHeap.h"
#include "render/directx/resources/DirectXResourceManager.h"
#include "misc/Globals.h"

namespace ne {

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResource::create(
        const DirectXResourceManager* pResourceManager,
        const std::string& sResourceName,
        D3D12MA::Allocator* pMemoryAllocator,
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState,
        std::optional<D3D12_CLEAR_VALUE> resourceClearValue) {
        auto pCreatedResource = std::unique_ptr<DirectXResource>(new DirectXResource(pResourceManager));

        const D3D12_CLEAR_VALUE* pClearValue = nullptr;
        if (resourceClearValue.has_value()) {
            pClearValue = &resourceClearValue.value();
        }

        // Allocate resource.
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

        // Assign resource name.
        pCreatedResource->pAllocatedResource->SetName(stringToWstring(sResourceName).c_str());

        return pCreatedResource;
    }

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResource::createResourceFromSwapChainBuffer(
        const DirectXResourceManager* pResourceManager,
        DirectXDescriptorHeap* pRtvHeap,
        const ComPtr<ID3D12Resource>& pSwapChainBuffer) {
        auto pCreatedResource = std::unique_ptr<DirectXResource>(new DirectXResource(pResourceManager));

        pCreatedResource->pSwapChainBuffer = pSwapChainBuffer;

        // Assign descriptor.
        auto optionalError = pRtvHeap->assignDescriptor(pCreatedResource.get(), DescriptorType::RTV);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        return pCreatedResource;
    }

    std::optional<Error> DirectXResource::bindRtv() {
        auto optionalError = pResourceManager->getRtvHeap()->assignDescriptor(this, DescriptorType::RTV);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> DirectXResource::bindDsv() {
        auto optionalError = pResourceManager->getDsvHeap()->assignDescriptor(this, DescriptorType::DSV);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> DirectXResource::bindCbv() {
        auto optionalError =
            pResourceManager->getCbvSrvUavHeap()->assignDescriptor(this, DescriptorType::CBV);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> DirectXResource::bindSrv() {
        auto optionalError =
            pResourceManager->getCbvSrvUavHeap()->assignDescriptor(this, DescriptorType::SRV);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> DirectXResource::bindUav() {
        auto optionalError =
            pResourceManager->getCbvSrvUavHeap()->assignDescriptor(this, DescriptorType::UAV);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError;
        }

        return {};
    }

    ID3D12Resource* DirectXResource::getInternalResource() const {
        if (pAllocatedResource != nullptr) {
            return pAllocatedResource->GetResource();
        }

        return pSwapChainBuffer.Get();
    }

    std::string DirectXResource::getResourceName() const {
        if (pAllocatedResource != nullptr) {
            return wstringToString(std::wstring(pAllocatedResource->GetName()));
        }

        return "swap chain buffer resource";
    }

    DirectXResource::DirectXResource(const DirectXResourceManager* pResourceManager) {
        this->pResourceManager = pResourceManager;
        vHeapDescriptors.resize(static_cast<int>(DescriptorType::END));
    }
} // namespace ne
