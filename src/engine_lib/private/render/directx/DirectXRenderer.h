﻿#pragma once

// Standard.
#include <optional>
#include <variant>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>

// Custom.
#include "render/Renderer.h"
#include "misc/Error.h"
#include "render/directx/resources/DirectXResource.h"

// External.
#include "directx/d3dx12.h"

// OS.
#include <wrl.h>
#include <dxgi1_4.h>

namespace ne {
    using namespace Microsoft::WRL;

    class Game;
    class DirectXResourceManager;
    class Material;
    class CameraProperties;

    /**
     * DirectX 12 renderer.
     */
    class DirectXRenderer : public Renderer {
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

        virtual ~DirectXRenderer() override = default;

        /**
         * Returns static texture samplers, used in texture filtering.
         *
         * @return Array of static texture samplers.
         */
        static std::array<const CD3DX12_STATIC_SAMPLER_DESC, 4> getStaticTextureSamplers();

        /**
         * Looks for video adapters (GPUs) that support used DirectX version and feature level.
         *
         * @return Error if can't find any GPU that supports our DirectX version and feature level,
         * vector with GPU names if successful.
         */
        virtual std::variant<std::vector<std::string>, Error> getSupportedGpuNames() const override;

        /**
         * Returns a list of supported render resolution (pairs of width and height).
         *
         * @return Error if something went wrong, otherwise render mode.
         */
        virtual std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
        getSupportedRenderResolutions() const override;

        /**
         * Returns a list of supported screen refresh rates (pairs of numerator and denominator).
         *
         * @remark The list of supported refresh rates depends on the currently used GPU,
         * so if changing used GPU this list might return different values.
         *
         * @return Error if something went wrong, otherwise refresh rates.
         */
        virtual std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
        getSupportedRefreshRates() const override;

        /**
         * Returns the name of the GPU that is being currently used.
         *
         * @return Name of the GPU.
         */
        virtual std::string getCurrentlyUsedGpuName() const override;

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
         * Blocks the current thread until the GPU finishes executing all queued commands up to this point.
         */
        virtual void waitForGpuToFinishWorkUpToThisPoint() override;

        /**
         * Returns DirectX device.
         *
         * @return Do not delete (free) this pointer. Render's internal device.
         */
        ID3D12Device* getD3dDevice() const;

        /**
         * Returns DirectX command list.
         *
         * @return Do not delete (free) this pointer. Render's internal command list.
         */
        ID3D12GraphicsCommandList* getD3dCommandList();

        /**
         * Returns DirectX command queue.
         *
         * @return Do not delete (free) this pointer. Render's internal command queue.
         */
        ID3D12CommandQueue* getD3dCommandQueue();

        /**
         * Returns DirectX video adapter.
         *
         * @return Do not delete (free) this pointer. Video adapter.
         */
        IDXGIAdapter3* getVideoAdapter() const;

        /**
         * Returns used back buffer format.
         *
         * @return Back buffer format.
         */
        DXGI_FORMAT getBackBufferFormat() const;

        /**
         * Returns used depth/stencil buffer format.
         *
         * @return Depth/stencil buffer format.
         */
        DXGI_FORMAT getDepthStencilBufferFormat() const;

        /**
         * Returns supported quality level for MSAA.
         *
         * @return MSAA quality level.
         */
        UINT getMsaaQualityLevel() const;

    protected:
        /**
         * Update internal resources for the next frame.
         *
         * @param pCameraProperties Camera properties to use.
         */
        void updateResourcesForNextFrame(CameraProperties* pCameraProperties);

        /** Draw new frame. */
        virtual void drawNextFrame() override;

        /**
         * Recreates all render buffers to match current settings.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> updateRenderBuffers() override;

        /**
         * Tells whether the renderer is initialized or not
         * (whether it's safe to use renderer functionality or not).
         *
         * @return Whether the renderer is initialized or not.
         */
        virtual bool isInitialized() const override;

    private:
        /**
         * Enables DX debug layer.
         *
         * @warning Should be called before you create DXGI factory or D3D device.
         *
         * @remark Only enables debug layer if DEBUG macro is defined.
         *
         * @return Returns an error if something went wrong.
         */
        [[nodiscard]] static std::optional<Error> enableDebugLayer();

        /**
         * (Re)creates depth/stencil buffer.
         *
         * @remark Make sure that the old depth/stencil buffer (if was) is not used by the GPU.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> createDepthStencilBuffer() override;

        /**
         * Sets the video adapter to be used.
         *
         * @param sVideoAdapterName Name of the video adapter to use.
         * You can query supported video adapters by using @ref getSupportedGpuNames.
         *
         * @return Error if something went wrong, for example, if an adapter with the specified
         * name was not found, or if it was found but does not support used DirectX version
         * or feature level.
         */
        [[nodiscard]] std::optional<Error> setVideoAdapter(const std::string& sVideoAdapterName);

        /**
         * Sets first found output adapter (monitor).
         *
         * @return Error if something went wrong or no output adapter was found.
         */
        [[nodiscard]] std::optional<Error> setOutputAdapter();

