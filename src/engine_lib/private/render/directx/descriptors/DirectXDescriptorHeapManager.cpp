#include "DirectXDescriptorHeapManager.h"

// Custom.
#include "render/directx/DirectXRenderer.h"
#include "io/Logger.h"
#include "render/directx/resources/DirectXResource.h"

namespace ne {
    std::variant<std::unique_ptr<DirectXDescriptorHeapManager>, Error>
    DirectXDescriptorHeapManager::create(DirectXRenderer* pRenderer, DescriptorHeapType heapType) {
        auto pManager = std::unique_ptr<DirectXDescriptorHeapManager>(
            new DirectXDescriptorHeapManager(pRenderer, heapType));

        auto optionalError = pManager->createHeap(iHeapGrowSize);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        return pManager;
    }

    std::optional<Error> DirectXDescriptorHeapManager::assignDescriptor(DirectXResource* pResource) {
        std::scoped_lock guard(mtxRwHeap);

        if (bindedResources.size() == static_cast<size_t>(iHeapCapacity)) {
            auto optionalError = expandHeap();
            if (optionalError.has_value()) {
                optionalError->addEntry();
                return optionalError.value();
            }
        }

        auto heapHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pHeap->GetCPUDescriptorHandleForHeapStart());

        INT iDescriptorIndex = 0;
        if (iNextFreeHeapIndex.load() == iHeapCapacity) {
            iDescriptorIndex = noLongerUsedDescriptorIndexes.front();
            noLongerUsedDescriptorIndexes.pop();
        } else {
            iDescriptorIndex = iNextFreeHeapIndex.fetch_add(1);
        }
        heapHandle.Offset(iDescriptorIndex, iDescriptorSize);

        createView(heapHandle, pResource);

        bindedResources.insert(pResource);
        pResource->heapDescriptor = DirectXDescriptor(this, pResource, iDescriptorIndex);

