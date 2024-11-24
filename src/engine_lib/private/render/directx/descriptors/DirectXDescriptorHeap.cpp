#include "DirectXDescriptorHeap.h"

// Custom.
#include "render/directx/DirectXRenderer.h"
#include "io/Logger.h"
#include "render/directx/resource/DirectXResource.h"

namespace ne {
    std::variant<std::unique_ptr<DirectXDescriptorHeap>, Error>
    DirectXDescriptorHeap::create(DirectXRenderer* pRenderer, DescriptorHeapType heapType) {
        auto pManager =
            std::unique_ptr<DirectXDescriptorHeap>(new DirectXDescriptorHeap(pRenderer, heapType));

        auto optionalError = pManager->createHeap(iHeapGrowSize, nullptr);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return pManager;
    }

    std::variant<std::shared_ptr<ContinuousDirectXDescriptorRange>, Error>
    DirectXDescriptorHeap::allocateContinuousDescriptorRange(
        const std::string& sRangeName, const std::function<void()>& onRangeIndicesChanged) {
        std::scoped_lock guard(mtxInternalData.first);

        // Create a new range.
        auto pRange = std::shared_ptr<ContinuousDirectXDescriptorRange>(
            new ContinuousDirectXDescriptorRange(this, onRangeIndicesChanged, sRangeName));

        // Add to the heap.
        mtxInternalData.second.continuousDescriptorRanges.insert(pRange.get());

        // Allocate initial capacity for the range.
        auto optionalError = expandRange(pRange.get());
        if (optionalError.has_value()) [[unlikely]] {
            auto error = std::move(optionalError.value());
            error.addCurrentLocationToErrorStack();
            return error;
        }

        return pRange;
    }

    std::optional<Error> DirectXDescriptorHeap::assignDescriptor(
        DirectXResource* pResource,
        DirectXDescriptorType descriptorType,
        const std::shared_ptr<ContinuousDirectXDescriptorRange>& pRange,
        bool bBindDescriptorsToCubemapFaces) {
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

        // Prepare range mutex (if valid).
        std::recursive_mutex mtxDummy;
        std::recursive_mutex* pRangeMutex = &mtxDummy;
        if (pRange != nullptr) {
            pRangeMutex = &pRange->mtxInternalData.first;
        }

        // Lock heap, resource descriptors and range together to avoid deadlocks.
        std::scoped_lock guard(mtxInternalData.first, pResource->mtxHeapDescriptors.first, *pRangeMutex);

        // Prepare a lambda to allocate a descriptor.
        const auto allocateDescriptor = [&](std::optional<size_t> cubemapFaceIndex) -> std::optional<Error> {
            // Prepare to create a new view.
            INT iFreeDescriptorIndexInHeap = 0;

            if (pRange != nullptr) {
                // Make sure the specified range exists.
                const auto rangeIt = mtxInternalData.second.continuousDescriptorRanges.find(pRange.get());
                if (rangeIt == mtxInternalData.second.continuousDescriptorRanges.end()) [[unlikely]] {
                    return Error(std::format(
                        "resource \"{}\" attempted to assign a descriptor in {} heap with "
                        "invalid range specified",
                        pResource->getResourceName(),
                        sHeapType));
                }

                // Lock range data.
                std::scoped_lock rangeGuard(pRange->mtxInternalData.first);

                // Get free heap index from range to use.
                auto indexResult = pRange->tryReserveFreeHeapIndexToCreateDescriptor();
                if (std::holds_alternative<Error>(indexResult)) [[unlikely]] {
                    auto error = std::get<Error>(std::move(indexResult));
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
                auto optionalHeapIndex = std::get<std::optional<INT>>(indexResult);

                if (!optionalHeapIndex.has_value()) {
                    // Expand range.
                    auto optionalError = expandRange(pRange.get());
                    if (optionalError.has_value()) [[unlikely]] {
                        optionalError->addCurrentLocationToErrorStack();
                        return optionalError;
                    }

                    // Again, get free heap index from range to use.
                    indexResult = pRange->tryReserveFreeHeapIndexToCreateDescriptor();
                    if (std::holds_alternative<Error>(indexResult)) [[unlikely]] {
                        auto error = std::get<Error>(std::move(indexResult));
                        error.addCurrentLocationToErrorStack();
                        return error;
                    }
                    optionalHeapIndex = std::get<std::optional<INT>>(indexResult);

                    // Make sure it's valid.
                    if (!optionalHeapIndex.has_value()) [[unlikely]] {
                        return Error(std::format(
                            "{} heap expanded the range \"{}\" but the range still reports that there are no "
                            "space for a new descriptor",
                            sHeapType,
                            pRange->sRangeName));
                    }
                }

                // Save index.
                iFreeDescriptorIndexInHeap = optionalHeapIndex.value();
            } else {
                // Expand the heap if it's full.
                if (mtxInternalData.second.iHeapSize == mtxInternalData.second.iHeapCapacity) {
                    auto optionalError = expandHeap(nullptr);
                    if (optionalError.has_value()) [[unlikely]] {
                        optionalError->addCurrentLocationToErrorStack();
                        return optionalError.value();
                    }
                }

                // Mark increased heap size.
                mtxInternalData.second.iHeapSize += 1;

                // Get free index.
                if (mtxInternalData.second.iNextFreeHeapIndex == mtxInternalData.second.iHeapCapacity) {
                    iFreeDescriptorIndexInHeap =
                        mtxInternalData.second.noLongerUsedSingleDescriptorIndices.front();
                    mtxInternalData.second.noLongerUsedSingleDescriptorIndices.pop();
                } else {
                    iFreeDescriptorIndexInHeap = mtxInternalData.second.iNextFreeHeapIndex;
                    mtxInternalData.second.iNextFreeHeapIndex += 1;
                }
            }

            // Create heap handle to point to a free place.
            auto heapHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
                mtxInternalData.second.pHeap->GetCPUDescriptorHandleForHeapStart());
            heapHandle.Offset(iFreeDescriptorIndexInHeap, iDescriptorSize);

            // Create view.
            createView(heapHandle, pResource, descriptorType, cubemapFaceIndex);

            // Create descriptor.
            auto pDescriptor = std::unique_ptr<DirectXDescriptor>(new DirectXDescriptor(
                this, descriptorType, pResource, iFreeDescriptorIndexInHeap, cubemapFaceIndex, pRange));

            // Save descriptor.
            if (pRange != nullptr) {
                pRange->mtxInternalData.second.allocatedDescriptors.insert(pDescriptor.get());
            } else {
                mtxInternalData.second.bindedSingleDescriptors.insert(pDescriptor.get());
            }

            // Save descriptor in resource.
            auto& descriptorsSameType =
                pResource->mtxHeapDescriptors.second[static_cast<int>(descriptorType)];
            if (cubemapFaceIndex.has_value()) {
                descriptorsSameType.vCubemapFaces[cubemapFaceIndex.value()] = std::move(pDescriptor);
            } else {
                descriptorsSameType.pResource = std::move(pDescriptor);
            }

            return {};
        };

        // Check if this resource is a cubemap.
        D3D12_RESOURCE_DESC desc;
        desc = pResource->pInternalResource->GetDesc();
        const auto bIsCubeMap = desc.DepthOrArraySize == 6; // NOLINT: cubemap face count

        // Bind a descriptor to the entire resource.
        auto optionalError = allocateDescriptor({});
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        if (bIsCubeMap && bBindDescriptorsToCubemapFaces) {
            // Bind a descriptor to each cubemap face.
            for (size_t iDescriptorIndex = 0; iDescriptorIndex < desc.DepthOrArraySize; iDescriptorIndex++) {
                auto optionalError = allocateDescriptor(iDescriptorIndex);
                if (optionalError.has_value()) [[unlikely]] {
                    optionalError->addCurrentLocationToErrorStack();
                    return optionalError;
                }
            }
        }

        return {};
    }

