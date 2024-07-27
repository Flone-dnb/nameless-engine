#include "DirectXResource.h"

// Custom.
#include "render/directx/descriptors/DirectXDescriptorHeap.h"
#include "render/directx/resources/DirectXResourceManager.h"
#include "misc/Globals.h"
#include "render/directx/DirectXRenderer.h"

namespace ne {

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResource::create(
        DirectXResourceManager* pResourceManager,
        const std::string& sResourceName,
        D3D12MA::Allocator* pMemoryAllocator,
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState,
        std::optional<D3D12_CLEAR_VALUE> resourceClearValue,
        size_t iElementSizeInBytes,
        size_t iElementCount) {
        // Make sure element size/count will fit type limit.
        if (iElementSizeInBytes > std::numeric_limits<unsigned int>::max()) [[unlikely]] {
            Error error(std::format(
                "unable to create resource \"{}\" because its element size ({}) will exceed type limit",
                sResourceName,
                iElementSizeInBytes));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        } else if (iElementCount > std::numeric_limits<unsigned int>::max()) [[unlikely]] {
            Error error(std::format(
                "unable to create resource \"{}\" because its element count ({}) will exceed type limit",
                sResourceName,
                iElementCount));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Create resource.
        auto pCreatedResource = std::unique_ptr<DirectXResource>(new DirectXResource(
            pResourceManager,
            sResourceName,
            static_cast<UINT>(iElementSizeInBytes),
            static_cast<UINT>(iElementCount)));

        // Prepare clear value.
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
        DirectXResourceManager* pResourceManager,
        DirectXDescriptorHeap* pRtvHeap,
        const ComPtr<ID3D12Resource>& pSwapChainBuffer) {
        auto pCreatedResource = std::unique_ptr<DirectXResource>(
            new DirectXResource(pResourceManager, "swap chain buffer resource", 0, 0));

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
    DirectXResource::getBindedDescriptorCpuHandle(DirectXDescriptorType descriptorType) {
        std::scoped_lock guard(mtxHeapDescriptors.first);

        // Get descriptors of the specified type.
        const auto& descriptorsSameType = mtxHeapDescriptors.second[static_cast<size_t>(descriptorType)];

        // Get descriptor.
        const auto& pDescriptor = descriptorsSameType.pResource;

        // Quit if no such descriptor was binded.
        if (pDescriptor == nullptr) {
            return {};
        }

        // Get heap that this descriptor uses.
        const auto pHeap = pDescriptor->getDescriptorHeap();

        // Construct descriptor handle.
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            pHeap->getInternalHeap()->GetCPUDescriptorHandleForHeapStart(),
            pDescriptor->getDescriptorOffsetInDescriptors(),
            pHeap->getDescriptorSize());
    }

    std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> DirectXResource::getBindedCubemapFaceDescriptorCpuHandle(
        DirectXDescriptorType descriptorType, size_t iCubemapFaceIndex) {
        std::scoped_lock guard(mtxHeapDescriptors.first);

        // Get descriptors of the specified type.
        const auto& descriptorsSameType = mtxHeapDescriptors.second[static_cast<size_t>(descriptorType)];

        // Make sure the specified index is not out of bounds.
        if (iCubemapFaceIndex >= descriptorsSameType.vCubemapFaces.size()) [[unlikely]] {
            Error error(std::format(
                "unable to get binded descriptor for resource \"{}\" because the specified descriptor index "
                "{} is out of bounds",
                getResourceName(),
                iCubemapFaceIndex));
        }

        // Get descriptor.
        const auto& pDescriptor = descriptorsSameType.vCubemapFaces[iCubemapFaceIndex];

        // Quit if no such descriptor was binded.
        if (pDescriptor == nullptr) {
            return {};
        }

        // Get heap that this descriptor uses.
        const auto pHeap = pDescriptor->getDescriptorHeap();

        // Construct descriptor handle.
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            pHeap->getInternalHeap()->GetCPUDescriptorHandleForHeapStart(),
            pDescriptor->getDescriptorOffsetInDescriptors(),
            pHeap->getDescriptorSize());
    }

    std::optional<D3D12_GPU_DESCRIPTOR_HANDLE>
    DirectXResource::getBindedDescriptorGpuHandle(DirectXDescriptorType descriptorType) {
        std::scoped_lock guard(mtxHeapDescriptors.first);

        // Get descriptors of the specified type.
        const auto& descriptorsSameType = mtxHeapDescriptors.second[static_cast<size_t>(descriptorType)];

        // Get descriptor.
        const auto& pDescriptor = descriptorsSameType.pResource;

        // Quit if no such descriptor was binded.
        if (pDescriptor == nullptr) {
            return {};
        }

        // Get heap that this descriptor uses.
        const auto pHeap = pDescriptor->getDescriptorHeap();

        // Construct descriptor handle.
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(
            pHeap->getInternalHeap()->GetGPUDescriptorHandleForHeapStart(),
            pDescriptor->getDescriptorOffsetInDescriptors(),
            pHeap->getDescriptorSize());
    }

    DirectXDescriptor* DirectXResource::getDescriptor(DirectXDescriptorType descriptorType) {
        std::scoped_lock guard(mtxHeapDescriptors.first);

        // Get descriptors of the specified type.
        const auto& descriptorsSameType = mtxHeapDescriptors.second[static_cast<size_t>(descriptorType)];

        return descriptorsSameType.pResource.get();
    }

    DirectXResource::DirectXResource(
        DirectXResourceManager* pResourceManager,
        const std::string& sResourceName,
        UINT iElementSizeInBytes,
        UINT iElementCount)
        : GpuResource(pResourceManager, sResourceName, iElementSizeInBytes, iElementCount) {
        // Initialize descriptor pointers (just in case).
        for (auto& descriptorsSameType : mtxHeapDescriptors.second) {
            descriptorsSameType.pResource = nullptr;
            for (auto& pDescriptor : descriptorsSameType.vCubemapFaces) {
                pDescriptor = nullptr;
            }
        }
    }

    DirectXResource::~DirectXResource() {
        // Don't log here to avoid spamming.

        // Make sure the GPU is not using this resource.
        getResourceManager()->getRenderer()->waitForGpuToFinishWorkUpToThisPoint();
    }

    std::optional<Error> DirectXResource::bindDescriptor(
        DirectXDescriptorType descriptorType,
        ContinuousDirectXDescriptorRange* pRange,
        bool bBindDescriptorsToCubemapFaces) {
        std::scoped_lock guard(mtxHeapDescriptors.first);

        if (getDescriptor(descriptorType) != nullptr) {
            // Nothing to do, already initialized.
            return {};
        }

        // Get resource manager.
        const auto pResourceManager = dynamic_cast<DirectXResourceManager*>(getResourceManager());
        if (pResourceManager == nullptr) [[unlikely]] {
            return Error("invalid resource manager");
        }

        // Pick the appropriate heap.
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
        auto optionalError =
            pHeap->assignDescriptor(this, descriptorType, pRange, bBindDescriptorsToCubemapFaces);
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }
} // namespace ne
