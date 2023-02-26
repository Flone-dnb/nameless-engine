#include "DirectXDescriptorHeap.h"

// Custom.
#include "render/directx/DirectXRenderer.h"
#include "io/Logger.h"
#include "render/directx/resources/DirectXResource.h"
#include "render/RenderSettings.h"

namespace ne {
    std::variant<std::unique_ptr<DirectXDescriptorHeap>, Error>
    DirectXDescriptorHeap::create(DirectXRenderer* pRenderer, DescriptorHeapType heapType) {
        auto pManager =
            std::unique_ptr<DirectXDescriptorHeap>(new DirectXDescriptorHeap(pRenderer, heapType));

        auto optionalError = pManager->createHeap(iHeapGrowSize);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        return pManager;
    }

    std::optional<Error> DirectXDescriptorHeap::assignDescriptor(
        DirectXResource* pResource, GpuResource::DescriptorType descriptorType) {
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

        // Add a new descriptor to the heap.

        std::scoped_lock guard(mtxInternalResources.first);

        // Expand the heap if needed.
        if (mtxInternalResources.second.iHeapSize == mtxInternalResources.second.iHeapCapacity) {
            auto optionalError = expandHeap();
            if (optionalError.has_value()) {
                optionalError->addEntry();
                return optionalError.value();
            }
        }

        // Prepare to create a new view.
        auto heapHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            mtxInternalResources.second.pHeap->GetCPUDescriptorHandleForHeapStart());

        // Get free index.
        INT iDescriptorIndex = 0;
        if (mtxInternalResources.second.iNextFreeHeapIndex == mtxInternalResources.second.iHeapCapacity) {
            iDescriptorIndex = mtxInternalResources.second.noLongerUsedDescriptorIndexes.front();
            mtxInternalResources.second.noLongerUsedDescriptorIndexes.pop();
        } else {
            iDescriptorIndex = mtxInternalResources.second.iNextFreeHeapIndex;
            mtxInternalResources.second.iNextFreeHeapIndex += 1;
        }
        heapHandle.Offset(iDescriptorIndex, iDescriptorSize);

        createView(heapHandle, pResource, descriptorType);

        // Save binding information.
        mtxInternalResources.second.bindedResources.insert(pResource);
        pResource->vHeapDescriptors[static_cast<int>(descriptorType)] =
            DirectXDescriptor(this, descriptorType, pResource, iDescriptorIndex);

        // Mark increased heap size.
        mtxInternalResources.second.iHeapSize += 1;

