﻿#pragma once

// STL.
#include <optional>
#include <variant>
#include <vector>
#include <mutex>

// Custom.
#include "render/IRenderer.h"
#include "misc/Error.h"

// DirectX.
#include "d3d12.h"
#include <dxgi1_4.h>
#include "DirectXHelpers/d3dx12.h"

// OS.
#include <wrl.h>
using namespace Microsoft::WRL;

// External.
#include "D3D12MemoryAllocator/D3D12MemAlloc.h"

namespace ne {
    class Window;
    /**
     * DirectX 12 renderer.
     */
    class DirectXRenderer : public IRenderer {
    public:
        /**
         * Initializes renderer base entities.
         *
         * @param pWindow Window that we will render to.
         */
        DirectXRenderer(Window* pWindow);
        DirectXRenderer() = delete;
        DirectXRenderer(const DirectXRenderer&) = delete;
        DirectXRenderer& operator=(const DirectXRenderer&) = delete;

        virtual ~DirectXRenderer() override {}

        /**
         * Looks for video adapters (GPUs) that support used DirectX version and feature level.
         *
         * @return Error if can't find any GPU that supports our DirectX version and feature level,
         * vector with GPU names if successful.
         */
        virtual std::variant<std::vector<std::wstring>, Error> getSupportedGpus() const override;

        /**
         * Returns backbuffer resolution.
         *
         * @return A pair of X and Y pixels.
         */
        virtual std::pair<int, int> getRenderResolution() const override;

        /**
         * Returns a list of supported render resolution.
         *
         * @return Error if something went wrong, otherwise render mode.
         */
        virtual std::variant<std::vector<RenderMode>, Error> getSupportedRenderResolutions() const override;

        /**
         * Returns the name of the GPU that is being currently used.
         *
         * @return Name of the GPU.
         */
        virtual std::wstring getCurrentlyUsedGpuName() const override;

        /**
         * Returns currently used AA settings.
         *
         * @return AA settings.
         */
        virtual Antialiasing getCurrentAntialiasing() const override;

        /**
         * Returns total video memory size (VRAM) in megabytes.
         *
         * @return Total video memory size in megabytes.
         */
        virtual size_t getTotalVideoMemoryInMb() const override;

        /**
         * Returns used video memory size (VRAM) in megabytes.
         *
         * @return Used video memory size in megabytes.
         */
        virtual size_t getUsedVideoMemoryInMb() const override;

        /**
         * Sets backbuffer color.
         *
         * @param fillColor Color that the backbuffer will be filled with before rendering a frame.
         * 4 values correspond to RGBA parameters.
         *
         * @return Error if something went wrong.
         */
        virtual std::optional<Error> setBackbufferFillColor(float fillColor[4]) override;

    protected:
        /** Update internal resources for next frame. */
        virtual void update() override;

        /** Draw new frame. */
        virtual void drawFrame() override;

        /**
         * Enables DX debug layer.
         *
         * @warning Should be called before you create DXGI factory or D3D device.
         *
         * @return Returns an error if something went wrong.
         */
        std::optional<Error> enableDebugLayer() const;

        /**
         * Sets the video adapter to be used.
         *
         * @param sVideoAdapterName Name of the video adapter to use.
         * You can query supported video adapters by using @ref getSupportedGpus
         *
         * @return Error if something went wrong, for example, if an adapter with the specified
         * name was not found, or if it was found but does not support used DirectX version
         * or feature level.
         */
        std::optional<Error> setVideoAdapter(const std::wstring& sVideoAdapterName);

        /**
         * Sets first found output adapter (monitor).
         *
         * @return Error if something went wrong or no output adapter was found.
         */
        std::optional<Error> setOutputAdapter();

        /**
         * Creates and initializes Command Queue, Command List and Command List Allocator.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> createCommandObjects();

        /**
         * Creates memory allocator (external helper class).
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> createMemoryAllocator();

        /**
         * Creates and initializes the swap chain.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> createSwapChain();

        /**
         * Creates and initializes the RTV and DSV heaps.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> createRtvAndDsvDescriptorHeaps();

        /**
         * Checks if the created device supports MSAA.
         *
         * @return Error if something went wrong, for example, if device does not support MSAA.
         */
        std::optional<Error> checkMsaaSupport();