    INT DirectXDescriptorHeap::getHeapCapacity() {
        std::scoped_lock guard(mtxInternalData.first);
        return mtxInternalData.second.iHeapCapacity;
    }

    INT DirectXDescriptorHeap::getHeapSize() {
        std::scoped_lock guard(mtxInternalData.first);
        return mtxInternalData.second.iHeapSize;
    }

    size_t DirectXDescriptorHeap::getNoLongerUsedDescriptorCount() {
        std::scoped_lock guard(mtxInternalData.first);
        return mtxInternalData.second.noLongerUsedSingleDescriptorIndices.size();
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
        static_assert(
            ContinuousDirectXDescriptorRange::iRangeGrowSize % 2 == 0,
            "grow size must be even because we use INT / INT");
        static_assert(
            ContinuousDirectXDescriptorRange::iRangeGrowSize > iHeapGrowSize / 8, // NOLINT
            "avoid small range grow size because each time a range needs expand/shrink operation it will "
            "cause the heap to be re-created");
        static_assert(
            ContinuousDirectXDescriptorRange::iRangeGrowSize < iHeapGrowSize / 2,
            "if the range grow size will exceed heap grow size heap's expand function will allocate not "
            "enough descriptors and shrink function will also behave incorrectly");

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

    DirectXDescriptorHeap::~DirectXDescriptorHeap() {
        std::scoped_lock guard(mtxInternalData.first);

        // Make sure no single descriptor exists.
        if (!mtxInternalData.second.bindedSingleDescriptors.empty()) [[unlikely]] {
            Error error(std::format(
                "descriptor heap \"{}\" is being destroyed but there are still {} single descriptor(s) alive",
                sHeapType,
                mtxInternalData.second.bindedSingleDescriptors.size()));
            error.showError();
            return; // don't throw in destructor
        }

        // Make sure no range exists.
        if (!mtxInternalData.second.continuousDescriptorRanges.empty()) [[unlikely]] {
            Error error(std::format(
                "descriptor heap \"{}\" is being destroyed but there are still {} descriptor range(s) alive",
                sHeapType,
                mtxInternalData.second.continuousDescriptorRanges.size()));
            error.showError();
            return; // don't throw in destructor
        }

        // Make sure our size is zero.
        if (mtxInternalData.second.iHeapSize != 0) [[unlikely]] {
            Error error(std::format(
                "descriptor heap \"{}\" is being destroyed but its size is {}",
                sHeapType,
                mtxInternalData.second.iHeapSize));
            error.showError();
            return; // don't throw in destructor
        }
    }

    void DirectXDescriptorHeap::onDescriptorBeingDestroyed(
        DirectXDescriptor* pDescriptor, ContinuousDirectXDescriptorRange* pRange) {
        // Prepare range mutex if valid.
        std::recursive_mutex mtxDummy;
        std::recursive_mutex* pRangeMutex = &mtxDummy;
        if (pRange != nullptr) {
            pRangeMutex = &pRange->mtxInternalData.first;
        }

        // Lock together to avoid deadlocks.
        std::scoped_lock guard(mtxInternalData.first, *pRangeMutex);

        if (pRange == nullptr) {
            // Make sure the specified descriptor actually exists in our "database".
            const auto it = mtxInternalData.second.bindedSingleDescriptors.find(pDescriptor);
            if (it == mtxInternalData.second.bindedSingleDescriptors.end()) [[unlikely]] {
                Error error(std::format(
                    "descriptor notified the heap \"{}\" about being destroyed but the heap is unable to "
                    "find this descriptor (with descriptor offset {}) in the heap's \"database\" of active "
                    "descriptors",
                    sHeapType,
                    pDescriptor->iDescriptorOffsetInDescriptors));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Remove this resource from our "database".
            mtxInternalData.second.bindedSingleDescriptors.erase(it);

            // Save index of this descriptor to array of no longer used descriptors indices.
            mtxInternalData.second.noLongerUsedSingleDescriptorIndices.push(
                pDescriptor->iDescriptorOffsetInDescriptors);

            // Decrement heap size.
            mtxInternalData.second.iHeapSize -= 1;

            // Shrink the heap if needed.
            auto result = shrinkHeapIfPossible(nullptr);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            return;
        }

        // Make sure the specified range exists.
        const auto rangeIt = mtxInternalData.second.continuousDescriptorRanges.find(pRange);
        if (rangeIt == mtxInternalData.second.continuousDescriptorRanges.end()) [[unlikely]] {
            Error error(std::format(
                "descriptor notified the heap \"{}\" about being destroyed (was allocated from a range) "
                "but the heap can't find the specified range in the array of previously created ranges",
                sHeapType));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Remove descriptor from range.
        auto optionalError = pRange->markDescriptorAsUnused(pDescriptor);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }

        auto& rangeData = pRange->mtxInternalData.second;

        // Check if range can be shrinked.
        if (!isShrinkingPossible(
                static_cast<INT>(rangeData.allocatedDescriptors.size()),
                rangeData.iRangeCapacity,
                ContinuousDirectXDescriptorRange::iRangeGrowSize)) {
            // Nothing to do.
            return;
        }

        // Update range capacity.
        rangeData.iRangeCapacity -= ContinuousDirectXDescriptorRange::iRangeGrowSize;

        // Update heap size because range capacity changed.
        mtxInternalData.second.iHeapSize -= ContinuousDirectXDescriptorRange::iRangeGrowSize;

        // Shrink the heap if needed.
        auto result = shrinkHeapIfPossible(pRange);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        const auto bShrinkedHeap = std::get<bool>(result);

        if (bShrinkedHeap) {
            // Heap was re-created and the space that the range was using was updated in the heap.
            return;
        }

        // Re-create the heap to update space for range.
        optionalError = createHeap(mtxInternalData.second.iHeapCapacity, pRange);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }
    }

    void DirectXDescriptorHeap::onDescriptorRangeBeingDestroyed(ContinuousDirectXDescriptorRange* pRange) {
        std::scoped_lock guard(mtxInternalData.first);

        // Make sure this range was "registered".
        const auto rangeIt = mtxInternalData.second.continuousDescriptorRanges.find(pRange);
        if (rangeIt == mtxInternalData.second.continuousDescriptorRanges.end()) [[unlikely]] {
            Error error(std::format(
                "descriptor range \"{}\" notified the heap \"{}\" about being destroyed but this "
                "heap is unable to find the range in the array of previously created ranges",
                pRange->sRangeName,
                sHeapType));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Remove range size from heap size.
        mtxInternalData.second.iHeapSize -= pRange->mtxInternalData.second.iRangeCapacity;

        // Remove range.
        mtxInternalData.second.continuousDescriptorRanges.erase(rangeIt);

        // Shrink the heap if needed.
        auto result = shrinkHeapIfPossible(pRange);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        const auto bShrinkedHeap = std::get<bool>(result);

        if (bShrinkedHeap) {
            // Heap was re-created and deleted range no longer takes any place in the heap.
            return;
        }

        // Re-create the heap to remove space for deleted range.
        auto optionalError = createHeap(mtxInternalData.second.iHeapCapacity, pRange);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }
    }

    std::optional<Error> DirectXDescriptorHeap::expandHeap(ContinuousDirectXDescriptorRange* pChangedRange) {
        std::scoped_lock guard(mtxInternalData.first);

        // Make sure the heap is fully filled and there's no free space.
        if (mtxInternalData.second.iHeapSize < mtxInternalData.second.iHeapCapacity) [[unlikely]] {
            return Error(std::format(
                "a request to expand {} heap of capacity {} while the actual size is {} was "
                "rejected, reason: expand condition is not met (this is a bug, report to developers)",
                sHeapType,
                mtxInternalData.second.iHeapCapacity,
                mtxInternalData.second.iHeapSize));
        }

        // Make sure there are no unused descriptors.
        if (!mtxInternalData.second.noLongerUsedSingleDescriptorIndices.empty()) [[unlikely]] {
            return Error(std::format(
                "requested to expand {} heap of capacity {} while there are not used "
                "descriptors exist ({}) (actual heap size is {}) (this is a bug, report to developers)",
                sHeapType,
                mtxInternalData.second.iHeapCapacity,
                mtxInternalData.second.noLongerUsedSingleDescriptorIndices.size(),
                mtxInternalData.second.iHeapSize));
        }

        // Make sure our new capacity will not exceed type limit.
        constexpr auto iMaxHeapCapacity = std::numeric_limits<INT>::max();
        if (iMaxHeapCapacity - iHeapGrowSize < mtxInternalData.second.iHeapCapacity) [[unlikely]] {
            return Error(std::format(
                "a request to expand {} descriptor heap of capacity {} was rejected, reason: "
                "heap will exceed the type limit of {}",
                sHeapType,
                mtxInternalData.second.iHeapCapacity,
                iMaxHeapCapacity));
        }

        // Save old heap capacity to use later.
        const auto iOldHeapCapacity = mtxInternalData.second.iHeapCapacity;

        // Re-create the heap with the new capacity.
        auto optionalError = createHeap(iOldHeapCapacity + iHeapGrowSize, pChangedRange);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return {};
    }

    bool DirectXDescriptorHeap::isShrinkingPossible(INT iSize, INT iCapacity, INT iGrowSize) {
        // Make sure grow size is even (because we do INT / INT and expect even values).
        if (iGrowSize % 2 != 0) [[unlikely]] {
            Error error(std::format("expected grow size to be even, got: {}", iGrowSize));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure we can shrink.
        if (iCapacity < iGrowSize * 2) {
            return false;
        }

        // Only shrink if we can erase `iGrowSize` unused elements and will have some
        // free space (i.e. we will not be on the edge to expand).
        if (iSize > (iCapacity - iGrowSize - iGrowSize / 2)) {
            return false;
        }

        return true;
    }

    std::optional<Error> DirectXDescriptorHeap::expandRange(ContinuousDirectXDescriptorRange* pRange) {
        std::scoped_lock guard(mtxInternalData.first, pRange->mtxInternalData.first);

        // Expand the range.
        pRange->mtxInternalData.second.iRangeCapacity += ContinuousDirectXDescriptorRange::iRangeGrowSize;

        // Update heap size.
        mtxInternalData.second.iHeapSize += ContinuousDirectXDescriptorRange::iRangeGrowSize;

        // Expand the heap if needed.
        if (mtxInternalData.second.iHeapSize > mtxInternalData.second.iHeapCapacity) {
            // Expand the heap to update space for this range.
            auto optionalError = expandHeap(pRange);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }

            return {};
        }

        // Re-create the heap to update space for this range.
        auto optionalError = createHeap(mtxInternalData.second.iHeapCapacity, pRange);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return {};
    };

    std::variant<bool, Error>
    DirectXDescriptorHeap::shrinkHeapIfPossible(ContinuousDirectXDescriptorRange* pChangedRange) {
        std::scoped_lock guard(mtxInternalData.first);

        // Make sure we can shrink.
        if (!isShrinkingPossible(
                mtxInternalData.second.iHeapSize, mtxInternalData.second.iHeapCapacity, iHeapGrowSize)) {
            return false;
        }

        // Calculate the new capacity.
        const auto iNewHeapCapacity = mtxInternalData.second.iHeapCapacity - iHeapGrowSize;

        // Re-create the heap with the new capacity.
        auto optionalError = createHeap(iNewHeapCapacity, pChangedRange);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return true;
    }

    void DirectXDescriptorHeap::createView(
        CD3DX12_CPU_DESCRIPTOR_HANDLE heapHandle,
        const DirectXResource* pResource,
        DirectXDescriptorType descriptorType,
        std::optional<size_t> cubemapFaceIndex) const {
        // Get resource description.
        const auto resourceDesc = pResource->getInternalResource()->GetDesc();

        switch (descriptorType) {
        case (DirectXDescriptorType::RTV): {
            // Prepare description.
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
            rtvDesc.Format = resourceDesc.Format;

            if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
                Error error(std::format(
                    "unable to create DSV for resource \"{}\": 3D texture support not implemented",
                    pResource->getResourceName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
                // Set dimension.
                rtvDesc.ViewDimension = resourceDesc.SampleDesc.Count > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS
                                                                          : D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Texture2D.MipSlice = 0;
                rtvDesc.Texture2D.PlaneSlice = 0;

                if (resourceDesc.DepthOrArraySize > 1) {
                    // Reference the whole texture array in one view.
                    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                    rtvDesc.Texture2DArray.MipSlice = 0;

                    // Reference all textures.
                    rtvDesc.Texture2DArray.FirstArraySlice = 0;
                    rtvDesc.Texture2DArray.ArraySize = resourceDesc.DepthOrArraySize;
                }

                if (cubemapFaceIndex.has_value()) {
                    // Specify which cubemap face to reference in the view.
                    rtvDesc.Texture2DArray.FirstArraySlice = static_cast<UINT>(cubemapFaceIndex.value());
                    rtvDesc.Texture2DArray.ArraySize = 1;
                }
            } else [[unlikely]] {
                Error error(std::format(
                    "unexpected resource dimension for RTV of resource \"{}\": {}",
                    static_cast<int>(resourceDesc.Dimension),
                    pResource->getResourceName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            pRenderer->getD3dDevice()->CreateRenderTargetView(
                pResource->getInternalResource(), &rtvDesc, heapHandle);
            break;
        }
        case (DirectXDescriptorType::DSV): {
            // Prepare DSV description.
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
            dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
            dsvDesc.Format = resourceDesc.Format;

            if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
                Error error(std::format(
                    "unable to create DSV for resource \"{}\": 3D texture support not implemented",
                    pResource->getResourceName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
                // Set dimension.
                dsvDesc.ViewDimension = resourceDesc.SampleDesc.Count > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS
                                                                          : D3D12_DSV_DIMENSION_TEXTURE2D;

                dsvDesc.Texture2D.MipSlice = 0;

                if (resourceDesc.DepthOrArraySize > 1) {
                    // Reference the whole texture array in one view.
                    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                    dsvDesc.Texture2DArray.MipSlice = 0;

                    // Reference all textures.
                    dsvDesc.Texture2DArray.FirstArraySlice = 0;
                    dsvDesc.Texture2DArray.ArraySize = resourceDesc.DepthOrArraySize;
                }

                if (cubemapFaceIndex.has_value()) {
                    // Specify which cubemap face to reference in the view.
                    dsvDesc.Texture2DArray.FirstArraySlice = static_cast<UINT>(cubemapFaceIndex.value());
                    dsvDesc.Texture2DArray.ArraySize = 1;
                }
            } else [[unlikely]] {
                Error error(std::format(
                    "unexpected resource dimension for DSV of resource \"{}\": {}",
                    static_cast<int>(resourceDesc.Dimension),
                    pResource->getResourceName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Create DSV.
            pRenderer->getD3dDevice()->CreateDepthStencilView(
                pResource->getInternalResource(), &dsvDesc, heapHandle);
            break;
        }
        case (DirectXDescriptorType::CBV): {
            const auto resourceGpuVirtualAddress = pResource->getInternalResource()->GetGPUVirtualAddress();

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            cbvDesc.BufferLocation = resourceGpuVirtualAddress;
            cbvDesc.SizeInBytes = static_cast<UINT>(pResource->getInternalResource()->GetDesc().Width);

            // Create CBV.
            pRenderer->getD3dDevice()->CreateConstantBufferView(&cbvDesc, heapHandle);
            break;
        }
        case (DirectXDescriptorType::SRV): {
            // Prepare SRV description.
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

            // Setup SRV depending on the dimension.
            switch (resourceDesc.Dimension) {
            case (D3D12_RESOURCE_DIMENSION_TEXTURE3D): {
                Error error(std::format(
                    "unable to create SRV for resource \"{}\": 3D texture support not implemented",
                    pResource->getResourceName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
                break;
            }
            case (D3D12_RESOURCE_DIMENSION_TEXTURE2D): {
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MostDetailedMip = 0;
                srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
                if (resourceDesc.Format == DXGI_FORMAT_D32_FLOAT) {
                    // SRV cannon be created with the depth component so use the red component instead.
                    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
                } else {
                    srvDesc.Format = resourceDesc.Format;
                }

                if (resourceDesc.DepthOrArraySize > 1) {
                    // Reference the whole texture array in one view.
                    srvDesc.ViewDimension = resourceDesc.DepthOrArraySize == 6 // NOLINT
                                                ? D3D12_SRV_DIMENSION_TEXTURECUBE
                                                : D3D12_SRV_DIMENSION_TEXTURE2DARRAY;

                    srvDesc.Texture2DArray.PlaneSlice = 0;
                    srvDesc.Texture2DArray.MostDetailedMip = 0;
                    srvDesc.Texture2DArray.MipLevels = resourceDesc.MipLevels;

                    // Reference all textures.
                    srvDesc.Texture2DArray.FirstArraySlice = 0;
                    srvDesc.Texture2DArray.ArraySize = resourceDesc.DepthOrArraySize;
                }

                if (cubemapFaceIndex.has_value()) {
                    // Specify which cubemap face to reference in the view.
                    srvDesc.Texture2DArray.FirstArraySlice = static_cast<UINT>(cubemapFaceIndex.value());
                    srvDesc.Texture2DArray.ArraySize = 1;
                }

                break;
            }
            case (D3D12_RESOURCE_DIMENSION_BUFFER): {
                // Make sure element size / count are specified.
                if (pResource->getElementSizeInBytes() == 0 || pResource->getElementCount() == 0) {
                    Error error(std::format(
                        "unable to create an SRV for resource \"{}\" because its element size/count were not "
                        "specified",
                        pResource->getResourceName()));
                    error.showError();
                    throw std::runtime_error(error.getFullErrorMessage());
                }
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srvDesc.Buffer.FirstElement = 0;
                srvDesc.Buffer.StructureByteStride = pResource->getElementSizeInBytes();
                srvDesc.Buffer.NumElements = pResource->getElementCount();
                srvDesc.Format = DXGI_FORMAT_UNKNOWN; // must be `UNKNOWN` if `StructureByteStride` is not 0
                break;
            }
            default: {
                Error error(
                    std::format("unsupported resource dimension \"{}\"", pResource->getResourceName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
                break;
            }
            }

            // Fill general info.
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            // Create SRV.
            pRenderer->getD3dDevice()->CreateShaderResourceView(
                pResource->getInternalResource(), &srvDesc, heapHandle);
            break;
        }
        case (DirectXDescriptorType::UAV): {
            const auto resourceDesc = pResource->getInternalResource()->GetDesc();

            // Prepare base description.
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = resourceDesc.Format;
            uavDesc.Texture2D.MipSlice = 0;

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

            if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_UNKNOWN ||
                resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
                // Make sure element size / count are specified.
                if (pResource->getElementSizeInBytes() == 0 || pResource->getElementCount() == 0) {
                    Error error(std::format(
                        "unable to create an UAV for resource \"{}\" because its element size/count were not "
                        "specified",
                        pResource->getResourceName()));
                    error.showError();
                    throw std::runtime_error(error.getFullErrorMessage());
                }

                uavDesc.Buffer.FirstElement = 0;
                uavDesc.Buffer.StructureByteStride = pResource->getElementSizeInBytes();
                uavDesc.Buffer.NumElements = pResource->getElementCount();
                uavDesc.Format = DXGI_FORMAT_UNKNOWN; // must be `UNKNOWN` if `StructureByteStride` is not 0
            }

            // Set view dimension.
            uavDesc.ViewDimension = uavDimension;

            // Create UAV.
            pRenderer->getD3dDevice()->CreateUnorderedAccessView(
                pResource->getInternalResource(), nullptr, &uavDesc, heapHandle);
            break;
        }
        case (DirectXDescriptorType::END): {
            const Error err("invalid heap type");
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }
        }
    }

    std::optional<Error>
    DirectXDescriptorHeap::createHeap(INT iCapacity, ContinuousDirectXDescriptorRange* pChangedRange) {
        std::scoped_lock guard(mtxInternalData.first);

        // Prepare log message.
        auto sLogMessage = std::format(
            "waiting for the GPU to finish work up to this point to (re)create {} descriptor heap from "
            "capacity {} to {} (current actual heap size: {}) (range count: {})",
            sHeapType,
            mtxInternalData.second.iHeapCapacity,
            iCapacity,
            mtxInternalData.second.iHeapSize,
            mtxInternalData.second.continuousDescriptorRanges.size());

        // Add range info (if specified).
        if (pChangedRange != nullptr) {
            sLogMessage +=
                std::format(" due to changes in a descriptor range \"{}\"", pChangedRange->sRangeName);
        }

        // Log message.
        Logger::get().info(sLogMessage);

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
            &heapDesc, IID_PPV_ARGS(mtxInternalData.second.pHeap.ReleaseAndGetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Save heap size.
        mtxInternalData.second.iHeapCapacity = iCapacity;

        // Re-bind views and update heap indices.
        auto optionalError = rebindViewsUpdateIndices();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::vector<DirectXDescriptorType> DirectXDescriptorHeap::getDescriptorTypesHandledByThisHeap() const {
        switch (heapType) {
        case (DescriptorHeapType::RTV):
            return {DirectXDescriptorType::RTV};
        case (DescriptorHeapType::DSV):
            return {DirectXDescriptorType::DSV};
        case (DescriptorHeapType::CBV_SRV_UAV):
            return {DirectXDescriptorType::CBV, DirectXDescriptorType::SRV, DirectXDescriptorType::UAV};
        }

        const Error err("not handled heap type");
        err.showError();
        throw std::runtime_error(err.getFullErrorMessage());
    }

    std::optional<Error> DirectXDescriptorHeap::rebindViewsUpdateIndices() {
        std::scoped_lock guard(mtxInternalData.first);

        // Start from 0 heap index, increment and update old offsets
        // to "shrink" heap usage (needed for heap shrinking).
        auto heapHandle =
            CD3DX12_CPU_DESCRIPTOR_HANDLE(mtxInternalData.second.pHeap->GetCPUDescriptorHandleForHeapStart());
        INT iCurrentHeapIndex = 0;

        // Prepare a lambda to update a descriptor.
        const auto updateDescriptor = [&](DirectXDescriptor* pDescriptor) -> std::optional<Error> {
            // Self check: make sure we don't assign indices out of heap bounds.
            if (iCurrentHeapIndex >= mtxInternalData.second.iHeapCapacity) [[unlikely]] {
                return Error(std::format(
                    "next free descriptor index {} reached heap capacity {}",
                    iCurrentHeapIndex,
                    mtxInternalData.second.iHeapCapacity));
            }

            // Lock resource descriptors.
            std::scoped_lock resourceDescriptorsGuard(pDescriptor->pResource->mtxHeapDescriptors.first);

            // Re-create the view.
            createView(
                heapHandle,
                pDescriptor->pResource,
                pDescriptor->descriptorType,
                pDescriptor->referencedCubemapFaceIndex);

            // Update descriptor index.
            pDescriptor->iDescriptorOffsetInDescriptors = iCurrentHeapIndex;

            // Increment next descriptor index.
            heapHandle.Offset(1, iDescriptorSize);
            iCurrentHeapIndex += 1;

            return {};
        };

        // First, assign space for continuous descriptor ranges.
        for (auto& pRange : mtxInternalData.second.continuousDescriptorRanges) {
            std::scoped_lock rangeGuard(pRange->mtxInternalData.first);

            // Prepare a short reference for range data.
            auto& rangeData = pRange->mtxInternalData.second;

            // Assign new range start.
            bool bInitializedRangeForTheFirstTime = rangeData.iRangeStartInHeap < 0;
            rangeData.iRangeStartInHeap = iCurrentHeapIndex;

            // Refresh range indices.
            rangeData.iNextFreeIndexInRange = 0;
            rangeData.noLongerUsedDescriptorIndices = {};

            // Update descriptors of the range.
            for (const auto& pDescriptor : rangeData.allocatedDescriptors) {
                // Self check: make sure we don't assign indices out of range bounds.
                if (rangeData.iNextFreeIndexInRange >= rangeData.iRangeCapacity) [[unlikely]] {
                    return Error(std::format(
                        "next free range descriptor index {} reached range capacity {}",
                        rangeData.iNextFreeIndexInRange,
                        rangeData.iRangeCapacity));
                }

                // Update descriptor.
                auto optionalError = updateDescriptor(pDescriptor);
                if (optionalError.has_value()) [[unlikely]] {
                    optionalError->addCurrentLocationToErrorStack();
                    return optionalError;
                }

                // Increment next free range index.
                rangeData.iNextFreeIndexInRange += 1;
            }

            if (!bInitializedRangeForTheFirstTime) {
                // Notify the user of this range.
                pRange->onRangeIndicesChanged();
            } // else: the range is just being created/initialized and we should not notify the user because
              // the user still have not received the range pointer

            // Jump to the end of the range.
            const auto iSkipDescriptorCount =
                rangeData.iRangeCapacity - static_cast<INT>(rangeData.allocatedDescriptors.size());
            if (iSkipDescriptorCount < 0) [[unlikely]] {
                return Error(std::format(
                    "unexpected internal range descriptor counter to be negative ({}) for range \"{}\" "
                    "(capacity: {}, size: {})",
                    iSkipDescriptorCount,
                    pRange->sRangeName,
                    rangeData.iRangeCapacity,
                    rangeData.allocatedDescriptors.size()));
            }
            iCurrentHeapIndex += iSkipDescriptorCount;
            heapHandle.Offset(iSkipDescriptorCount, iDescriptorSize);
        }

        // Iterate over all descriptors from this heap.
        for (const auto& pDescriptor : mtxInternalData.second.bindedSingleDescriptors) {
            // Update descriptor.
            auto optionalError = updateDescriptor(pDescriptor);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        // Update internal values.
        mtxInternalData.second.iNextFreeHeapIndex = iCurrentHeapIndex;
        mtxInternalData.second.noLongerUsedSingleDescriptorIndices = {};

        return {};
    }

    ContinuousDirectXDescriptorRange::ContinuousDirectXDescriptorRange(
        DirectXDescriptorHeap* pHeap,
        const std::function<void()>& onRangeIndicesChanged,
        const std::string& sRangeName)
        : onRangeIndicesChanged(onRangeIndicesChanged), sRangeName(sRangeName), pHeap(pHeap) {}

    size_t ContinuousDirectXDescriptorRange::getRangeSize() {
        std::scoped_lock guard(mtxInternalData.first);
        return mtxInternalData.second.allocatedDescriptors.size();
    }

    size_t ContinuousDirectXDescriptorRange::getRangeCapacity() {
        std::scoped_lock guard(mtxInternalData.first);
        return static_cast<size_t>(mtxInternalData.second.iRangeCapacity);
    }

    INT ContinuousDirectXDescriptorRange::getRangeStartInHeap() {
        std::scoped_lock guard(mtxInternalData.first);
        return mtxInternalData.second.iRangeStartInHeap;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE ContinuousDirectXDescriptorRange::getGpuDescriptorHandleToRangeStart() const {
        return D3D12_GPU_DESCRIPTOR_HANDLE{
            pHeap->getInternalHeap()->GetGPUDescriptorHandleForHeapStart().ptr +
            mtxInternalData.second.iRangeStartInHeap * pHeap->getDescriptorSize()};
    }

    ContinuousDirectXDescriptorRange::~ContinuousDirectXDescriptorRange() {
        std::scoped_lock guard(mtxInternalData.first);

        // Make sure no descriptor references the range.
        if (!mtxInternalData.second.allocatedDescriptors.empty()) [[unlikely]] {
            Error error(std::format(
                "range \"{}\" is being destroyed but there are still {} active descriptor(s) that reference "
                "it",
                sRangeName,
                mtxInternalData.second.allocatedDescriptors.size()));
            error.showError();
            return; // don't throw in destructor
        }

        // Notify heap.
        pHeap->onDescriptorRangeBeingDestroyed(this);
    }

    std::optional<Error>
    ContinuousDirectXDescriptorRange::markDescriptorAsUnused(DirectXDescriptor* pDescriptor) {
        std::scoped_lock guard(mtxInternalData.first);

        // Make sure this descriptor exists.
        const auto descriptorIt = mtxInternalData.second.allocatedDescriptors.find(pDescriptor);
        if (descriptorIt == mtxInternalData.second.allocatedDescriptors.end()) [[unlikely]] {
            return Error(
                std::format("range \"{}\" is unable to find the specified descriptor to remove", sRangeName));
        }

        // Remove descriptor.
        mtxInternalData.second.allocatedDescriptors.erase(descriptorIt);

        // Mark index as unused.
        mtxInternalData.second.noLongerUsedDescriptorIndices.push(
            pDescriptor->getDescriptorOffsetInDescriptors());

        // Nothing else needs to be done (the heap will check the shrinking condition).
        return {};
    }

    std::variant<std::optional<INT>, Error>
    ContinuousDirectXDescriptorRange::tryReserveFreeHeapIndexToCreateDescriptor() {
        std::scoped_lock guard(mtxInternalData.first);

        auto& rangeData = mtxInternalData.second;

        // Check if range is full.
        if (rangeData.iNextFreeIndexInRange == rangeData.iRangeCapacity) {
            if (rangeData.noLongerUsedDescriptorIndices.empty()) {
                // No space.
                return std::optional<INT>{};
            }

            // Reserve unused index.
            const auto iFreeDescriptorIndexInHeap = rangeData.noLongerUsedDescriptorIndices.front();
            rangeData.noLongerUsedDescriptorIndices.pop();

            return iFreeDescriptorIndexInHeap;
        }

        // Self check: make sure next free index will not exceed capacity.
        if (rangeData.iNextFreeIndexInRange >= rangeData.iRangeCapacity) [[unlikely]] {
            return Error(std::format(
                "range \"{}\" next free descriptor index {} reached range capacity {}",
                sRangeName,
                rangeData.iNextFreeIndexInRange,
                rangeData.iRangeCapacity));
        }

        // Reserve new index.
        const auto iFreeDescriptorIndexInHeap = rangeData.iRangeStartInHeap + rangeData.iNextFreeIndexInRange;

        // Increment next free descriptor index.
        rangeData.iNextFreeIndexInRange += 1;

        return iFreeDescriptorIndexInHeap;
    }
} // namespace ne
