#include "DirectXResourceManager.h"

// Custom.
#include "render/directx/descriptors/DirectXDescriptorHeap.h"
#include "render/directx/resources/DirectXResource.h"
#include "render/directx/DirectXRenderer.h"
#include "render/general/resources/UploadBuffer.h"

namespace ne {
    std::variant<std::unique_ptr<DirectXResourceManager>, Error>
    DirectXResourceManager::create(DirectXRenderer* pRenderer) {
        // Create resource allocator.
        D3D12MA::ALLOCATOR_DESC desc = {};
        desc.Flags = D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED;
        desc.pDevice = pRenderer->getDevice();
        desc.pAdapter = pRenderer->getVideoAdapter();

        ComPtr<D3D12MA::Allocator> pMemoryAllocator;

        const HRESULT hResult = D3D12MA::CreateAllocator(&desc, pMemoryAllocator.ReleaseAndGetAddressOf());
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Create RTV heap manager.
        auto heapManagerResult = DirectXDescriptorHeap::create(pRenderer, DescriptorHeapType::RTV);
        if (std::holds_alternative<Error>(heapManagerResult)) {
            Error err = std::get<Error>(std::move(heapManagerResult));
            err.addEntry();
            return err;
        }
        auto pRtvHeapManager = std::get<std::unique_ptr<DirectXDescriptorHeap>>(std::move(heapManagerResult));

        // Create DSV heap manager.
        heapManagerResult = DirectXDescriptorHeap::create(pRenderer, DescriptorHeapType::DSV);
        if (std::holds_alternative<Error>(heapManagerResult)) {
            Error err = std::get<Error>(std::move(heapManagerResult));
            err.addEntry();
            return err;
        }
        auto pDsvHeapManager = std::get<std::unique_ptr<DirectXDescriptorHeap>>(std::move(heapManagerResult));

        // Create CBV/SRV/UAV heap manager.
        heapManagerResult = DirectXDescriptorHeap::create(pRenderer, DescriptorHeapType::CBV_SRV_UAV);
        if (std::holds_alternative<Error>(heapManagerResult)) {
            Error err = std::get<Error>(std::move(heapManagerResult));
            err.addEntry();
            return err;
        }
        auto pCbvSrvUavHeapManager =
            std::get<std::unique_ptr<DirectXDescriptorHeap>>(std::move(heapManagerResult));

        return std::unique_ptr<DirectXResourceManager>(new DirectXResourceManager(
            std::move(pMemoryAllocator),
            std::move(pRtvHeapManager),
            std::move(pDsvHeapManager),
            std::move(pCbvSrvUavHeapManager)));
    }

    std::variant<std::unique_ptr<UploadBuffer>, Error> DirectXResourceManager::createCbvResourceWithCpuAccess(
        const std::string& sResourceName, size_t iElementSizeInBytes, size_t iElementCount) const {
        // Constant buffers must be multiple of 256.
        iElementSizeInBytes = makeMultipleOf256(iElementSizeInBytes);

        // Prepare resource description.
        CD3DX12_RESOURCE_DESC resourceDesc =
            CD3DX12_RESOURCE_DESC::Buffer(iElementSizeInBytes * iElementCount);
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

        // Create resource.
        auto result =
            createCbvResource(sResourceName, allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }

        return std::unique_ptr<UploadBuffer>(new UploadBuffer(
            std::get<std::unique_ptr<DirectXResource>>(std::move(result)),
            iElementSizeInBytes,
            iElementCount));
    }

    size_t DirectXResourceManager::getTotalVideoMemoryInMb() const {
        D3D12MA::Budget localBudget{};
        pMemoryAllocator->GetBudget(&localBudget, nullptr);

        return localBudget.BudgetBytes / 1024 / 1024; // NOLINT
    }

