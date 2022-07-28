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

    std::optional<Error> DirectXDescriptorHeapManager::assignDescriptor(
        DirectXResource* pResource, DescriptorType descriptorType) {
        // Check if this heap handles the specified descriptor type.
        const auto vHandledDescriptorTypes = getDescriptorTypesHandledByThisHeap();
        const auto descriptorIt = std::ranges::find_if(
            vHandledDescriptorTypes, [&descriptorType](const auto& type) { return descriptorType == type; });
        if (descriptorIt == vHandledDescriptorTypes.end()) [[unlikely]] {
            // this descriptor type is not handled in this heap
            return Error(std::format(
                "{} heap does not assign descriptors of the specified type (descriptor type {})",
                convertHeapTypeToString(heapType),
                static_cast<int>(descriptorType)));
        }

        // Check if the resource already has descriptor of this type.
        if (pResource->vHeapDescriptors[static_cast<int>(descriptorType)].has_value()) [[unlikely]] {
            return Error(std::format(
                "resource already has this descriptor assigned (descriptor type {})",
                static_cast<int>(descriptorType)));
        }

        std::scoped_lock guard(mtxRwHeap);

        if (iHeapSize.load() == iHeapCapacity.load()) {
            auto optionalError = expandHeap();
            if (optionalError.has_value()) {
                optionalError->addEntry();
                return optionalError.value();
            }
        }

        auto heapHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pHeap->GetCPUDescriptorHandleForHeapStart());

        INT iDescriptorIndex = 0;
        if (iNextFreeHeapIndex.load() == iHeapCapacity.load()) {
            iDescriptorIndex = noLongerUsedDescriptorIndexes.front();
            noLongerUsedDescriptorIndexes.pop();
        } else {
            iDescriptorIndex = iNextFreeHeapIndex.fetch_add(1);
        }
        heapHandle.Offset(iDescriptorIndex, iDescriptorSize);

        createView(heapHandle, pResource, descriptorType);

        bindedResources.insert(pResource);
        pResource->vHeapDescriptors[static_cast<int>(descriptorType)] =
            DirectXDescriptor(this, descriptorType, pResource, iDescriptorIndex);

        iHeapSize.fetch_add(1);

        return {};
    }

    INT DirectXDescriptorHeapManager::getHeapCapacity() {
        std::scoped_lock guard(mtxRwHeap);
        return iHeapCapacity.load();
    }

    INT DirectXDescriptorHeapManager::getHeapSize() {
        std::scoped_lock guard(mtxRwHeap);
        return iHeapSize.load();
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
        case (DescriptorHeapType::RTV): {
            iDescriptorSize =
                pRenderer->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        } break;
        case (DescriptorHeapType::DSV): {
            iDescriptorSize =
                pRenderer->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        } break;
        case (DescriptorHeapType::CBV_SRV_UAV): {
            iDescriptorSize =
                pRenderer->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        } break;
        default: {
            const Error err("invalid heap type");
            err.showError();
            throw std::runtime_error(err.getError());
        }
        }

        sHeapType = convertHeapTypeToString(heapType);
    }

    void DirectXDescriptorHeapManager::markDescriptorAsNoLongerBeingUsed(DirectXResource* pResource) {
        std::scoped_lock guard(mtxRwHeap);

        const auto it = bindedResources.find(pResource);
        if (it == bindedResources.end()) {
            Logger::get().error(
                std::format("the specified resource {} was not found", reinterpret_cast<void*>(pResource)),
                sDescriptorHeapLogCategory);
            return;
        }

        bindedResources.erase(it);

        // Save indexes of no longer used descriptors.
        const auto vHandledDescriptorTypes = getDescriptorTypesHandledByThisHeap();
        for (const auto& descriptor : pResource->vHeapDescriptors) {
            if (!descriptor.has_value())
                continue;

            const auto descriptorIt =
                std::ranges::find_if(vHandledDescriptorTypes, [&descriptor](const auto& descriptorType) {
                    return descriptor->descriptorType == descriptorType;
                });
            if (descriptorIt == vHandledDescriptorTypes.end())
                continue; // this descriptor type is not handled in this heap

            noLongerUsedDescriptorIndexes.push(descriptor->iDescriptorOffsetInDescriptors.value());
            iHeapSize.fetch_sub(1);
        }

        if (iHeapCapacity.load() >= iHeapGrowSize * 2 &&
            iHeapSize.load() <= (iHeapCapacity.load() - iHeapGrowSize - iHeapGrowSize / 2)) {
            auto optionalError = shrinkHeap();
            if (optionalError.has_value()) {
                optionalError->addEntry();
                Logger::get().error(optionalError->getError(), sDescriptorHeapLogCategory);
            }
        }
    }

    std::optional<Error> DirectXDescriptorHeapManager::expandHeap() {
        std::scoped_lock guard(mtxRwHeap);

        if (iHeapSize.load() != iHeapCapacity.load()) [[unlikely]] {
            Logger::get().error(
                std::format(
                    "requested to expand {} heap of capacity {} while the actual size is {}",
                    sHeapType,
                    iHeapCapacity.load(),
                    iHeapSize.load()),
                sDescriptorHeapLogCategory);
        }

        if (!noLongerUsedDescriptorIndexes.empty()) [[unlikely]] {
            Logger::get().error(
                std::format(
                    "requested to expand {} heap of capacity {} while there are not used "
                    "descriptors exist ({}) (actual heap size is {})",
                    sHeapType,
                    iHeapCapacity.load(),
                    noLongerUsedDescriptorIndexes.size(),
                    iHeapSize.load()),
                sDescriptorHeapLogCategory);
        }

        if (static_cast<size_t>(iHeapCapacity.load()) + static_cast<size_t>(iHeapGrowSize) >
            static_cast<size_t>(INT_MAX)) [[unlikely]] {
            return Error(std::format(
                "a request to expand {} descriptor heap (from capacity {} to {}) was rejected, reason: "
                "heap will exceed type limit of {}",
                sHeapType,
                iHeapCapacity.load(),
                static_cast<size_t>(iHeapCapacity.load()) + static_cast<size_t>(iHeapGrowSize),
                INT_MAX));
        }

        const auto iOldHeapSize = iHeapCapacity.load();

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

        if (iHeapCapacity.load() < iHeapGrowSize * 2) [[unlikely]] {
            return Error(std::format(
                "a request to shrink {} heap of capacity {} with the actual size of {} was rejected, reason: "
                "expected at least size of {}",
                sHeapType,
                iHeapCapacity.load(),
                iHeapSize.load(),
                iHeapGrowSize * 2));
        }

        if (iHeapSize.load() > iHeapCapacity.load() - iHeapGrowSize - iHeapGrowSize / 2) [[unlikely]] {
            return Error(std::format(
                "a request to shrink {} heap of capacity {} with the actual size of {} was rejected, reason: "
                "shrink condition is not met (size {} < {} is false)",
                sHeapType,
                iHeapCapacity.load(),
                iHeapSize.load(),
                iHeapSize.load(),
                iHeapCapacity.load() - iHeapGrowSize - iHeapGrowSize / 2));
        }

        const auto iNewHeapSize = iHeapCapacity.load() - iHeapGrowSize;

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
        CD3DX12_CPU_DESCRIPTOR_HANDLE heapHandle,
        const DirectXResource* pResource,
        DescriptorType descriptorType) const {
        switch (descriptorType) {
        case (DescriptorType::RTV): {
            pRenderer->pDevice->CreateRenderTargetView(pResource->getResource(), nullptr, heapHandle);
        } break;
        case (DescriptorType::DSV): {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
            dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
            dsvDesc.ViewDimension =
                pRenderer->bIsMsaaEnabled ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Format = pRenderer->depthStencilFormat;
            dsvDesc.Texture2D.MipSlice = 0;

            pRenderer->pDevice->CreateDepthStencilView(pResource->getResource(), &dsvDesc, heapHandle);
        } break;
        case (DescriptorType::CBV): {
            const auto resourceGpuVirtualAddress = pResource->getResource()->GetGPUVirtualAddress();

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            cbvDesc.BufferLocation = resourceGpuVirtualAddress;
            cbvDesc.SizeInBytes = static_cast<UINT>(pResource->getResource()->GetDesc().Width);

            pRenderer->pDevice->CreateConstantBufferView(&cbvDesc, heapHandle);
        } break;
        case (DescriptorType::SRV): {
            const auto resourceDesc = pResource->getResource()->GetDesc();

            // Determine SRV dimension.
            D3D12_SRV_DIMENSION srvDimension = D3D12_SRV_DIMENSION_UNKNOWN;
            switch (resourceDesc.Dimension) {
            case (D3D12_RESOURCE_DIMENSION_UNKNOWN):
                srvDimension = D3D12_SRV_DIMENSION_UNKNOWN;
                break;
            case (D3D12_RESOURCE_DIMENSION_BUFFER):
                srvDimension = D3D12_SRV_DIMENSION_BUFFER;
                break;
            case (D3D12_RESOURCE_DIMENSION_TEXTURE1D):
                srvDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
                break;
            case (D3D12_RESOURCE_DIMENSION_TEXTURE2D):
                srvDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                break;
            case (D3D12_RESOURCE_DIMENSION_TEXTURE3D):
                srvDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                break;
            }

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = resourceDesc.Format;
            srvDesc.ViewDimension = srvDimension;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;

            pRenderer->pDevice->CreateShaderResourceView(pResource->getResource(), &srvDesc, heapHandle);
        } break;
        case (DescriptorType::UAV): {
            const auto resourceDesc = pResource->getResource()->GetDesc();

            // Determine UAV dimension.
            D3D12_UAV_DIMENSION uavDimension = D3D12_UAV_DIMENSION_UNKNOWN;
            switch (resourceDesc.Dimension) {
            case (D3D12_RESOURCE_DIMENSION_UNKNOWN):
                uavDimension = D3D12_UAV_DIMENSION_UNKNOWN;
                break;
            case (D3D12_RESOURCE_DIMENSION_BUFFER):
                uavDimension = D3D12_UAV_DIMENSION_BUFFER;
                break;
            case (D3D12_RESOURCE_DIMENSION_TEXTURE1D):
                uavDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
                break;
            case (D3D12_RESOURCE_DIMENSION_TEXTURE2D):
                uavDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                break;
            case (D3D12_RESOURCE_DIMENSION_TEXTURE3D):
                uavDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
                break;
            }

            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = resourceDesc.Format;
            uavDesc.ViewDimension = uavDimension;
            uavDesc.Texture2D.MipSlice = 0;

            pRenderer->pDevice->CreateUnorderedAccessView(
                pResource->getResource(), nullptr, &uavDesc, heapHandle);
        } break;
        case (DescriptorType::END): {
            const Error err("invalid heap type");
            err.showError();
            throw std::runtime_error(err.getError());
        }
        }
    }

    std::optional<Error> DirectXDescriptorHeapManager::createHeap(INT iCapacity) {
        Logger::get().info(
            std::format(
                "flushing the command queue to (re)create {} descriptor heap (from capacity {} to {}) "
                "(size: {})",
                sHeapType,
                iHeapCapacity.load(),
                iCapacity,
                iHeapSize.load()),
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
        iHeapCapacity.store(iCapacity);

        recreateOldViews();

        return {};
    }

    std::vector<DescriptorType> DirectXDescriptorHeapManager::getDescriptorTypesHandledByThisHeap() const {
        switch (heapType) {
        case (DescriptorHeapType::RTV):
            return {DescriptorType::RTV};
        case (DescriptorHeapType::DSV):
            return {DescriptorType::DSV};
        case (DescriptorHeapType::CBV_SRV_UAV):
            return {DescriptorType::CBV, DescriptorType::SRV, DescriptorType::UAV};
        }

        const Error err("not handled heap type");
        err.showError();
        throw std::runtime_error(err.getError());
    }

    void DirectXDescriptorHeapManager::recreateOldViews() const {
        // Start from 0 heap index, increment and update old offsets
        // to "shrink" heap usage (needed for heap shrinking).
        auto heapHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pHeap->GetCPUDescriptorHandleForHeapStart());
        INT iCurrentHeapIndex = 0;
        const auto vHandledDescriptorTypes = getDescriptorTypesHandledByThisHeap();

        for (const auto& pResource : bindedResources) {
            for (const auto& descriptor : pResource->vHeapDescriptors) {
                if (!descriptor.has_value())
                    continue;

                const auto descriptorIt =
                    std::ranges::find_if(vHandledDescriptorTypes, [&descriptor](const auto& descriptorType) {
                        return descriptor->descriptorType == descriptorType;
                    });
                if (descriptorIt == vHandledDescriptorTypes.end())
                    break; // this descriptor type is not handled in this heap

                createView(heapHandle, pResource, descriptor->descriptorType);
                pResource->vHeapDescriptors[static_cast<int>(descriptor->descriptorType)]
                    ->iDescriptorOffsetInDescriptors = iCurrentHeapIndex;

                heapHandle.Offset(1, iDescriptorSize);
                iCurrentHeapIndex += 1;
            }
        }
    }
} // namespace ne
