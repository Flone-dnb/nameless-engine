#include "DirectXResource.h"

// Custom.
#include "render/directx/descriptors/DirectXDescriptorHeap.h"
#include "render/directx/resources/DirectXResourceManager.h"
#include "misc/Globals.h"
#include "render/directx/DirectXRenderer.h"

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

        // Save pointer to internal resource.
        pCreatedResource->pInternalResource = pCreatedResource->pAllocatedResource->GetResource();

        // Assign resource name.
        pCreatedResource->pAllocatedResource->SetName(Globals::stringToWstring(sResourceName).c_str());

        return pCreatedResource;
    }

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResource::createResourceFromSwapChainBuffer(
        const DirectXResourceManager* pResourceManager,
        DirectXDescriptorHeap* pRtvHeap,
        const ComPtr<ID3D12Resource>& pSwapChainBuffer) {
        auto pCreatedResource = std::unique_ptr<DirectXResource>(new DirectXResource(pResourceManager));

        pCreatedResource->pSwapChainBuffer = pSwapChainBuffer;

        // Save pointer to internal resource.
        pCreatedResource->pInternalResource = pCreatedResource->pSwapChainBuffer.Get();

        // Assign descriptor.
        auto optionalError = pRtvHeap->assignDescriptor(pCreatedResource.get(), DirectXDescriptorType::RTV);
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return pCreatedResource;
    }

    std::optional<D3D12_CPU_DESCRIPTOR_HANDLE>
    DirectXResource::getBindedDescriptorHandle(DirectXDescriptorType descriptorType) {
        std::scoped_lock guard(mtxHeapDescriptors.first);

        // Get descriptor.
        const auto pOptionalDescriptor = &mtxHeapDescriptors.second[static_cast<size_t>(descriptorType)];

        // Make sure it was binded previously.
        if (!pOptionalDescriptor->has_value()) {
            return {};
        }

        // Get descriptor offset.
        auto optionalOffset = pOptionalDescriptor->value().getDescriptorOffsetInDescriptors();
        if (!optionalOffset.has_value()) {
            return {};
        }

        // Get heap that this descriptor uses.
        const auto pHeap = pOptionalDescriptor->value().getDescriptorHeap();

        // Construct descriptor handle.
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            pHeap->getInternalHeap()->GetCPUDescriptorHandleForHeapStart(),
            optionalOffset.value(),
            pHeap->getDescriptorSize());
    }

    DirectXDescriptor* DirectXResource::getDescriptor(DirectXDescriptorType descriptorType) {
        std::scoped_lock guard(mtxHeapDescriptors.first);

        // Get descriptor.
        const auto pOptionalDescriptor = &mtxHeapDescriptors.second[static_cast<size_t>(descriptorType)];

        // Make sure it was binded previously.
        if (!pOptionalDescriptor->has_value()) {
            return nullptr;
        }

        return &pOptionalDescriptor->value();
    }

    std::string DirectXResource::getResourceName() const {
        if (pAllocatedResource != nullptr) {
            return Globals::wstringToString(std::wstring(pAllocatedResource->GetName()));
        }

        return "swap chain buffer resource";
    }

    DirectXResource::DirectXResource(const DirectXResourceManager* pResourceManager) {
        this->pResourceManager = pResourceManager;
        mtxHeapDescriptors.second.resize(static_cast<int>(DirectXDescriptorType::END));
    }

    DirectXResource::~DirectXResource() {
        // Don't log here to avoid spamming.

        // Make sure the GPU is not using this resource.
        pResourceManager->getRenderer()->waitForGpuToFinishWorkUpToThisPoint();
    }

    std::optional<Error> DirectXResource::bindDescriptor(DirectXDescriptorType descriptorType) {
        std::scoped_lock guard(mtxHeapDescriptors.first);

        // Check if we already have descriptor of this type binded.
        if (mtxHeapDescriptors.second[static_cast<int>(descriptorType)].has_value()) {
            // Nothing to do.
            return {};
        }

        DirectXDescriptorHeap* pHeap = nullptr;

        switch (descriptorType) {
        case (DirectXDescriptorType::CBV): {
            pHeap = pResourceManager->getCbvSrvUavHeap();
            break;
        }
        case (DirectXDescriptorType::SRV): {
            pHeap = pResourceManager->getCbvSrvUavHeap();
            break;
        }
        case (DirectXDescriptorType::UAV): {
            pHeap = pResourceManager->getCbvSrvUavHeap();
            break;
        }
        case (DirectXDescriptorType::RTV): {
            pHeap = pResourceManager->getRtvHeap();
            break;
        }
        case (DirectXDescriptorType::DSV): {
            pHeap = pResourceManager->getDsvHeap();
            break;
        }
        default:
            Error error("unexpected case");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Assign descriptor.
        auto optionalError = pHeap->assignDescriptor(this, descriptorType);
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }
} // namespace ne
