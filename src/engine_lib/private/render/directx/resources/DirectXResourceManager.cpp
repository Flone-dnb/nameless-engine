#include "DirectXResourceManager.h"

// Custom.
#include "render/directx/resources/DirectXResource.h"
#include "render/directx/DirectXRenderer.h"
#include "render/general/resources/FrameResourcesManager.h"
#include "render/general/resources/UploadBuffer.h"

namespace ne {
    std::variant<std::unique_ptr<DirectXResourceManager>, Error>
    DirectXResourceManager::create(DirectXRenderer* pRenderer) {
        // Create resource allocator.
        D3D12MA::ALLOCATOR_DESC desc = {};
        desc.Flags = D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED;
        desc.pDevice = pRenderer->getD3dDevice();
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
            pRenderer,
            std::move(pMemoryAllocator),
            std::move(pRtvHeapManager),
            std::move(pDsvHeapManager),
            std::move(pCbvSrvUavHeapManager)));
    }

    std::variant<std::unique_ptr<UploadBuffer>, Error> DirectXResourceManager::createResourceWithCpuAccess(
        const std::string& sResourceName,
        size_t iElementSizeInBytes,
        size_t iElementCount,
        bool bIsShaderConstantBuffer) const {
        if (bIsShaderConstantBuffer) {
            // Constant buffers must be multiple of 256 (hardware requirement).
            iElementSizeInBytes = makeMultipleOf256(iElementSizeInBytes);
        }

        // Prepare resource description.
        CD3DX12_RESOURCE_DESC resourceDesc =
            CD3DX12_RESOURCE_DESC::Buffer(iElementSizeInBytes * iElementCount);
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

        // Create resource.
        auto result = DirectXResource::create(
            this,
            sResourceName,
            pMemoryAllocator.Get(),
            allocationDesc,
            resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            {});
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }
        auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        return std::unique_ptr<UploadBuffer>(
            new UploadBuffer(std::move(pResource), iElementSizeInBytes, iElementCount));
    }

    std::variant<std::unique_ptr<GpuResource>, Error> DirectXResourceManager::createResourceWithData(
        const std::string& sResourceName,
        const void* pBufferData,
        size_t iDataSizeInBytes,
        bool bAllowUnorderedAccess) const {
        // In order to create a GPU resource with our data from the CPU
        // we have to do a few steps:
        // 1. Create a GPU resource with DEFAULT heap type (CPU read-only heap) AKA resulting resource.
        // 2. Create a GPU resource with UPLOAD heap type (CPU read-write heap) AKA upload resource.
        // 3. Copy our data from the CPU to the resulting resource by using the upload resource.
        // 4. Wait for GPU to finish copying data and delete the upload resource.

        // 1. Create the resulting resource.
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(
            iDataSizeInBytes,
            bAllowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE);
        D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
        auto result = DirectXResource::create(
            this,
            sResourceName,
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
        auto pResultingResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        // 2. Create the upload resource.
        allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(iDataSizeInBytes);
        initialResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
        result = DirectXResource::create(
            this,
            std::format("upload resource for \"{}\"", sResourceName),
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
        auto pUploadResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        // Stop rendering.
        std::scoped_lock renderingGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Get command allocator from the current frame resource.
        const auto pFrameResourcesManager = pRenderer->getFrameResourcesManager();
        const auto mtxCurrentFrameResource = pFrameResourcesManager->getCurrentFrameResource();
        std::scoped_lock frameResourceGuard(*mtxCurrentFrameResource.first);

        const auto pCommandList = pRenderer->getD3dCommandList();
        const auto pCommandQueue = pRenderer->getD3dCommandQueue();
        const auto pCommandAllocator = mtxCurrentFrameResource.second->pCommandAllocator.Get();

        // Clear command list allocator (because it's not used by the GPU now).
        auto hResult = pCommandAllocator->Reset();
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Open command list (it was closed).
        hResult = pCommandList->Reset(pCommandAllocator, nullptr);
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // 3. Copy our data from the CPU to the resulting resource by using the upload resource.
        D3D12_SUBRESOURCE_DATA subResourceData = {};
        subResourceData.pData = pBufferData;
        subResourceData.RowPitch = iDataSizeInBytes;
        subResourceData.SlicePitch = subResourceData.RowPitch;
        // Queues a copy command using `ID3D12CommandList::CopySubresourceRegion`.
        UpdateSubresources<1>(
            pCommandList,
            pResultingResource->getInternalResource(),
            pUploadResource->getInternalResource(),
            0,
            0,
            1,
            &subResourceData);

        // Queue resulting resource state change.
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
            pResultingResource->getInternalResource(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            bAllowUnorderedAccess ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS
                                  : D3D12_RESOURCE_STATE_GENERIC_READ);
        pCommandList->ResourceBarrier(1, &transition);

        // Close command list.
        hResult = pCommandList->Close();
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Add the command list to the command queue for execution.
        ID3D12CommandList* commandLists[] = {pCommandList};
        pCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

        // 4. Wait for GPU to finish copying data and delete the upload resource.
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        return pResultingResource;
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
        // Create resource.
        auto result = DirectXResource::create(
            this,
            sResourceName,
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
        auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        // Assign descriptor.
        auto optionalError = pResource->bindRtv();
        if (optionalError.has_value()) {
            auto err = optionalError.value();
            err.addEntry();
            return err;
        }

        return pResource;
    }

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResourceManager::createDsvResource(
        const std::string& sResourceName,
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState,
        const D3D12_CLEAR_VALUE& resourceClearValue) const {
        // Create resource.
        auto result = DirectXResource::create(
            this,
            sResourceName,
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
        auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        // Assign descriptor.
        auto optionalError = pResource->bindDsv();
        if (optionalError.has_value()) {
            auto err = optionalError.value();
            err.addEntry();
            return err;
        }

        return pResource;
    }

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResourceManager::createCbvResource(
        const std::string& sResourceName,
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState) const {
        // Create resource.
        auto result = DirectXResource::create(
            this,
            sResourceName,
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
        auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        // Assign descriptor.
        auto optionalError = pResource->bindCbv();
        if (optionalError.has_value()) {
            auto err = optionalError.value();
            err.addEntry();
            return err;
        }

        return pResource;
    }

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResourceManager::createSrvResource(
        const std::string& sResourceName,
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState) const {
        // Create resource.
        auto result = DirectXResource::create(
            this,
            sResourceName,
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

        auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        // Assign descriptor.
        auto optionalError = pResource->bindSrv();
        if (optionalError.has_value()) {
            auto err = optionalError.value();
            err.addEntry();
            return err;
        }

        return pResource;
    }

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResourceManager::createUavResource(
        const std::string& sResourceName,
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState) const {
        // Create resource.
        auto result = DirectXResource::create(
            this,
            sResourceName,
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

        auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        // Assign descriptor.
        auto optionalError = pResource->bindUav();
        if (optionalError.has_value()) {
            auto err = optionalError.value();
            err.addEntry();
            return err;
        }

        return pResource;
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
        DirectXRenderer* pRenderer,
        ComPtr<D3D12MA::Allocator>&& pMemoryAllocator,
        std::unique_ptr<DirectXDescriptorHeap>&& pRtvHeap,
        std::unique_ptr<DirectXDescriptorHeap>&& pDsvHeap,
        std::unique_ptr<DirectXDescriptorHeap>&& pCbvSrvUavHeap) {
        this->pRenderer = pRenderer;
        this->pMemoryAllocator = std::move(pMemoryAllocator);
        this->pRtvHeap = std::move(pRtvHeap);
        this->pDsvHeap = std::move(pDsvHeap);
        this->pCbvSrvUavHeap = std::move(pCbvSrvUavHeap);
    }
} // namespace ne