        /**
         * Creates and initializes command queue.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createCommandQueue();

        /**
         * Creates and initializes command list.
         *
         * @remark Requires frame resources to be created.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createCommandList();

        /**
         * Creates and initializes the swap chain.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createSwapChain();

        /**
         * Checks if the created device supports MSAA.
         *
         * @return Error if something went wrong, for example, if device does not support MSAA.
         */
        [[nodiscard]] std::optional<Error> checkMsaaSupport();

        /**
         * Initializes DirectX.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> initializeDirectX();

        /**
         * Compiles all essential shaders that the engine will use.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> compileEngineShaders() const;

        /**
         * Setups everything for render commands to be recorded (calls @ref updateResourcesForNextFrame,
         * resets lists, binds RTV/DSV, etc.).
         *
         * @param pCameraProperties Camera properties to use.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> prepareForDrawingNextFrame(CameraProperties* pCameraProperties);

        /**
         * Does final logic in drawing next frame (closes lists, executes lists, etc.).
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> finishDrawingNextFrame();

        /**
         * Adds draw commands to command list to draw all mesh nodes that use the specified material
         *
         * @param pMaterial                  Material that mesh nodes use.
         * @param iCurrentFrameResourceIndex Index of the current frame resource.
         */
        void drawMeshNodes(Material* pMaterial, size_t iCurrentFrameResourceIndex);

        /**
         * Waits until the GPU has completed commands up to the specified fence point.
         *
         * @param iFenceToWaitFor Fence value to wait for.
         */
        void waitForFenceValue(UINT64 iFenceToWaitFor);

        /**
         * Takes the current frame resource and updates frame data constant buffer that it stores (by copying
         * new (up to date) constants to it).
         *
         * @param pCurrentFrameResource Current frame resource.
         * @param pCameraProperties     Camera properties to use.
         */
        void
        updateFrameConstantsBuffer(FrameResource* pCurrentFrameResource, CameraProperties* pCameraProperties);

        /**
         * Returns a vector of display modes that the current output adapter
         * supports for current back buffer format.
         *
         * @return Error if something went wrong, vector of display modes otherwise.
         */
        std::variant<std::vector<DXGI_MODE_DESC>, Error> getSupportedDisplayModes() const;

        /**
         * Returns current buffer to draw to.
         *
         * @return GPU resource.
         */
        DirectXResource* getCurrentBackBufferResource();

        /** DXGI Factory. */
        ComPtr<IDXGIFactory4> pFactory;

        /** D3D12 Device. */
        ComPtr<ID3D12Device> pDevice;

        /** GPU. */
        ComPtr<IDXGIAdapter3> pVideoAdapter;

        /** Monitor. */
        ComPtr<IDXGIOutput> pOutputAdapter;

        /** Swap chain. */
        ComPtr<IDXGISwapChain3> pSwapChain;

        /** GPU command queue. */
        ComPtr<ID3D12CommandQueue> pCommandQueue;

        /** Contains commands for the GPU. */
        ComPtr<ID3D12GraphicsCommandList> pCommandList;

        /** Fence object. */
        ComPtr<ID3D12Fence> pFence;

        /** Fence counter. */
        std::pair<std::recursive_mutex, UINT64> mtxCurrentFenceValue;

        /** Up to date frame-global constant data. */
        std::pair<std::mutex, FrameConstants> mtxFrameConstants;

        /** Swap chain buffer. */
        std::vector<std::unique_ptr<DirectXResource>> vSwapChainBuffers;

        /** Depth stencil buffer. */
        std::unique_ptr<DirectXResource> pDepthStencilBuffer;

        /** Default back buffer fill color. */
        float backBufferFillColor[4] = {0.0F, 0.0F, 0.0F, 1.0F};

        /** Render target for MSAA rendering. */
        std::unique_ptr<DirectXResource> pMsaaRenderBuffer;

        /** Supported quality level for MSAA. */
        UINT iMsaaQuality = 0;

        /** Screen viewport size and depth range. */
        D3D12_VIEWPORT screenViewport;

        /** Scissor rectangle for viewport. */
        D3D12_RECT scissorRect;

        /** Name of the GPU we are currently using. */
        std::string sUsedVideoAdapter;

        /**
         * Index of the picked GPU.
         *
         * @remark Although we have this value in screen render setting object, the value there
         * represents preferred GPU index which can change on the fly, while here we care about picked
         * GPU index (not preferred).
         */
        unsigned int iPickedGpuIndex = 0;

        /** Whether @ref initializeDirectX was finished without errors or not. */
        bool bIsDirectXInitialized = false;

        /** Minimum value for depth. */
        const float fMinDepth = 0.0F;

        /** Maximum value for depth. */
        const float fMaxDepth = 1.0F;

        /** Back buffer format. */
        const DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        /** Depth/stencil buffer format. */
        const DXGI_FORMAT depthStencilBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        /** Use only display modes that use this scaling. */
        const DXGI_MODE_SCALING usedScaling = DXGI_MODE_SCALING_UNSPECIFIED;

        /** Use only display modes that use this scanline ordering. */
        const DXGI_MODE_SCANLINE_ORDER usedScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;

        /** D3D feature level that we use (required feature level). */
        const D3D_FEATURE_LEVEL rendererD3dFeatureLevel = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_12_1;

        /** Name of the category used for logging. */
        inline static const char* sDirectXRendererLogCategory = "DirectX Renderer";
    };
} // namespace ne
