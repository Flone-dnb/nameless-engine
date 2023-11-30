#pragma once

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

    class GameManager;
    class DirectXResourceManager;
    class Material;
    class CameraProperties;
    class DirectXPso;
    struct QueuedForExecutionComputeShaders;
    class ComputeShaderInterface;
    struct DirectXFrameResource;

    /** Renderer made with DirectX 12 API. */
    class DirectXRenderer : public Renderer {
    public:
        DirectXRenderer() = delete;
        DirectXRenderer(const DirectXRenderer&) = delete;
        DirectXRenderer& operator=(const DirectXRenderer&) = delete;

        virtual ~DirectXRenderer() override = default;

        /**
         * Creates a new DirectX renderer.
         *
         * @param pGameManager         GameManager object that owns this renderer.
         * @param vBlacklistedGpuNames Names of GPUs that should not be used, generally this means
         * that these GPUs were previously used to create the renderer but something went wrong.
         *
         * @return Created renderer if successful, otherwise multiple values in a pair: error and a
         * name of the GPU that the renderer tried to use (can be empty if failed before picking a GPU
         * or if all supported GPUs are blacklisted).
         */
        static std::variant<std::unique_ptr<Renderer>, std::pair<Error, std::string>>
        create(GameManager* pGameManager, const std::vector<std::string>& vBlacklistedGpuNames);

        /**
         * Returns used back buffer format.
         *
         * @return Back buffer format.
         */
        static DXGI_FORMAT getBackBufferFormat();

        /**
         * Returns used depth/stencil buffer format.
         *
         * @return Depth/stencil buffer format.
         */
        static DXGI_FORMAT getDepthStencilBufferFormat();

        /**
         * Returns used depth buffer format for resolved depth buffer.
         *
         * @return Depth buffer format.
         */
        static DXGI_FORMAT getDepthBufferFormatNoMultisampling();

        /**
         * Looks for video adapters (GPUs) that support this renderer.
         *
         * @remark Note that returned array might differ depending on the used renderer.
         *
         * @return Empty array if no GPU supports used renderer,
         * otherwise array with GPU names that can be used for this renderer.
         */
        virtual std::vector<std::string> getSupportedGpuNames() const override;

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
         * Blocks the current thread until the GPU finishes executing all queued commands up to this point.
         */
        virtual void waitForGpuToFinishWorkUpToThisPoint() override;

        /**
         * Returns renderer's type.
         *
         * @return Renderer's type.
         */
        virtual RendererType getType() const override;

        /**
         * Returns API version or a feature level that the renderer uses.
         *
         * For example DirectX renderer will return used feature level and Vulkan renderer
         * will return used Vulkan API version.
         *
         * @return Used API version.
         */
        virtual std::string getUsedApiVersion() const override;

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
         * Returns quality level count for the current MSAA sample count.
         *
         * @return MSAA quality level count.
         */
        UINT getMsaaQualityLevel() const;

        /**
         * Returns pointer to the texture resource that represents renderer's depth texture
         * without multisampling (resolved resource).
         *
         * @warning If MSAA is enabled this function will return one resource (pointer to a separate
         * depth resolved resource), if it's disabled it will return the other resource (pointer to depth
         * texture). So it may be a good idea to query this pointer every time you need it instead of saving
         * it and reusing it because every frame this pointer may change (due to other reasons such as
         * render target resize and etc).
         *
         * @return Pointer to depth texture.
         */
        virtual GpuResource* getDepthTextureNoMultisampling() override;

        /**
         * Returns size of the render target (size of the underlying render image).
         *
         * @return Render image size in pixels (width and height).
         */
        virtual std::pair<unsigned int, unsigned int> getRenderTargetSize() const override;

    protected:
        /**
         * Creates an empty (uninitialized) renderer.
         *
         * @remark Use @ref initialize to initialize the renderer.
         *
         * @param pGameManager GameManager object that owns this renderer.
         */
        DirectXRenderer(GameManager* pGameManager);

        /** Submits a new frame to the GPU. */
        virtual void drawNextFrame() override;

        /**
         * Submits commands to draw meshes and the specified depth only (vertex shader only) pipelines.
         *
         * @param pCurrentFrameResource      Frame resource of the frame being submitted.
         * @param iCurrentFrameResourceIndex Index of the current frame resource.
         * @param opaquePipelines            Opaque pipelines (depth pipeline will be retrieved from them).
         */
        void drawMeshesDepthPrepass(
            DirectXFrameResource* pCurrentFrameResource,
            size_t iCurrentFrameResourceIndex,
            const std::vector<Renderer::MeshesInFrustum::PipelineInFrustumInfo>& opaquePipelines);

        /**
         * Submits commands to draw meshes and pipelines of specific types (only opaque or transparent).
         *
         * @param pCurrentFrameResource       Frame resource of the frame being submitted.
         * @param iCurrentFrameResourceIndex  Index of the current frame resource.
         * @param pipelinesOfSpecificType     Pipelines to use.
         * @param bIsDrawingTransparentMeshes `true` if transparent pipelines are used, `false` otherwise.
         */
        void drawMeshesMainPass(
            DirectXFrameResource* pCurrentFrameResource,
            size_t iCurrentFrameResourceIndex,
            const std::vector<Renderer::MeshesInFrustum::PipelineInFrustumInfo>& pipelinesOfSpecificType,
            const bool bIsDrawingTransparentMeshes);

        /**
         * Called after some render setting is changed to recreate internal resources to match the current
         * settings.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> onRenderSettingsChanged() override;

        /**
         * Blocks the current thread until the GPU is finished using the specified frame resource.
         *
         * @remark Generally the current frame resource will be passed and so the current frame resource
         * mutex will be locked at the time of calling and until the function is not finished it will not
         * be unlocked.
         *
         * @param pFrameResource Frame resource to wait for.
         */
        virtual void waitForGpuToFinishUsingFrameResource(FrameResource* pFrameResource) override;

        /**
         * Returns the maximum anti-aliasing quality that can be used on the picked
         * GPU (@ref getCurrentlyUsedGpuName).
         *
         * @remark Note that the maximum supported AA quality can differ depending on the used GPU/renderer.
         *
         * @return Error if something went wrong,
         * otherwise `DISABLED` if AA is not supported or the maximum supported AA quality.
         */
        virtual std::variant<MsaaState, Error> getMaxSupportedAntialiasingQuality() const override;

        /**
         * Tells whether the renderer is initialized or not.
         *
         * Initialized renderer means that the hardware supports it and it's safe to use renderer
         * functionality such as @ref onRenderSettingsChanged.
         *
         * @return Whether the renderer is initialized or not.
         */
        virtual bool isInitialized() const override;

    private:
        /**
         * Initializes the renderer.
         *
         * @remark This function is usually called after constructing a new empty (uninitialized)
         * DirectX renderer.
         *
         * @param vBlacklistedGpuNames Names of GPUs that should not be used, generally this means
         * that these GPUs were previously used to create the renderer but something went wrong.
         *
         * @return Error if something went wrong (for ex. if the hardware does not support this
         * renderer).
         */
        [[nodiscard]] std::optional<Error> initialize(const std::vector<std::string>& vBlacklistedGpuNames);

        /**
         * Enables DX debug layer.
         *
         * @warning Should be called before you create DXGI factory or D3D device.
         *
         * @remark Only enables debug layer if DEBUG macro is defined.
         *
         * @return Returns an error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> enableDebugLayer();

        /**
         * (Re)creates the depth/stencil buffer with the "depth write" initial state
         * and binds a DSV to it.
         *
         * @remark Make sure that the old depth/stencil buffer (if was) is not used by the GPU before
         * calling this function.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createDepthStencilBuffer();

        /**
         * Rates available GPUs and picks the best one to be used (also considers GPU specified in
         * RenderSettings).
         *
         * @param vBlacklistedGpuNames Names of GPUs that should not be used, generally this means
         * that these GPUs were previously used to create the renderer but something went wrong.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error>
        pickVideoAdapter(const std::vector<std::string>& vBlacklistedGpuNames);

        /**
         * Rates the specified GPU and gives it a suitability score.
         *
         * @param adapterDesc GPU description.
         *
         * @return 0 if the GPU is not suitable for the renderer, otherwise GPU's suitability score.
         */
        static size_t rateGpuSuitability(DXGI_ADAPTER_DESC1 adapterDesc);

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
         * Initializes DirectX.
         *
         * @param vBlacklistedGpuNames Names of GPUs that should not be used, generally this means
         * that these GPUs were previously used to create the renderer but something went wrong.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error>
        initializeDirectX(const std::vector<std::string>& vBlacklistedGpuNames);

        /**
         * Setups everything for render commands to be recorded (updates frame constants, shader resources,
         * resets lists, binds RTV/DSV, etc.).
         *
         * @warning Expects that render resources mutex is locked.
         *
         * @param pCameraProperties     Camera properties to use.
         * @param pCurrentFrameResource Current frame resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> prepareForDrawingNextFrame(
            CameraProperties* pCameraProperties, DirectXFrameResource* pCurrentFrameResource);

        /**
         * Resets @ref pCommandList and adds initial commands like set descriptor heaps for graphics commands.
         *
         * @param pCurrentFrameResource Frame resource for the current frame.
         */
        void resetCommandListForGraphics(DirectXFrameResource* pCurrentFrameResource);

        /**
         * Closes and executes the specified command list on @ref pCommandQueue.
         *
         * @param pCommandListToExecute Command list to execute.
         */
        void executeGraphicsCommandList(ID3D12GraphicsCommandList* pCommandListToExecute);

        /**
         * Does final logic in drawing next frame (closes lists, executes lists, etc.).
         *
         * @warning Expects that render resources mutex is locked.
         *
         * @param pCurrentFrameResource Current frame resource.
         * @param pQueuedComputeShaders Queued shaders to dispatch.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> finishDrawingNextFrame(
            DirectXFrameResource* pCurrentFrameResource,
            QueuedForExecutionComputeShaders* pQueuedComputeShaders);

        /**
         * Queries the current render settings for MSAA quality and updates @ref iMsaaQualityLevelsCount.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> updateMsaaQualityLevelCount();

        /**
         * Waits until the GPU has completed commands up to the specified fence point.
         *
         * @param iFenceToWaitFor Fence value to wait for.
         */
        void waitForFenceValue(UINT64 iFenceToWaitFor);

        /**
         * Submits compute dispatch commands using @ref pCommandQueue.
         *
         * @param pCommandAllocator        Command allocator to reset @ref pComputeCommandList.
         * @param computePipelinesToSubmit Compute shaders and their pipelines to dispatch.
         */
        void dispatchComputeShadersOnGraphicsQueue(
            ID3D12CommandAllocator* pCommandAllocator,
            std::unordered_map<Pipeline*, std::unordered_set<ComputeShaderInterface*>>&
                computePipelinesToSubmit);

        /**
         * Returns a vector of display modes that the current output adapter
         * supports for current back buffer format.
         *
         * @return Error if something went wrong, vector of display modes otherwise.
         */
        std::variant<std::vector<DXGI_MODE_DESC>, Error> getSupportedDisplayModes() const;

        /**
         * Returns current buffer to draw to: either MSAA render buffer of swap chain's buffer.
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

        /** Command list for @ref dispatchComputeShadersOnGraphicsQueue. */
        ComPtr<ID3D12GraphicsCommandList> pComputeCommandList;

        /** Fence object. */
        ComPtr<ID3D12Fence> pFence;

#if defined(DEBUG)
        /** Debug message queue. */
        ComPtr<ID3D12InfoQueue1> pInfoQueue;

        /** Debug layer. */
        ComPtr<ID3D12Debug> pDebugController;
#endif

        /** Fence counter. */
        std::pair<std::recursive_mutex, UINT64> mtxCurrentFenceValue;

        /** Swap chain buffer. */
        std::vector<std::unique_ptr<DirectXResource>> vSwapChainBuffers;

        /** Depth stencil buffer. */
        std::unique_ptr<DirectXResource> pDepthStencilBuffer;

        /**
         * Depth buffer without multisampling (for light culing compute shader).
         *
         * @warning When @ref pDepthStencilBuffer does not use multisampling this buffer is not used
         * and does not store contents of @ref pDepthStencilBuffer.
         *
         * @remark Stores non-multisampled depth data from @ref pDepthStencilBuffer for shaders.
         */
        std::unique_ptr<DirectXResource> pDepthBufferNoMultisampling;

        /** Default back buffer fill color. */
        float backBufferFillColor[4] = {0.0F, 0.0F, 0.0F, 1.0F};

        /** Render target when MSAA is enabled because our swap chain does not support multisampling. */
        std::unique_ptr<DirectXResource> pMsaaRenderBuffer;

        /** List of supported GPUs, filled during @ref pickVideoAdapter. */
        std::vector<std::string> vSupportedGpuNames;

        /** Last set size of the underlying swap chain buffer. */
        std::pair<unsigned int, unsigned int> renderTargetSize = {0, 0};

        /** The number of supported quality levels for the current MSAA sample count. */
        UINT iMsaaQualityLevelsCount = 0;

        /** Screen viewport size and depth range. */
        D3D12_VIEWPORT screenViewport;

        /** Scissor rectangle for viewport. */
        D3D12_RECT scissorRect;

        /** Name of the GPU we are currently using. */
        std::string sUsedVideoAdapter;

        /** Synchronize presentation for at least N vertical blanks. Used when VSync is enabled.*/
        UINT iPresentSyncInterval = 0;

        /** Used to prevent tearing when VSync is enabled. */
        UINT iPresentFlags = 0;

        /**
         * Whether MSAA enabled and we use @ref pMsaaRenderBuffer as render buffer or not
         * and we use @ref pSwapChain as render buffer.
         */
        bool bIsUsingMsaaRenderTarget = true;

        /** Whether @ref initializeDirectX was finished without errors or not. */
        bool bIsDirectXInitialized = false;

        /** Back buffer format. */
        static constexpr DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        /** Depth/stencil buffer format. */
        static constexpr DXGI_FORMAT depthStencilBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        /**
         * Depth buffer format for @ref pDepthBufferNoMultisampling.
         *
         * @remark Resolve is only supported for non-integer and non-stencil types.
         */
        static constexpr DXGI_FORMAT depthBufferNoMultisamplingFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

        /** Use only display modes that use this scaling. */
        static constexpr DXGI_MODE_SCALING usedScaling = DXGI_MODE_SCALING_UNSPECIFIED;

        /** Use only display modes that use this scanline ordering. */
        static constexpr DXGI_MODE_SCANLINE_ORDER usedScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;

        /** D3D feature level that we use (required feature level). */
        static constexpr D3D_FEATURE_LEVEL rendererD3dFeatureLevel =
            D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_12_1;
    };
} // namespace ne
