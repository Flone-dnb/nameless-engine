#include "DirectXResourceManager.h"

// Custom.
#include "render/directx/descriptors/DirectXDescriptorHeapManager.h"
#include "render/directx/resources/DirectXResource.h"

namespace ne {
    std::variant<std::unique_ptr<DirectXResourceManager>, Error> DirectXResourceManager::create(
        DirectXRenderer* pRenderer, ID3D12Device* pDevice, IDXGIAdapter3* pVideoAdapter) {
        // Create resource allocator.
        D3D12MA::ALLOCATOR_DESC desc = {};
        desc.Flags = D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED;
        desc.pDevice = pDevice;
        desc.pAdapter = pVideoAdapter;

        ComPtr<D3D12MA::Allocator> pMemoryAllocator;

        const HRESULT hResult = D3D12MA::CreateAllocator(&desc, pMemoryAllocator.ReleaseAndGetAddressOf());
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Create RTV heap manager.
        auto heapManagerResult = DirectXDescriptorHeapManager::create(pRenderer, DescriptorHeapType::RTV);
        if (std::holds_alternative<Error>(heapManagerResult)) {
            Error err = std::get<Error>(std::move(heapManagerResult));
            err.addEntry();
            return err;
        }
        auto pRtvHeapManager =
            std::get<std::unique_ptr<DirectXDescriptorHeapManager>>(std::move(heapManagerResult));

        // Create DSV heap manager.
        heapManagerResult = DirectXDescriptorHeapManager::create(pRenderer, DescriptorHeapType::DSV);
        if (std::holds_alternative<Error>(heapManagerResult)) {
            Error err = std::get<Error>(std::move(heapManagerResult));
            err.addEntry();
            return err;
        }
        auto pDsvHeapManager =
            std::get<std::unique_ptr<DirectXDescriptorHeapManager>>(std::move(heapManagerResult));

        // Create CBV/SRV/UAV heap manager.
        heapManagerResult = DirectXDescriptorHeapManager::create(pRenderer, DescriptorHeapType::CBV_SRV_UAV);
        if (std::holds_alternative<Error>(heapManagerResult)) {
            Error err = std::get<Error>(std::move(heapManagerResult));
            err.addEntry();
            return err;
        }
        auto pCbvSrvUavHeapManager =
            std::get<std::unique_ptr<DirectXDescriptorHeapManager>>(std::move(heapManagerResult));

        return std::unique_ptr<DirectXResourceManager>(new DirectXResourceManager(
            std::move(pMemoryAllocator),
            std::move(pRtvHeapManager),
            std::move(pDsvHeapManager),
            std::move(pCbvSrvUavHeapManager)));
    }

    size_t DirectXResourceManager::getTotalVideoMemoryInMb() const {
        D3D12MA::Budget localBudget{};
        pMemoryAllocator->GetBudget(&localBudget, nullptr);

        return localBudget.BudgetBytes / 1024 / 1024;
    }

    size_t DirectXResourceManager::getUsedVideoMemoryInMb() const {
        D3D12MA::Budget localBudget{};
        pMemoryAllocator->GetBudget(&localBudget, nullptr);

        return localBudget.UsageBytes / 1024 / 1024;
    }

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResourceManager::createRtvResource(
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState,
        const D3D12_CLEAR_VALUE& resourceClearValue) const {
        auto result = DirectXResource::create(
            this,
            pRtvHeapManager.get(),
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
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState,
        const D3D12_CLEAR_VALUE& resourceClearValue) const {
        auto result = DirectXResource::create(
            this,
            pDsvHeapManager.get(),
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
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState) const {
        auto result = DirectXResource::create(
            this,
            pCbvSrvUavHeapManager.get(),
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
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState) const {
        auto result = DirectXResource::create(
            this,
            pCbvSrvUavHeapManager.get(),
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
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState) const {
        auto result = DirectXResource::create(
            this,
            pCbvSrvUavHeapManager.get(),
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

            auto result =
                DirectXResource::createResourceFromSwapChainBuffer(this, pRtvHeapManager.get(), pBuffer);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addEntry();
                return err;
            }

            vCreatedResources[i] = std::get<std::unique_ptr<DirectXResource>>(std::move(result));
        }

        return vCreatedResources;
    }

    DirectXDescriptorHeapManager* DirectXResourceManager::getRtvHeap() const { return pRtvHeapManager.get(); }

    DirectXDescriptorHeapManager* DirectXResourceManager::getDsvHeap() const { return pDsvHeapManager.get(); }

    DirectXDescriptorHeapManager* DirectXResourceManager::getCbvSrvUavHeap() const {
        return pCbvSrvUavHeapManager.get();
    }

    DirectXResourceManager::DirectXResourceManager(
        ComPtr<D3D12MA::Allocator>&& pMemoryAllocator,
        std::unique_ptr<DirectXDescriptorHeapManager>&& pRtvHeapManager,
        std::unique_ptr<DirectXDescriptorHeapManager>&& pDsvHeapManager,
        std::unique_ptr<DirectXDescriptorHeapManager>&& pCbvSrvUavHeapManager) {
        this->pMemoryAllocator = std::move(pMemoryAllocator);
        this->pRtvHeapManager = std::move(pRtvHeapManager);
        this->pDsvHeapManager = std::move(pDsvHeapManager);
        this->pCbvSrvUavHeapManager = std::move(pCbvSrvUavHeapManager);
    }
} // namespace ne