    size_t DirectXResourceManager::getUsedVideoMemoryInMb() const {
        D3D12MA::Budget localBudget{};
        pMemoryAllocator->GetBudget(&localBudget, nullptr);

        return localBudget.UsageBytes / 1024 / 1024; // NOLINT
    }

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResourceManager::createRtvResource(
        const std::string& sResourceName,
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState,
        const D3D12_CLEAR_VALUE& resourceClearValue) const {
        auto result = DirectXResource::create(
            this,
            sResourceName,
            pRtvHeap.get(),
            DescriptorType::RTV,
            pMemoryAllocator.Get(),
            allocationDesc,
            resourceDesc,
            initialResourceState,
            resourceClearValue);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }

        return result;
    }

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResourceManager::createDsvResource(
        const std::string& sResourceName,
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState,
        const D3D12_CLEAR_VALUE& resourceClearValue) const {
        auto result = DirectXResource::create(
            this,
            sResourceName,
            pDsvHeap.get(),
            DescriptorType::DSV,
            pMemoryAllocator.Get(),
            allocationDesc,
            resourceDesc,
            initialResourceState,
            resourceClearValue);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }

        return result;
    }

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResourceManager::createCbvResource(
        const std::string& sResourceName,
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState) const {
        auto result = DirectXResource::create(
            this,
            sResourceName,
            pCbvSrvUavHeap.get(),
            DescriptorType::CBV,
            pMemoryAllocator.Get(),
            allocationDesc,
            resourceDesc,
            initialResourceState,
            {});
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }

        return result;
    }

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResourceManager::createSrvResource(
        const std::string& sResourceName,
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState) const {
        auto result = DirectXResource::create(
            this,
            sResourceName,
            pCbvSrvUavHeap.get(),
            DescriptorType::SRV,
            pMemoryAllocator.Get(),
            allocationDesc,
            resourceDesc,
            initialResourceState,
            {});
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }

        return result;
    }

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResourceManager::createUavResource(
        const std::string& sResourceName,
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState) const {
        auto result = DirectXResource::create(
            this,
            sResourceName,
            pCbvSrvUavHeap.get(),
            DescriptorType::UAV,
            pMemoryAllocator.Get(),
            allocationDesc,
            resourceDesc,
            initialResourceState,
            {});
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }

        return result;
    }

    std::variant<std::vector<std::unique_ptr<DirectXResource>>, Error>
    DirectXResourceManager::makeRtvResourcesFromSwapChainBuffer(
        IDXGISwapChain3* pSwapChain, unsigned int iSwapChainBufferCount) const {
        std::vector<std::unique_ptr<DirectXResource>> vCreatedResources(iSwapChainBufferCount);
        for (unsigned int i = 0; i < iSwapChainBufferCount; i++) {
            ComPtr<ID3D12Resource> pBuffer;
            const HRESULT hResult = pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBuffer));
            if (FAILED(hResult)) {
                return Error(hResult);
            }

            auto result = DirectXResource::createResourceFromSwapChainBuffer(this, pRtvHeap.get(), pBuffer);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addEntry();
                return err;
            }

            vCreatedResources[i] = std::get<std::unique_ptr<DirectXResource>>(std::move(result));
        }

        return vCreatedResources;
    }

    DirectXDescriptorHeap* DirectXResourceManager::getRtvHeap() const { return pRtvHeap.get(); }

    DirectXDescriptorHeap* DirectXResourceManager::getDsvHeap() const { return pDsvHeap.get(); }

    DirectXDescriptorHeap* DirectXResourceManager::getCbvSrvUavHeap() const { return pCbvSrvUavHeap.get(); }

    DirectXResourceManager::DirectXResourceManager(
        ComPtr<D3D12MA::Allocator>&& pMemoryAllocator,
        std::unique_ptr<DirectXDescriptorHeap>&& pRtvHeap,
        std::unique_ptr<DirectXDescriptorHeap>&& pDsvHeap,
        std::unique_ptr<DirectXDescriptorHeap>&& pCbvSrvUavHeap) {
        this->pMemoryAllocator = std::move(pMemoryAllocator);
        this->pRtvHeap = std::move(pRtvHeap);
        this->pDsvHeap = std::move(pDsvHeap);
        this->pCbvSrvUavHeap = std::move(pCbvSrvUavHeap);
    }
} // namespace ne
