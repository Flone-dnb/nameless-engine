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
        /**
         * Initializes renderer base entities.
         */
        DirectXRenderer();

        DirectXRenderer(const DirectXRenderer &) = delete;
        DirectXRenderer &operator=(const DirectXRenderer &) = delete;

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
        std::optional<Error> setVideoAdapter(const std::wstring &sVideoAdapterName);

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
         * Checks if the created device supports MSAA.
         *
         * @return Error if something went wrong, for example, if device does not support MSAA.
         */
        std::optional<Error> checkMsaaSupport();

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
        Microsoft::WRL::ComPtr<IDXGIFactory4> pFactory;
        /* D3D12 Device */
        Microsoft::WRL::ComPtr<ID3D12Device> pDevice;
        /* GPU */
        Microsoft::WRL::ComPtr<IDXGIAdapter3> pVideoAdapter;
        /* Monitor */
        Microsoft::WRL::ComPtr<IDXGIOutput> pOutputAdapter;
        /* Swap chain */
        Microsoft::WRL::ComPtr<IDXGISwapChain1> pSwapChain;

        // Command objects.
        /* GPU command queue. */
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> pCommandQueue;
        /* Command list allocator - stores commands from command list (works like std::vector). */
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCommandListAllocator;
        /* CPU command list. */
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList;

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
        bool bIsMsaaEnabled = true;
        UINT iMsaaSampleCount = 4;
        UINT iMsaaQuality = 0;

        // Display mode.
        DXGI_MODE_DESC currentDisplayMode;
        /** Only display modes with this scaling are supported. */
        DXGI_MODE_SCALING usedScaling = DXGI_MODE_SCALING_UNSPECIFIED;

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
