#pragma once

// STL.
#include <optional>

// Custom.
#include "render/IRenderer.h"
#include "misc/Error.h"

// DirectX.
#include "d3d12.h"
#include <dxgi1_4.h>

// OS.
#include <wrl.h>

namespace ne {
    /**
     * DirectX 12 renderer.
     */
    class DirectXRenderer : public IRenderer {
    public:
        DirectXRenderer();

        DirectXRenderer(const DirectXRenderer &) = delete;
        DirectXRenderer &operator=(const DirectXRenderer &) = delete;

        virtual ~DirectXRenderer() override {}

        virtual void update() override;
        virtual void drawFrame() override;

    private:
        /**
         * Enables DX debug layer.
         *
         * @warning Should be called before you create DXGI factory or D3D device.
         *
         * @return Returns an error if something went wrong.
         */
        std::optional<Error> enableDebugLayer() const;

        /**
         * Tries to find a display adapter that supports used D3D version and feature level.
         *
         * @param pOutAdapter   Found supported display adapter.
         *
         * @return Error if can't find supported display adapter.
         */
        std::optional<Error> getFirstSupportedAdapter(IDXGIAdapter3 *&pOutAdapter) const;

        std::optional<Error> checkMsaaSupport();

        // ----------------------------------------------------------------------------------------

        // Main DX objects.
        Microsoft::WRL::ComPtr<IDXGIFactory4> pFactory;
        Microsoft::WRL::ComPtr<ID3D12Device> pDevice;
        Microsoft::WRL::ComPtr<IDXGIAdapter3> pAdapter;
        Microsoft::WRL::ComPtr<IDXGIOutput> pOutput;
        Microsoft::WRL::ComPtr<IDXGISwapChain1> pSwapChain;

        // Fence.
        Microsoft::WRL::ComPtr<ID3D12Fence> pFence;
        UINT64 iCurrentFence = 0;

        // Descriptor heaps and descriptor sizes.
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pRtvHeap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDsvHeap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pCbvSrvUavHeap;
        UINT iRtvDescriptorSize = 0;
        UINT iDsvDescriptorSize = 0;
        UINT iCbvSrvUavDescriptorSize = 0;

        // Buffer formats.
        DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        // MSAA.
        Microsoft::WRL::ComPtr<ID3D12Resource> pMsaaRenderTarget;
        bool isMsaaEnabled = true;
        UINT iMsaaSampleCount = 4;
        UINT iMsaaQuality = 0;

        // ----------------------------------------------------------------------------------------

        /**
         * D3D feature level that we use (required feature level).
         */
        const D3D_FEATURE_LEVEL rendererD3dFeatureLevel = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_12_1;
    };
} // namespace ne