        return {};
    }

    INT DirectXDescriptorHeapManager::getHeapCapacity() {
        std::scoped_lock guard(mtxRwHeap);
        return iHeapCapacity;
    }

    INT DirectXDescriptorHeapManager::getHeapSize() {
        std::scoped_lock guard(mtxRwHeap);
        return static_cast<INT>(bindedResources.size());
    }

    std::string DirectXDescriptorHeapManager::convertHeapTypeToString(DescriptorHeapType heapType) {
        switch (heapType) {
        case (DescriptorHeapType::RTV):
            return "RTV";
        case (DescriptorHeapType::DSV):
            return "DSV";
        case (DescriptorHeapType::CBV_SRV_UAV):
            return "CBV/SRV/UAV";
        }

        const Error err("not handled heap type");
        err.showError();
        throw std::runtime_error(err.getError());
    }

    DirectXDescriptorHeapManager::DirectXDescriptorHeapManager(
        DirectXRenderer* pRenderer, DescriptorHeapType heapType) {
        this->pRenderer = pRenderer;
        this->heapType = heapType;

        // Get descriptor size and heap type.
        switch (heapType) {
        case (DescriptorHeapType::RTV):
            iDescriptorSize =
                pRenderer->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            break;
        case (DescriptorHeapType::DSV):
            iDescriptorSize =
                pRenderer->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            break;
        case (DescriptorHeapType::CBV_SRV_UAV):
            iDescriptorSize =
                pRenderer->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            break;
        }

        sHeapType = convertHeapTypeToString(heapType);
    }

    void DirectXDescriptorHeapManager::markDescriptorAsNoLongerBeingUsed(DirectXResource* pResource) {
        std::scoped_lock guard(mtxRwHeap);

        const auto it = bindedResources.find(pResource);
        if (it == bindedResources.end()) [[unlikely]] {
            Logger::get().error(
                std::format("the specified resource {} was not found", reinterpret_cast<void*>(pResource)),
                sDescriptorHeapLogCategory);
            return;
        }

        bindedResources.erase(it);
        noLongerUsedDescriptorIndexes.push(pResource->heapDescriptor->iDescriptorOffsetInDescriptors.value());

        if (iHeapCapacity >= iHeapGrowSize * 2 &&
            static_cast<INT>(bindedResources.size()) <= (iHeapCapacity - iHeapGrowSize - iHeapGrowSize / 2)) {
            auto optionalError = shrinkHeap();
            if (optionalError.has_value()) {
                optionalError->addEntry();
                Logger::get().error(optionalError->getError(), sDescriptorHeapLogCategory);
            }
        }
    }

    std::optional<Error> DirectXDescriptorHeapManager::expandHeap() {
        std::scoped_lock guard(mtxRwHeap);

        if (bindedResources.size() != static_cast<size_t>(iHeapCapacity)) [[unlikely]] {
            Logger::get().error(
                std::format(
                    "requested to expand {} heap of capacity {} while the actual size is {}",
                    sHeapType,
                    iHeapCapacity,
                    bindedResources.size()),
                sDescriptorHeapLogCategory);
        }

        if (!noLongerUsedDescriptorIndexes.empty()) [[unlikely]] {
            Logger::get().error(
                std::format(
                    "requested to expand {} heap of capacity {} while there are not used "
                    "descriptors exist ({}) (actual heap size is {})",
                    sHeapType,
                    iHeapCapacity,
                    noLongerUsedDescriptorIndexes.size(),
                    bindedResources.size()),
                sDescriptorHeapLogCategory);
        }

        if (static_cast<size_t>(iHeapCapacity) + static_cast<size_t>(iHeapGrowSize) >
            static_cast<size_t>(INT_MAX)) [[unlikely]] {
            return Error(std::format(
                "a request to expand {} descriptor heap (from capacity {} to {}) was rejected, reason: "
                "heap will exceed type limit of {}",
                sHeapType,
                iHeapCapacity,
                static_cast<size_t>(iHeapCapacity) + static_cast<size_t>(iHeapGrowSize),
                INT_MAX));
        }

        const auto iOldHeapSize = iHeapCapacity;

        auto optionalError = createHeap(iOldHeapSize + iHeapGrowSize);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        iNextFreeHeapIndex.store(iOldHeapSize);
        noLongerUsedDescriptorIndexes = {};

        return {};
    }

    std::optional<Error> DirectXDescriptorHeapManager::shrinkHeap() {
        std::scoped_lock guard(mtxRwHeap);

        if (iHeapCapacity < iHeapGrowSize * 2) [[unlikely]] {
            return Error(std::format(
                "a request to shrink {} heap of capacity {} with the actual size of {} was rejected, reason: "
                "expected at least size of {}",
                sHeapType,
                iHeapCapacity,
                bindedResources.size(),
                iHeapGrowSize * 2));
        }

        if (static_cast<INT>(bindedResources.size()) > iHeapCapacity - iHeapGrowSize - iHeapGrowSize / 2)
            [[unlikely]] {
            return Error(std::format(
                "a request to shrink {} heap of capacity {} with the actual size of {} was rejected, reason: "
                "shrink condition is not met (size {} < {} is false)",
                sHeapType,
                iHeapCapacity,
                bindedResources.size(),
                bindedResources.size(),
                iHeapCapacity - iHeapGrowSize - iHeapGrowSize / 2));
        }

        const auto iNewHeapSize = iHeapCapacity - iHeapGrowSize;

        auto optionalError = createHeap(iNewHeapSize);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        iNextFreeHeapIndex.store(iNewHeapSize - iHeapGrowSize / 2);
        noLongerUsedDescriptorIndexes = {};

        return {};
    }

    void DirectXDescriptorHeapManager::createView(
        CD3DX12_CPU_DESCRIPTOR_HANDLE heapHandle, const DirectXResource* pResource) const {
        switch (heapType) {
        case (DescriptorHeapType::RTV):
            pRenderer->pDevice->CreateRenderTargetView(pResource->getResource(), nullptr, heapHandle);
            break;
        case (DescriptorHeapType::DSV):
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
            dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
            dsvDesc.ViewDimension =
                pRenderer->bIsMsaaEnabled ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Format = pRenderer->depthStencilFormat;
            dsvDesc.Texture2D.MipSlice = 0;

            pRenderer->pDevice->CreateDepthStencilView(pResource->getResource(), &dsvDesc, heapHandle);
            break;
        case (DescriptorHeapType::CBV_SRV_UAV):
            const auto resourceGpuVirtualAddress = pResource->getResource()->GetGPUVirtualAddress();

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            cbvDesc.BufferLocation = resourceGpuVirtualAddress;
            cbvDesc.SizeInBytes = static_cast<UINT>(pResource->getResource()->GetDesc().Width);

            pRenderer->pDevice->CreateConstantBufferView(&cbvDesc, heapHandle);
            break;
        }
    }

    std::optional<Error> DirectXDescriptorHeapManager::createHeap(INT iCapacity) {
        Logger::get().info(
            std::format(
                "flushing the command queue to (re)create {} descriptor heap (from capacity {} to {})",
                sHeapType,
                iHeapCapacity,
                iCapacity),
            sDescriptorHeapLogCategory);

        // Make sure we don't render anything.
        std::scoped_lock drawGuard(pRenderer->mtxRwRenderResources);
        pRenderer->flushCommandQueue();

        // Create heap.
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
        heapDesc.NumDescriptors = iCapacity;
        heapDesc.Type = d3dHeapType;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heapDesc.NodeMask = 0;

        const HRESULT hResult =
            pRenderer->pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(pHeap.ReleaseAndGetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Save heap size.
        iHeapCapacity = iCapacity;

        recreateOldViews();

        return {};
    }

    void DirectXDescriptorHeapManager::recreateOldViews() const {
        // Start from 0 heap index, increment and update old offsets
        // to "shrink" heap usage (needed for heap shrinking).
        auto heapHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pHeap->GetCPUDescriptorHandleForHeapStart());
        INT iCurrentHeapIndex = 0;

        for (const auto& pResource : bindedResources) {
            createView(heapHandle, pResource);
            pResource->heapDescriptor->iDescriptorOffsetInDescriptors = iCurrentHeapIndex;

            heapHandle.Offset(1, iDescriptorSize);
            iCurrentHeapIndex += 1;
        }
    }
} // namespace ne