        /**
         * Initializes DirectX.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> initializeDirectX();

        /**
         * Creates root signature (defines the resources the shader programs expect).
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> createRootSignature();

        /**
         * Returns static texture samplers, used in texture filtering.
         *
         * @return Array of static texture samplers.
         */
        static std::array<const CD3DX12_STATIC_SAMPLER_DESC, 4> getStaticTextureSamplers();

        /**
         * Recreates all render buffers to match current display mode (width/height).
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> resizeRenderBuffers();

        /**
         * Blocks until the GPU finishes executing all queued commands.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> flushCommandQueue();

        /**
         * Returns a vector of display modes that the current output adapter
         * supports for current back buffer format.
         *
         * @return Error if something went wrong, vector of display modes otherwise.
         */
        std::variant<std::vector<DXGI_MODE_DESC>, Error> getSupportedDisplayModes() const;

        /**
         * Writes current renderer configuration to disk.
         */
        virtual void writeConfigurationToConfigFile() override;

        /**
         * Reads renderer configuration from disk.
         */
        virtual void readConfigurationFromConfigFile() override;

    private:
        // Main DX objects.
        /* DXGI Factory */
        ComPtr<IDXGIFactory4> pFactory;
        /* D3D12 Device */
        ComPtr<ID3D12Device> pDevice;
        /* GPU */
        ComPtr<IDXGIAdapter3> pVideoAdapter;
        /* Monitor */
        ComPtr<IDXGIOutput> pOutputAdapter;
        /* Swap chain */
        ComPtr<IDXGISwapChain1> pSwapChain;

        // Command objects.
        /* GPU command queue. */
        ComPtr<ID3D12CommandQueue> pCommandQueue;
        /* Command list allocator - stores commands from command list (works like std::vector). */
        ComPtr<ID3D12CommandAllocator> pCommandListAllocator;
        /* CPU command list. */
        ComPtr<ID3D12GraphicsCommandList> pCommandList;

        // Allocator for GPU resources.
        ComPtr<D3D12MA::Allocator> pMemoryAllocator;

        // Shader related.
        ComPtr<ID3D12RootSignature> pRootSignature;

        // Fence.
        ComPtr<ID3D12Fence> pFence;
        std::mutex mtxRwFence;
        UINT64 iCurrentFence = 0;

        // Descriptor heaps and descriptor sizes.
        ComPtr<ID3D12DescriptorHeap> pRtvHeap;
        ComPtr<ID3D12DescriptorHeap> pDsvHeap;
        ComPtr<ID3D12DescriptorHeap> pCbvSrvUavHeap;
        UINT iRtvDescriptorSize = 0;
        UINT iDsvDescriptorSize = 0;
        UINT iCbvSrvUavDescriptorSize = 0;

        // Render/depth buffers.
        int iCurrentBackBufferIndex = 0;
        /** Lock when reading or writing to render resources. */
        std::recursive_mutex mtxRwRenderResources;
        ComPtr<ID3D12Resource> pSwapChainBuffer[getSwapChainBufferCount()];
        ComPtr<D3D12MA::Allocation> pDepthStencilBuffer;
        float backBufferFillColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

        // Buffer formats.
        DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        // MSAA.
        ComPtr<D3D12MA::Allocation> pMsaaRenderTarget;
        bool bIsMsaaEnabled = true;
        UINT iMsaaSampleCount = 4;
        UINT iMsaaQuality = 0;

        // Display mode.
        DXGI_MODE_DESC currentDisplayMode{};
        /** Use only display modes that use this scaling. */
        DXGI_MODE_SCALING usedScaling = DXGI_MODE_SCALING_UNSPECIFIED;
        bool bIsVSyncEnabled = false;

        // Viewport.
        D3D12_VIEWPORT screenViewport;
        D3D12_RECT scissorRect;
        float fMinDepth = 0.0f;
        float fMaxDepth = 1.0f;

        // Video Adapters (GPUs).
        long iPreferredGpuIndex = 0;
        std::vector<std::wstring> vSupportedVideoAdapters;
        std::wstring sUsedVideoAdapter;

        /** Will be 'true' if we read the configuration from disk at startup. */
        bool bStartedWithConfigurationFromDisk = false;

        /**
         * Used to create a critical section for read/write operation on config files.
         */
        std::mutex mtxRwConfigFile;

        // ----------------------------------------------------------------------------------------

        /**
         * D3D feature level that we use (required feature level).
         */
        const D3D_FEATURE_LEVEL rendererD3dFeatureLevel = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_12_1;
    };
} // namespace ne
