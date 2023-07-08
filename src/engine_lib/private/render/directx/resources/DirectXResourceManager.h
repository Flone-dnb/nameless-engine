#pragma once

// Standard.
#include <memory>
#include <variant>

// Custom.
#include "misc/Error.h"
#include "render/directx/descriptors/DirectXDescriptorHeap.h"
#include "render/general/resources/GpuResourceManager.h"

// External.
#include "directx/d3dx12.h"
#include "D3D12MemoryAllocator/include/D3D12MemAlloc.h"

// OS.
#include <wrl.h>

namespace ne {
    using namespace Microsoft::WRL;

    class DirectXResource;
    class DirectXRenderer;
    class UploadBuffer;

    /**
     * Controls resource creation and handles resource descriptors.
     */
    class DirectXResourceManager : public GpuResourceManager {
    public:
        DirectXResourceManager() = delete;
        DirectXResourceManager(const DirectXResourceManager&) = delete;
        DirectXResourceManager& operator=(const DirectXResourceManager&) = delete;

        virtual ~DirectXResourceManager() override = default;

        /**
         * Creates a new resource manager.
         *
         * @param pRenderer DirectX renderer.
         *
         * @return Error if something went wrong, otherwise created resource manager.
         */
        static std::variant<std::unique_ptr<DirectXResourceManager>, Error>
        create(DirectXRenderer* pRenderer);

        /**
         * Creates a new GPU resource with available CPU access, typically used
         * for resources that needs to be frequently updated from the CPU side.
         *
         * Example:
         * @code
         * struct ObjectData{
         *     glm::mat4x4 world;
         * };
         *
         * auto result = pResourceManager->createResourceWithCpuAccess(
         *     "object constant data",
         *     sizeof(ObjectData),
         *     1,
         *     true);
         * @endcode
         *
         * @param sResourceName           Resource name, used for logging.
         * @param iElementSizeInBytes     Size of one buffer element in bytes.
         * @param iElementCount           Amount of elements in the resulting buffer.
         * @param bIsShaderConstantBuffer Determines whether this resource will be used as
         * a constant buffer in shaders or not. Specify `true` if you plan to use this resource
         * as a constant buffer in shaders, this will result in element size being padded to
         * be a multiple of 256 because of the hardware requirement for shader constant buffers.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        virtual std::variant<std::unique_ptr<UploadBuffer>, Error> createResourceWithCpuAccess(
            const std::string& sResourceName,
            size_t iElementSizeInBytes,
            size_t iElementCount,
            bool bIsShaderConstantBuffer) const override;

        /**
         * Creates a new GPU resource and fills it with the specified data.
         *
         * Example:
         * @code
         * std::vector<glm::vec3> vVertices;
         *
         * auto result = pResourceManager->createResourceWithData(
         *     "mesh vertex buffer",
         *     vVertices.data(),
         *     vVertices.size() * sizeof(glm::vec3),
         *     true);
         * @endcode
         *
         * @param sResourceName         Resource name, used for logging.
         * @param pBufferData           Pointer to the data that the new resource will contain.
         * @param iDataSizeInBytes      Size in bytes of the data (resource size).
         * @param bAllowUnorderedAccess Whether the new resource allows unordered access or not.
         *
         * @return Error if something went wrong, otherwise created resource with filled data.
         */
        virtual std::variant<std::unique_ptr<GpuResource>, Error> createResourceWithData(
            const std::string& sResourceName,
            const void* pBufferData,
            size_t iDataSizeInBytes,
            bool bAllowUnorderedAccess) const override;

        /**
         * Returns total video memory size (VRAM) in megabytes.
         *
         * @return Total video memory size in megabytes.
         */
        virtual size_t getTotalVideoMemoryInMb() const override;

        /**
         * Returns the amount of video memory (VRAM) occupied by all currently allocated resources.
         *
         * @return Size of the video memory used by allocated resources.
         */
        virtual size_t getUsedVideoMemoryInMb() const override;

        /**
         * Creates a new GPU resource.
         *
         * @param sResourceName        Resource name, used for logging.
         * @param allocationDesc       Allocation description.
         * @param resourceDesc         Resource description.
         * @param initialResourceState Initial resource state.
         * @param resourceClearValue   Clear value for render target or depth/stencil resources.
         *
         * @return Error if something went wrong, otherwise created resource.
         */
        std::variant<std::unique_ptr<DirectXResource>, Error> createResource(
            const std::string& sResourceName,
            const D3D12MA::ALLOCATION_DESC& allocationDesc,
            const D3D12_RESOURCE_DESC& resourceDesc,
            const D3D12_RESOURCE_STATES& initialResourceState,
            const std::optional<D3D12_CLEAR_VALUE>& resourceClearValue) const;

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

        /**
         * Returns renderer that owns this resource manager.
         *
         * @return Do not delete returned pointer. Renderer.
         */
        DirectXRenderer* getRenderer() const;

    private:
        /**
         * Constructor.
         *
         * @param pRenderer        DirectX renderer.
         * @param pMemoryAllocator Created memory allocator to use.
         * @param pRtvHeap         Created RTV heap manager.
         * @param pDsvHeap         Created DSV heap manager.
         * @param pCbvSrvUavHeap   Created CBV/SRV/UAV heap.
         */
        DirectXResourceManager(
            DirectXRenderer* pRenderer,
            ComPtr<D3D12MA::Allocator>&& pMemoryAllocator,
            std::unique_ptr<DirectXDescriptorHeap>&& pRtvHeap,
            std::unique_ptr<DirectXDescriptorHeap>&& pDsvHeap,
            std::unique_ptr<DirectXDescriptorHeap>&& pCbvSrvUavHeap);

        /**
         * Modifies the value to be a multiple of 256.
         *
         * Example:
         * @code
         * const auto iResult = makeMultipleOf256(300);
         * assert(iResult == 512);
         * @endcode
         *
         * @param iNumber Value to make a multiple of 256.
         *
         * @return Resulting value.
         */
        static inline size_t makeMultipleOf256(size_t iNumber) {
            // We are adding 255 and then masking off
            // the lower 2 bytes which store all bits < 256.

            // Example: Suppose iNumber = 300.
            // (300 + 255) & ~255
            // 555 & ~255
            // 0x022B & ~0x00ff
            // 0x022B & 0xff00
            // 0x0200
            // 512
            return (iNumber + 255) & ~255; // NOLINT
        }

        /** Renderer that owns this manager. */
        DirectXRenderer* pRenderer = nullptr;

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
