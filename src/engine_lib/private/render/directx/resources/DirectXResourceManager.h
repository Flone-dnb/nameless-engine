#pragma once

// Standard.
#include <memory>
#include <variant>

// Custom.
#include "misc/Error.h"

// External.
#include "directx/d3dx12.h"
#include "D3D12MemoryAllocator/include/D3D12MemAlloc.h"

// OS.
#include <wrl.h>

namespace ne {
    using namespace Microsoft::WRL;

    class DirectXResource;
    class DirectXDescriptorHeap;
    class DirectXRenderer;

    /**
     * Controls resource creation and handles resource descriptors.
     */
    class DirectXResourceManager {
    public:
        DirectXResourceManager() = delete;
        DirectXResourceManager(const DirectXResourceManager&) = delete;
        DirectXResourceManager& operator=(const DirectXResourceManager&) = delete;

        /**
         * Creates a new resource manager.
         *
         * @param pRenderer     DirectX renderer.
         * @param pDevice       D3D device.
         * @param pVideoAdapter GPU to use.
         *
         * @return Error if something went wrong, otherwise created resource manager.
         */
        static std::variant<std::unique_ptr<DirectXResourceManager>, Error>
        create(DirectXRenderer* pRenderer, ID3D12Device* pDevice, IDXGIAdapter3* pVideoAdapter);

        /**
         * Returns total video memory size (VRAM) in megabytes.
         *
         * @return Total video memory size in megabytes.
         */
        size_t getTotalVideoMemoryInMb() const;

        /**
         * Returns used video memory size (VRAM) in megabytes.
         *
         * @return Used video memory size in megabytes.
         */
        size_t getUsedVideoMemoryInMb() const;

        /**
         * Creates a new resource and binds a render target view descriptor to it.
         *
         * @param sResourceName        Resource name, used for logging.
         * @param allocationDesc       Allocation description.
         * @param resourceDesc         Resource description.
         * @param initialResourceState Initial resource state.
         * @param resourceClearValue   Optimized clear value.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        std::variant<std::unique_ptr<DirectXResource>, Error> createRtvResource(
            const std::string& sResourceName,
            const D3D12MA::ALLOCATION_DESC& allocationDesc,
            const D3D12_RESOURCE_DESC& resourceDesc,
            const D3D12_RESOURCE_STATES& initialResourceState,
            const D3D12_CLEAR_VALUE& resourceClearValue) const;

        /**
         * Creates a new resource and binds a depth stencil view descriptor to it.
         *
         * @param sResourceName        Resource name, used for logging.
         * @param allocationDesc       Allocation description.
         * @param resourceDesc         Resource description.
         * @param initialResourceState Initial resource state.
         * @param resourceClearValue   Optimized clear value.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        std::variant<std::unique_ptr<DirectXResource>, Error> createDsvResource(
            const std::string& sResourceName,
            const D3D12MA::ALLOCATION_DESC& allocationDesc,
            const D3D12_RESOURCE_DESC& resourceDesc,
            const D3D12_RESOURCE_STATES& initialResourceState,
            const D3D12_CLEAR_VALUE& resourceClearValue) const;

        /**
         * Creates a new resource and binds a constant buffer view descriptor to it.
         *
         * @param sResourceName        Resource name, used for logging.
         * @param allocationDesc       Allocation description.
         * @param resourceDesc         Resource description.
         * @param initialResourceState Initial resource state.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        std::variant<std::unique_ptr<DirectXResource>, Error> createCbvResource(
            const std::string& sResourceName,
            const D3D12MA::ALLOCATION_DESC& allocationDesc,
            const D3D12_RESOURCE_DESC& resourceDesc,
            const D3D12_RESOURCE_STATES& initialResourceState) const;

        /**
         * Creates a new resource and binds a shader resource view descriptor to it.
         *
         * @param sResourceName        Resource name, used for logging.
         * @param allocationDesc       Allocation description.
         * @param resourceDesc         Resource description.
         * @param initialResourceState Initial resource state.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        std::variant<std::unique_ptr<DirectXResource>, Error> createSrvResource(
            const std::string& sResourceName,
            const D3D12MA::ALLOCATION_DESC& allocationDesc,
            const D3D12_RESOURCE_DESC& resourceDesc,
            const D3D12_RESOURCE_STATES& initialResourceState) const;

        /**
         * Creates a new resource and binds an unordered access view descriptor to it.
         *
         * @param sResourceName        Resource name, used for logging.
         * @param allocationDesc       Allocation description.
         * @param resourceDesc         Resource description.
         * @param initialResourceState Initial resource state.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        std::variant<std::unique_ptr<DirectXResource>, Error> createUavResource(
            const std::string& sResourceName,
            const D3D12MA::ALLOCATION_DESC& allocationDesc,
            const D3D12_RESOURCE_DESC& resourceDesc,
            const D3D12_RESOURCE_STATES& initialResourceState) const;

        /**
         * Creates new resources by wrapping swap chain buffers and binding
         * a RTV descriptor to each buffer.
         *
         * @param pSwapChain Swap chain.
         * @param iSwapChainBufferCount Number of buffers swap chain has.
         *
         * @return Error if something went wrong, otherwise created resources.
         */
        std::variant<std::vector<std::unique_ptr<DirectXResource>>, Error>
        makeRtvResourcesFromSwapChainBuffer(
            IDXGISwapChain3* pSwapChain, unsigned int iSwapChainBufferCount) const;

        /**
         * Returns RTV heap.
         *
         * @return RTV heap. Do not delete returned pointer.
         */
        DirectXDescriptorHeap* getRtvHeap() const;

        /**
         * Returns DSV heap.
         *
         * @return DSV heap. Do not delete returned pointer.
         */
        DirectXDescriptorHeap* getDsvHeap() const;

        /**
         * Returns CBV/SRV/UAV heap.
         *
         * @return CBV/SRV/UAV heap. Do not delete returned pointer.
         */
        DirectXDescriptorHeap* getCbvSrvUavHeap() const;

    private:
        /**
         * Constructor.
         *
         * @param pMemoryAllocator Created memory allocator to use.
         * @param pRtvHeap         Created RTV heap manager.
         * @param pDsvHeap         Created DSV heap manager.
         * @param pCbvSrvUavHeap   Created CBV/SRV/UAV heap.
         */
        DirectXResourceManager(
            ComPtr<D3D12MA::Allocator>&& pMemoryAllocator,
            std::unique_ptr<DirectXDescriptorHeap>&& pRtvHeap,
            std::unique_ptr<DirectXDescriptorHeap>&& pDsvHeap,
            std::unique_ptr<DirectXDescriptorHeap>&& pCbvSrvUavHeap);

        /** Allocator for GPU resources. */
        ComPtr<D3D12MA::Allocator> pMemoryAllocator;

        /** RTV heap manager. */
        std::unique_ptr<DirectXDescriptorHeap> pRtvHeap;
        /** DSV heap manager. */
        std::unique_ptr<DirectXDescriptorHeap> pDsvHeap;
        /** CBV/SRV/UAV heap manager. */
        std::unique_ptr<DirectXDescriptorHeap> pCbvSrvUavHeap;
    };
} // namespace ne
