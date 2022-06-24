#pragma once

// STL.
#include <optional>
#include <variant>
#include <vector>
#include <mutex>

// Custom.
#include "render/IRenderer.h"
#include "misc/Error.h"

// DirectX.
#include <dxgi1_4.h>
#include "directx/d3dx12.h"

// External.
#include "D3D12MemoryAllocator/include/D3D12MemAlloc.h"

// OS.
#include <wrl.h>

namespace ne {
    using namespace Microsoft::WRL;

    class Game;

    /**
     * DirectX 12 renderer.
     */
    class DirectXRenderer : public IRenderer {
    public:
        /**
         * Initializes renderer base entities.
         *
         * @param pGame   Game object that owns this renderer.
         */
        DirectXRenderer(Game* pGame);
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
        virtual std::variant<std::vector<std::string>, Error> getSupportedGpus() const override;

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
        virtual std::string getCurrentlyUsedGpuName() const override;

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

        /**
         * Returns static texture samplers, used in texture filtering.
         *
         * @return Array of static texture samplers.
         */
        static std::array<const CD3DX12_STATIC_SAMPLER_DESC, 4> getStaticTextureSamplers();

    protected:
        friend class HlslShader;

        /** Update internal resources for next frame. */
        virtual void update() override;

        /** Draw new frame. */
        virtual void drawFrame() override;

        /**
         * Enables DX debug layer.
         *
         * @warning Should be called before you create DXGI factory or D3D device.
         *
         * @remark Only enables debug layer if DEBUG macro is defined.
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
        std::optional<Error> setVideoAdapter(const std::string& sVideoAdapterName);

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
         * Compiles all essential shaders that the engine will use.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> compileEngineShaders() const;

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
        /** DXGI Factory. */
        ComPtr<IDXGIFactory4> pFactory;
        /** D3D12 Device. */
        ComPtr<ID3D12Device> pDevice;
        /** GPU. */
        ComPtr<IDXGIAdapter3> pVideoAdapter;
        /** Monitor. */
        ComPtr<IDXGIOutput> pOutputAdapter;
        /** Swap chain. */
        ComPtr<IDXGISwapChain1> pSwapChain;

        // Command objects.
        /** GPU command queue. */
        ComPtr<ID3D12CommandQueue> pCommandQueue;
        /** Command list allocator - stores commands from command list (works like std::vector). */
        ComPtr<ID3D12CommandAllocator> pCommandListAllocator;
        /** CPU command list. */
        ComPtr<ID3D12GraphicsCommandList> pCommandList;

        /** Allocator for GPU resources. */
        ComPtr<D3D12MA::Allocator> pMemoryAllocator;

        /** Fence object. */
        ComPtr<ID3D12Fence> pFence;
        /** Fence counter. */
        std::atomic<UINT64> iCurrentFence{0};

        /** Render target view descriptor heap. */
        ComPtr<ID3D12DescriptorHeap> pRtvHeap;
        /** Depth stencil view descriptor heap. */
        ComPtr<ID3D12DescriptorHeap> pDsvHeap;
        /** Constant buffer view, shader resource view and unordered access view descriptor heaps. */
        ComPtr<ID3D12DescriptorHeap> pCbvSrvUavHeap;
        /** Render target view descriptor size. */
        UINT iRtvDescriptorSize = 0;
        /** Depth stencil view descriptor size. */
        UINT iDsvDescriptorSize = 0;
        /** Constant buffer view, shader resource view and unordered access view descriptor size. */
        UINT iCbvSrvUavDescriptorSize = 0;

        // Render/depth buffers.
        /** Index of the current back buffer. */
        int iCurrentBackBufferIndex = 0;
        /** Lock when reading or writing to render resources. */
        std::recursive_mutex mtxRwRenderResources;
        /** Swap chain buffer. */
        ComPtr<ID3D12Resource> pSwapChainBuffer[getSwapChainBufferCount()];
        /** Depth stencil buffer. */
        ComPtr<D3D12MA::Allocation> pDepthStencilBuffer;
        /** Default back buffer fill color. */
        float backBufferFillColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

        // Buffer formats.
        /** Back buffer format. */
        DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        /** Depth/stencil format. */
        DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        // MSAA.
        /** Render target for MSAA rendering. */
        ComPtr<D3D12MA::Allocation> pMsaaRenderTarget;
        /** Whether the MSAA is currently enabled or not. */
        bool bIsMsaaEnabled = true;
        /** Current MSAA sample count (quality). */
        UINT iMsaaSampleCount = 4;
        /** Supported quality level for MSAA. */
        UINT iMsaaQuality = 0;

        // Display mode.
        /** Currently used display mode. */
        DXGI_MODE_DESC currentDisplayMode{};
        /** Use only display modes that use this scaling. */
        DXGI_MODE_SCALING usedScaling = DXGI_MODE_SCALING_UNSPECIFIED;
        /** Whether the VSync is currently enabled or not. */
        bool bIsVSyncEnabled = false;

        // Viewport.
        /** Screen viewport size and depth range. */
        D3D12_VIEWPORT screenViewport;
        /** Scissor rectangle for viewport. */
        D3D12_RECT scissorRect;
        /** Minimum value for depth. */
        const float fMinDepth = 0.0f;
        /** Maximum value for depth. */
        const float fMaxDepth = 1.0f;

        // Video Adapters (GPUs).
        /** Preferred GPU to use (for @ref getSupportedGpus). Can be overriden by configuration from disk. */
        long iPreferredGpuIndex = 0;
        /** Name of the GPU we are currently using. */
        std::string sUsedVideoAdapter;

        /** Will be 'true' if we read the configuration from disk at startup. */
        bool bStartedWithConfigurationFromDisk = false;

        /** Used to create a critical section for read/write operation on config files. */
        std::mutex mtxRwConfigFile;

        /** D3D feature level that we use (required feature level). */
        const D3D_FEATURE_LEVEL rendererD3dFeatureLevel = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_12_1;
    };
} // namespace ne