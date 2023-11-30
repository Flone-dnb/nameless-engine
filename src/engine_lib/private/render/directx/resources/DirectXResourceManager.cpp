#include "DirectXResourceManager.h"

// Custom.
#include "render/directx/resources/DirectXResource.h"
#include "render/directx/DirectXRenderer.h"
#include "render/general/resources/FrameResourcesManager.h"
#include "render/general/resources/UploadBuffer.h"
#include "render/directx/resources/DirectXFrameResource.h"

// External.
#include "DirectXTex/DDSTextureLoader/DDSTextureLoader12.h"

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
            err.addCurrentLocationToErrorStack();
            return err;
        }
        auto pRtvHeapManager = std::get<std::unique_ptr<DirectXDescriptorHeap>>(std::move(heapManagerResult));

        // Create DSV heap manager.
        heapManagerResult = DirectXDescriptorHeap::create(pRenderer, DescriptorHeapType::DSV);
        if (std::holds_alternative<Error>(heapManagerResult)) {
            Error err = std::get<Error>(std::move(heapManagerResult));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        auto pDsvHeapManager = std::get<std::unique_ptr<DirectXDescriptorHeap>>(std::move(heapManagerResult));

        // Create CBV/SRV/UAV heap manager.
        heapManagerResult = DirectXDescriptorHeap::create(pRenderer, DescriptorHeapType::CBV_SRV_UAV);
        if (std::holds_alternative<Error>(heapManagerResult)) {
            Error err = std::get<Error>(std::move(heapManagerResult));
            err.addCurrentLocationToErrorStack();
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

    std::variant<std::unique_ptr<GpuResource>, Error> DirectXResourceManager::loadTextureFromDisk(
        const std::string& sResourceName, const std::filesystem::path& pathToTextureFile) {
        // Make sure the specified path exists.
        if (!std::filesystem::exists(pathToTextureFile)) [[unlikely]] {
            return Error(
                std::format("the specified path \"{}\" does not exists", pathToTextureFile.string()));
        }

        // Make sure the specified path points to a file.
        if (std::filesystem::is_directory(pathToTextureFile)) [[unlikely]] {
            return Error(std::format(
                "expected the specified path \"{}\" to point to a file", pathToTextureFile.string()));
        }

        // Make sure the file has the ".DDS" extension.
        if (pathToTextureFile.extension() != ".DDS" && pathToTextureFile.extension() != ".dds") [[unlikely]] {
            return Error(std::format(
                "only DDS file extension is supported for texture loading, the path \"{}\" points to a "
                "non-DDS file",
                pathToTextureFile.string()));
        }

        // Get renderer.
        const auto pDirectXRenderer = dynamic_cast<DirectXRenderer*>(getRenderer());
        if (pDirectXRenderer == nullptr) [[unlikely]] {
            return Error("expected a DirectX renderer");
        }

        // Load DDS file as a new resource.
        ComPtr<ID3D12Resource> pCreatedResource;
        std::unique_ptr<uint8_t[]> pDdsData;
        std::vector<D3D12_SUBRESOURCE_DATA> vSubresources;
        const auto hResult = DirectX::LoadDDSTextureFromFile(
            pDirectXRenderer->getD3dDevice(),
            pathToTextureFile.wstring().c_str(),
            pCreatedResource.GetAddressOf(),
            pDdsData,
            vSubresources
            // max size argument
            // ... output arguments ...
        );
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        // Since DDSTextureLoader does not use our memory allocator we will just use created resource's
        // description to create a new resource using memory allocator and delete the resource that
        // DDSTextureLoader created.
        const auto finalResourceDescription = pCreatedResource->GetDesc();

        // Prepare upload resource description.
        const UINT64 iUploadBufferSize =
            GetRequiredIntermediateSize(pCreatedResource.Get(), 0, static_cast<UINT>(vSubresources.size()));
        const auto uploadResourceDescription = CD3DX12_RESOURCE_DESC::Buffer(iUploadBufferSize);

        // Create resource.
        return createResourceWithData(
            sResourceName, finalResourceDescription, vSubresources, uploadResourceDescription, true);
    }

    std::variant<std::unique_ptr<UploadBuffer>, Error>
    DirectXResourceManager::createResourceWithCpuWriteAccess(
        const std::string& sResourceName,
        size_t iElementSizeInBytes,
        size_t iElementCount,
        std::optional<bool> isUsedInShadersAsArrayResource) {
        if (isUsedInShadersAsArrayResource.has_value() && !isUsedInShadersAsArrayResource.value()) {
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
            {},
            iElementSizeInBytes,
            iElementCount);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        return std::unique_ptr<UploadBuffer>(
            new UploadBuffer(std::move(pResource), iElementSizeInBytes, iElementCount));
    }

    std::variant<std::unique_ptr<GpuResource>, Error> DirectXResourceManager::createResourceWithData(
        const std::string& sResourceName,
        const void* pBufferData,
        size_t iElementSizeInBytes,
        size_t iElementCount,
        ResourceUsageType usage,
        bool bIsShaderReadWriteResource) {
        // Calculate final resource size.
        const auto iDataSizeInBytes = iElementSizeInBytes * iElementCount;

        // Prepare final resource description.
        const auto finalResourceDescription = CD3DX12_RESOURCE_DESC::Buffer(
            iDataSizeInBytes,
            bIsShaderReadWriteResource ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                                       : D3D12_RESOURCE_FLAG_NONE);

        // Prepare subresource to copy.
        D3D12_SUBRESOURCE_DATA subResourceData = {};
        subResourceData.pData = pBufferData;
        subResourceData.RowPitch = iDataSizeInBytes;
        subResourceData.SlicePitch = subResourceData.RowPitch;
        std::vector<D3D12_SUBRESOURCE_DATA> vSubresourcesToCopy = {subResourceData};

        // Prepare upload resource description.
        const auto uploadResourceDescription = CD3DX12_RESOURCE_DESC::Buffer(iDataSizeInBytes);

        // Create resource.
        return createResourceWithData(
            sResourceName,
            finalResourceDescription,
            vSubresourcesToCopy,
            uploadResourceDescription,
            false,
            iElementSizeInBytes,
            iElementCount);
    }

    std::variant<std::unique_ptr<GpuResource>, Error> DirectXResourceManager::createResource(
        const std::string& sResourceName,
        size_t iElementSizeInBytes,
        size_t iElementCount,
        ResourceUsageType usage,
        bool bIsShaderReadWriteResource) {
        // Calculate resource size.
        const auto iDataSizeInBytes = iElementSizeInBytes * iElementCount;

        // Prepare resource description.
        const auto resourceDescription = CD3DX12_RESOURCE_DESC::Buffer(
            iDataSizeInBytes,
            bIsShaderReadWriteResource ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                                       : D3D12_RESOURCE_FLAG_NONE);

        // Prepare allocation heap.
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        // Create resource.
        auto result = DirectXResource::create(
            this,
            sResourceName,
            pMemoryAllocator.Get(),
            allocationDesc,
            resourceDescription,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            {},
            iElementSizeInBytes,
            iElementCount);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        return std::get<std::unique_ptr<DirectXResource>>(std::move(result));
    }

    std::variant<std::unique_ptr<GpuResource>, Error> DirectXResourceManager::createTextureResource(
        const std::string& sResourceName,
        unsigned int iWidth,
        unsigned int iHeight,
        TextureResourceFormat format) {
        // Prepare resource description.
        const auto resourceDescription = CD3DX12_RESOURCE_DESC::Tex2D(
            convertTextureResourceFormatToDxFormat(format),
            iWidth,
            iHeight,
            1, // array size
            1, // mip levels
            1, // sample count
            0, // sample quality
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        // Prepare allocation heap.
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        // Create resource.
        auto result = DirectXResource::create(
            this,
            sResourceName,
            pMemoryAllocator.Get(),
            allocationDesc,
            resourceDescription,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            {},
            0,
            0);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        return std::get<std::unique_ptr<DirectXResource>>(std::move(result));
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

    std::variant<std::unique_ptr<DirectXResource>, Error> DirectXResourceManager::createResource(
        const std::string& sResourceName,
        const D3D12MA::ALLOCATION_DESC& allocationDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_RESOURCE_STATES& initialResourceState,
        const std::optional<D3D12_CLEAR_VALUE>& resourceClearValue) const {
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
            err.addCurrentLocationToErrorStack();
            return err;
        }

        return std::get<std::unique_ptr<DirectXResource>>(std::move(result));
    }

    std::variant<std::vector<std::unique_ptr<DirectXResource>>, Error>
    DirectXResourceManager::makeRtvResourcesFromSwapChainBuffer(
        IDXGISwapChain3* pSwapChain, unsigned int iSwapChainBufferCount) {
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
                err.addCurrentLocationToErrorStack();
                return err;
            }

            vCreatedResources[i] = std::get<std::unique_ptr<DirectXResource>>(std::move(result));
        }

        return vCreatedResources;
    }

    DirectXDescriptorHeap* DirectXResourceManager::getRtvHeap() const { return pRtvHeap.get(); }

    DirectXDescriptorHeap* DirectXResourceManager::getDsvHeap() const { return pDsvHeap.get(); }

    DirectXDescriptorHeap* DirectXResourceManager::getCbvSrvUavHeap() const { return pCbvSrvUavHeap.get(); }

    std::string DirectXResourceManager::getCurrentStateInfo() {
        // Allocate stats.
        wchar_t* pStats = nullptr;
        pMemoryAllocator->BuildStatsString(&pStats, 1);

        // Construct wstring.
        std::wstring sWStats(pStats);

        // `wstringToString` does not work on this string for some reason
        std::string sStats;
        sStats.resize(sWStats.size());
        for (size_t i = 0; i < sWStats.size(); i++) {
            sStats[i] = static_cast<char>(sWStats[i]);
        }

        // Free stats.
        pMemoryAllocator->FreeStatsString(pStats);

        return sStats;
    }

    DirectXResourceManager::DirectXResourceManager(
        DirectXRenderer* pRenderer,
        ComPtr<D3D12MA::Allocator>&& pMemoryAllocator,
        std::unique_ptr<DirectXDescriptorHeap>&& pRtvHeap,
        std::unique_ptr<DirectXDescriptorHeap>&& pDsvHeap,
        std::unique_ptr<DirectXDescriptorHeap>&& pCbvSrvUavHeap)
        : GpuResourceManager(pRenderer) {
        this->pMemoryAllocator = std::move(pMemoryAllocator);
        this->pRtvHeap = std::move(pRtvHeap);
        this->pDsvHeap = std::move(pDsvHeap);
        this->pCbvSrvUavHeap = std::move(pCbvSrvUavHeap);
    }

    DXGI_FORMAT DirectXResourceManager::convertTextureResourceFormatToDxFormat(TextureResourceFormat format) {
        switch (format) {
        case (TextureResourceFormat::R32G32_UINT): {
            return DXGI_FORMAT_R32G32_UINT;
            break;
        }
        case (TextureResourceFormat::SIZE): {
            Error error("invalid format");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
            break;
        }
        }

        static_assert(static_cast<size_t>(TextureResourceFormat::SIZE) == 1, "add new formats to convert");

        Error error("unhandled case");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

    std::variant<std::unique_ptr<GpuResource>, Error> DirectXResourceManager::createResourceWithData(
        const std::string& sResourceName,
        const D3D12_RESOURCE_DESC& finalResourceDescription,
        const std::vector<D3D12_SUBRESOURCE_DATA>& vSubresourcesToCopy,
        const D3D12_RESOURCE_DESC& uploadResourceDescription,
        bool bIsTextureResource,
        size_t iElementSizeInBytes,
        size_t iElementCount) {
        // In order to create a GPU resource with our data from the CPU
        // we have to do a few steps:
        // 1. Create a GPU resource with DEFAULT heap type (CPU read-only heap) AKA resulting resource.
        // 2. Create a GPU resource with UPLOAD heap type (CPU read-write heap) AKA upload resource.
        // 3. Copy our data from the CPU to the resulting resource by using the upload resource.
        // 4. Wait for GPU to finish copying data and delete the upload resource.

        // 1. Create the resulting resource.
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
        const auto initialFinalResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
        auto result = DirectXResource::create(
            this,
            sResourceName,
            pMemoryAllocator.Get(),
            allocationDesc,
            finalResourceDescription,
            initialFinalResourceState,
            {},
            iElementSizeInBytes,
            iElementCount);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        auto pResultingResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        // 2. Create the upload resource.
        allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        const auto initialUploadResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
        result = DirectXResource::create(
            this,
            fmt::format("upload resource for \"{}\"", sResourceName),
            pMemoryAllocator.Get(),
            allocationDesc,
            uploadResourceDescription,
            initialUploadResourceState,
            {},
            iElementSizeInBytes,
            iElementCount);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        auto pUploadResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        // Get renderer.
        const auto pRenderer = dynamic_cast<DirectXRenderer*>(getRenderer());
        if (pRenderer == nullptr) [[unlikely]] {
            return Error("expected a DirectX renderer");
        }

        // Stop rendering.
        std::scoped_lock renderingGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Get command allocator from the current frame resource.
        const auto pFrameResourcesManager = pRenderer->getFrameResourcesManager();
        const auto pMtxCurrentFrameResource = pFrameResourcesManager->getCurrentFrameResource();
        std::scoped_lock frameResourceGuard(pMtxCurrentFrameResource->first);

        // Convert frame resource.
        const auto pDirectXFrameResource =
            dynamic_cast<DirectXFrameResource*>(pMtxCurrentFrameResource->second.pResource);
        if (pDirectXFrameResource == nullptr) [[unlikely]] {
            return Error("expected a DirectX frame resource");
        }

        const auto pCommandList = pRenderer->getD3dCommandList();
        const auto pCommandQueue = pRenderer->getD3dCommandQueue();
        const auto pCommandAllocator = pDirectXFrameResource->pCommandAllocator.Get();

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
        UpdateSubresources(
            pCommandList,
            pResultingResource->getInternalResource(),
            pUploadResource->getInternalResource(),
            0,
            0,
            static_cast<UINT>(vSubresourcesToCopy.size()),
            vSubresourcesToCopy.data());

        // Determine final resource state.
        D3D12_RESOURCE_STATES finalResourceState =
            (finalResourceDescription.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0
                ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS
                : D3D12_RESOURCE_STATE_GENERIC_READ;
        if (bIsTextureResource) {
            finalResourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }

        // Queue resulting resource state change.
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
            pResultingResource->getInternalResource(), initialFinalResourceState, finalResourceState);
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
} // namespace ne