        return {};
    }

    INT DirectXDescriptorHeap::getHeapCapacity() {
        std::scoped_lock guard(mtxInternalResources.first);
        return mtxInternalResources.second.iHeapCapacity;
    }

    INT DirectXDescriptorHeap::getHeapSize() {
        std::scoped_lock guard(mtxInternalResources.first);
        return mtxInternalResources.second.iHeapSize;
    }

    size_t DirectXDescriptorHeap::getNoLongerUsedDescriptorCount() {
        std::scoped_lock guard(mtxInternalResources.first);
        return mtxInternalResources.second.noLongerUsedDescriptorIndexes.size();
    }

    std::string DirectXDescriptorHeap::convertHeapTypeToString(DescriptorHeapType heapType) {
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
        throw std::runtime_error(err.getFullErrorMessage());
    }

    DirectXDescriptorHeap::DirectXDescriptorHeap(DirectXRenderer* pRenderer, DescriptorHeapType heapType) {
        static_assert(iHeapGrowSize % 2 == 0, "grow size must be even because we use INT / INT");

        this->pRenderer = pRenderer;
        this->heapType = heapType;

        // Get descriptor size and heap type.
        switch (heapType) {
        case (DescriptorHeapType::RTV): {
            iDescriptorSize =
                pRenderer->getD3dDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        } break;
        case (DescriptorHeapType::DSV): {
            iDescriptorSize =
                pRenderer->getD3dDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        } break;
        case (DescriptorHeapType::CBV_SRV_UAV): {
            iDescriptorSize = pRenderer->getD3dDevice()->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            d3dHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        } break;
        default: {
            const Error err("invalid heap type");
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }
        }

        sHeapType = convertHeapTypeToString(heapType);
    }

    void DirectXDescriptorHeap::markDescriptorAsNoLongerBeingUsed(DirectXResource* pResource) {
        std::scoped_lock guard(mtxInternalResources.first);

        const auto it = mtxInternalResources.second.bindedResources.find(pResource);
        if (it == mtxInternalResources.second.bindedResources.end()) {
            Logger::get().error(
                fmt::format("the specified resource \"{}\" is not found", pResource->getResourceName()),
                sDescriptorHeapLogCategory);
            return;
        }

        mtxInternalResources.second.bindedResources.erase(it);

        // Save indexes of no longer used descriptors.
        const auto vHandledDescriptorTypes = getDescriptorTypesHandledByThisHeap();
        for (auto& descriptor : pResource->vHeapDescriptors) {
            if (!descriptor.has_value()) {
                continue;
            }

            const auto descriptorIt =
                std::ranges::find_if(vHandledDescriptorTypes, [&descriptor](const auto& descriptorType) {
                    return descriptor->descriptorType == descriptorType;
                });
            if (descriptorIt == vHandledDescriptorTypes.end()) {
                continue; // this descriptor type is not handled in this heap
            }

            mtxInternalResources.second.noLongerUsedDescriptorIndexes.push(
                descriptor->iDescriptorOffsetInDescriptors.value());
            descriptor->iDescriptorOffsetInDescriptors = {}; // mark descriptor as cleared
            mtxInternalResources.second.iHeapSize -= 1;
        }

        // Shrink the heap if needed.
        if (mtxInternalResources.second.iHeapCapacity >= iHeapGrowSize * 2 &&
            mtxInternalResources.second.iHeapSize <=
                (mtxInternalResources.second.iHeapCapacity - iHeapGrowSize - iHeapGrowSize / 2)) {
            auto optionalError = shrinkHeap();
            if (optionalError.has_value()) {
                optionalError->addEntry();
                Logger::get().error(optionalError->getFullErrorMessage(), sDescriptorHeapLogCategory);
            }
        }
    }

    std::optional<Error> DirectXDescriptorHeap::expandHeap() {
        std::scoped_lock guard(mtxInternalResources.first);

        if (mtxInternalResources.second.iHeapSize != mtxInternalResources.second.iHeapCapacity) [[unlikely]] {
            Logger::get().error(
                std::format(
                    "requested to expand {} heap of capacity {} while the actual size is {}",
                    sHeapType,
                    mtxInternalResources.second.iHeapCapacity,
                    mtxInternalResources.second.iHeapSize),
                sDescriptorHeapLogCategory);
        }

        if (!mtxInternalResources.second.noLongerUsedDescriptorIndexes.empty()) [[unlikely]] {
            Logger::get().error(
                std::format(
                    "requested to expand {} heap of capacity {} while there are not used "
                    "descriptors exist ({}) (actual heap size is {})",
                    sHeapType,
                    mtxInternalResources.second.iHeapCapacity,
                    mtxInternalResources.second.noLongerUsedDescriptorIndexes.size(),
                    mtxInternalResources.second.iHeapSize),
                sDescriptorHeapLogCategory);
        }

        if (static_cast<size_t>(mtxInternalResources.second.iHeapCapacity) +
                static_cast<size_t>(iHeapGrowSize) >
            static_cast<size_t>(INT_MAX)) [[unlikely]] {
            return Error(std::format(
                "a request to expand {} descriptor heap (from capacity {} to {}) was rejected, reason: "
                "heap will exceed type limit of {}",
                sHeapType,
                mtxInternalResources.second.iHeapCapacity,
                static_cast<size_t>(mtxInternalResources.second.iHeapCapacity) +
                    static_cast<size_t>(iHeapGrowSize),
                INT_MAX));
        }

        const auto iOldHeapSize = mtxInternalResources.second.iHeapCapacity;

        auto optionalError = createHeap(iOldHeapSize + iHeapGrowSize);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        mtxInternalResources.second.iNextFreeHeapIndex = iOldHeapSize;
        mtxInternalResources.second.noLongerUsedDescriptorIndexes = {};

        return {};
    }

    std::optional<Error> DirectXDescriptorHeap::shrinkHeap() {
        std::scoped_lock guard(mtxInternalResources.first);

        if (mtxInternalResources.second.iHeapCapacity < iHeapGrowSize * 2) [[unlikely]] {
            return Error(std::format(
                "a request to shrink {} heap of capacity {} with the actual size of {} was rejected, reason: "
                "expected at least size of {}",
                sHeapType,
                mtxInternalResources.second.iHeapCapacity,
                mtxInternalResources.second.iHeapSize,
                iHeapGrowSize * 2));
        }

        if (mtxInternalResources.second.iHeapSize >
            mtxInternalResources.second.iHeapCapacity - iHeapGrowSize - iHeapGrowSize / 2) [[unlikely]] {
            return Error(std::format(
                "a request to shrink {} heap of capacity {} with the actual size of {} was rejected, reason: "
                "shrink condition is not met (size {} < {} is false)",
                sHeapType,
                mtxInternalResources.second.iHeapCapacity,
                mtxInternalResources.second.iHeapSize,
                mtxInternalResources.second.iHeapSize,
                mtxInternalResources.second.iHeapCapacity - iHeapGrowSize - iHeapGrowSize / 2));
        }

        const auto iNewHeapSize = mtxInternalResources.second.iHeapCapacity - iHeapGrowSize;

        auto optionalError = createHeap(iNewHeapSize);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        mtxInternalResources.second.iNextFreeHeapIndex = iNewHeapSize - iHeapGrowSize / 2;
        mtxInternalResources.second.noLongerUsedDescriptorIndexes = {};

        return {};
    }

    void DirectXDescriptorHeap::createView(
        CD3DX12_CPU_DESCRIPTOR_HANDLE heapHandle,
        const DirectXResource* pResource,
        GpuResource::DescriptorType descriptorType) const {
        const auto pRenderSettings = pRenderer->getRenderSettings();
        std::scoped_lock guard(pRenderSettings->first);

        switch (descriptorType) {
        case (GpuResource::DescriptorType::RTV): {
            pRenderer->getD3dDevice()->CreateRenderTargetView(
                pResource->getInternalResource(), nullptr, heapHandle);
        } break;
        case (GpuResource::DescriptorType::DSV): {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
            dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
            dsvDesc.ViewDimension = pRenderSettings->second->isAntialiasingEnabled()
                                        ? D3D12_DSV_DIMENSION_TEXTURE2DMS
                                        : D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Format = pRenderer->getDepthStencilBufferFormat();
            dsvDesc.Texture2D.MipSlice = 0;

            pRenderer->getD3dDevice()->CreateDepthStencilView(
                pResource->getInternalResource(), &dsvDesc, heapHandle);
        } break;
        case (GpuResource::DescriptorType::CBV): {
            const auto resourceGpuVirtualAddress = pResource->getInternalResource()->GetGPUVirtualAddress();

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            cbvDesc.BufferLocation = resourceGpuVirtualAddress;
            cbvDesc.SizeInBytes = static_cast<UINT>(pResource->getInternalResource()->GetDesc().Width);

            pRenderer->getD3dDevice()->CreateConstantBufferView(&cbvDesc, heapHandle);
        } break;
        case (GpuResource::DescriptorType::SRV): {
            const auto resourceDesc = pResource->getInternalResource()->GetDesc();

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

            pRenderer->getD3dDevice()->CreateShaderResourceView(
                pResource->getInternalResource(), &srvDesc, heapHandle);
        } break;
        case (GpuResource::DescriptorType::UAV): {
            const auto resourceDesc = pResource->getInternalResource()->GetDesc();

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

            pRenderer->getD3dDevice()->CreateUnorderedAccessView(
                pResource->getInternalResource(), nullptr, &uavDesc, heapHandle);
        } break;
        case (GpuResource::DescriptorType::END): {
            const Error err("invalid heap type");
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }
        }
    }

    std::optional<Error> DirectXDescriptorHeap::createHeap(INT iCapacity) {
        std::scoped_lock guard(mtxInternalResources.first);

        Logger::get().info(
            std::format(
                "flushing the command queue to (re)create {} descriptor heap (from capacity {} to {}) "
                "(current actual heap size: {})",
                sHeapType,
                mtxInternalResources.second.iHeapCapacity,
                iCapacity,
                mtxInternalResources.second.iHeapSize),
            sDescriptorHeapLogCategory);

        // Make sure we don't render anything and not processing any resources.
        std::scoped_lock drawGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Create heap.
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
        heapDesc.NumDescriptors = iCapacity;
        heapDesc.Type = d3dHeapType;
        heapDesc.Flags = heapDesc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
                             ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                             : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heapDesc.NodeMask = 0;

        const HRESULT hResult = pRenderer->getD3dDevice()->CreateDescriptorHeap(
            &heapDesc, IID_PPV_ARGS(mtxInternalResources.second.pHeap.ReleaseAndGetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Save heap size.
        mtxInternalResources.second.iHeapCapacity = iCapacity;

        recreateOldViews();

        return {};
    }

    std::vector<GpuResource::DescriptorType>
    DirectXDescriptorHeap::getDescriptorTypesHandledByThisHeap() const {
        switch (heapType) {
        case (DescriptorHeapType::RTV):
            return {GpuResource::DescriptorType::RTV};
        case (DescriptorHeapType::DSV):
            return {GpuResource::DescriptorType::DSV};
        case (DescriptorHeapType::CBV_SRV_UAV):
            return {
                GpuResource::DescriptorType::CBV,
                GpuResource::DescriptorType::SRV,
                GpuResource::DescriptorType::UAV};
        }

        const Error err("not handled heap type");
        err.showError();
        throw std::runtime_error(err.getFullErrorMessage());
    }

    void DirectXDescriptorHeap::recreateOldViews() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Start from 0 heap index, increment and update old offsets
        // to "shrink" heap usage (needed for heap shrinking).
        auto heapHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            mtxInternalResources.second.pHeap->GetCPUDescriptorHandleForHeapStart());
        INT iCurrentHeapIndex = 0;
        const auto vHandledDescriptorTypes = getDescriptorTypesHandledByThisHeap();

        for (const auto& pResource : mtxInternalResources.second.bindedResources) {
            for (const auto& descriptor : pResource->vHeapDescriptors) {
                if (!descriptor.has_value()) {
                    continue;
                }

                const auto descriptorIt =
                    std::ranges::find_if(vHandledDescriptorTypes, [&descriptor](const auto& descriptorType) {
                        return descriptor->descriptorType == descriptorType;
                    });
                if (descriptorIt == vHandledDescriptorTypes.end()) {
                    break; // this descriptor type is not handled in this heap
                }

                createView(heapHandle, pResource, descriptor->descriptorType);
                pResource->vHeapDescriptors[static_cast<int>(descriptor->descriptorType)]
                    ->iDescriptorOffsetInDescriptors = iCurrentHeapIndex;

                heapHandle.Offset(1, iDescriptorSize);
                iCurrentHeapIndex += 1;
            }
        }
    }
} // namespace ne
