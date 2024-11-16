#include "DirectXRenderer.h"

// Standard.
#include <format>
#include <array>

// Custom.
#include "game/Window.h"
#include "io/Logger.h"
#include "misc/Globals.h"
#include "render/RenderSettings.h"
#include "render/directx/pipeline/DirectXPso.h"
#include "render/directx/resource/DirectXResourceManager.h"
#include "render/general/pipeline/PipelineManager.h"
#include "render/general/resource/shadow/ShadowMapHandle.h"
#include "render/directx/resource/DirectXResource.h"
#include "material/Material.h"
#include "game/nodes/MeshNode.h"
#include "shader/hlsl/RootSignatureGenerator.h"
#include "shader/general/resource/binding/cpuwrite/ShaderCpuWriteResourceBinding.h"
#include "render/general/resource/frame/FrameResourceManager.h"
#include "game/camera/CameraProperties.h"
#include "game/camera/CameraManager.h"
#include "shader/hlsl/resource/binding/texture/HlslShaderTextureResourceBinding.h"
#include "render/directx/resource/DirectXFrameResource.h"
#include "render/directx/resource/shadow/DirectXShadowMapArrayIndexManager.h"
#include "shader/hlsl/HlslComputeShaderInterface.h"
#include "shader/general/resource/LightingShaderResourceManager.h"
#include "misc/Profiler.hpp"

// OS.
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#if defined(DEBUG) || defined(_DEBUG)
#include <dxgidebug.h>
#include <InitGuid.h>
#pragma comment(lib, "dxguid.lib")
#endif
#include "pix.h"

namespace ne {
// Prepare a macro to create a scoped GPU mark for RenderDoc.
#if defined(DEBUG)
    class GpuDirectXDebugMarkScopeGuard {
    public:
        GpuDirectXDebugMarkScopeGuard(
            const ComPtr<ID3D12CommandQueue>& pCommandQueue, const char* pLabelName) {
            this->pCommandQueue = pCommandQueue.Get();

            PIXBeginEvent(pCommandQueue.Get(), 0, pLabelName);
        }
        GpuDirectXDebugMarkScopeGuard(const GpuDirectXDebugMarkScopeGuard&) = delete;
        GpuDirectXDebugMarkScopeGuard& operator=(const GpuDirectXDebugMarkScopeGuard&) = delete;
        ~GpuDirectXDebugMarkScopeGuard() { PIXEndEvent(pCommandQueue); }

    private:
        ID3D12CommandQueue* pCommandQueue = nullptr;
    };

// Creates a scoped GPU mark for RenderDoc.
#define GPU_MARK_FUNC GpuDirectXDebugMarkScopeGuard gpuMarkFuncGuard(pCommandQueue, __FUNCTION__);
#else
#define GPU_MARK_FUNC
#endif

// Define debug layer callback.
#if defined(DEBUG)
    void d3dInfoQueueMessageCallback(
        D3D12_MESSAGE_CATEGORY Category,
        D3D12_MESSAGE_SEVERITY Severity,
        D3D12_MESSAGE_ID ID,
        LPCSTR pDescription,
        void* pContext) {
        Logger::get().error(std::format("[debug layer] {}", pDescription));
    }
#endif

    DirectXRenderer::DirectXRenderer(GameManager* pGameManager) : Renderer(pGameManager) {
        // Make sure we use the same formats as in Vulkan.
        static_assert(
            backBufferFormat == DXGI_FORMAT_R8G8B8A8_UNORM,
            "also change format in Vulkan renderer for (visual) consistency");
        static_assert(
            depthStencilBufferFormat == DXGI_FORMAT_D32_FLOAT,
            "also change format in Vulkan renderer for (visual) consistency");

        // Check shadow map format.
        static_assert(
            shadowMapFormat == DXGI_FORMAT_D32_FLOAT,
            "also change format in Vulkan renderer for (visual) consistency");
        static_assert(
            shadowMappingPointLightColorTargetFormat == DXGI_FORMAT_R32_FLOAT,
            "also change format in Vulkan renderer for (visual) consistency");

        // Self check for light culling compute shader:
        static_assert(
            depthStencilBufferFormat == DXGI_FORMAT_D32_FLOAT,
            "light culling compute shader expects the depth values to be in range [0..1] for atomic "
            "operations, please review the light culling compute shader and make sure atomics will work "
            "correctly");
    }

    DirectXRenderer::~DirectXRenderer() {
        // Just in case, wait for all work to be finished.
        DirectXRenderer::waitForGpuToFinishWorkUpToThisPoint();

        // Explicitly destroy some GPU resources.
        resetLightingShaderResourceManager();
        resetFrameResourceManager();

        pMsaaRenderBuffer = nullptr;
        pDepthBufferNoMultisampling = nullptr;
        pDepthStencilBuffer = nullptr;
        vSwapChainBuffers.clear();

        // Explicitly destroy GPU resource manager to see if any GPU resources are still left alive.
        resetGpuResourceManager();
    }

    std::optional<Error> DirectXRenderer::initialize(const std::vector<std::string>& vBlacklistedGpuNames) {
        std::scoped_lock frameGuard(*getRenderResourcesMutex());

        // Initialize the current fence value.
        mtxCurrentFenceValue.second = 0;

        // Initialize essential render entities (such as render settings).
        auto optionalError = initializeRenderer();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Initialize DirectX.
        optionalError = initializeDirectX(vBlacklistedGpuNames);
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Disable Alt + Enter in order to avoid switching to fullscreen exclusive mode
        // because we don't support it and use windowed fullscreen instead.
        const HRESULT hResult =
            pFactory->MakeWindowAssociation(getWindow()->getWindowHandle(), DXGI_MWA_NO_ALT_ENTER);
        if (FAILED(hResult)) {
            Error error(hResult);
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Set initial size for buffers.
        optionalError = onRenderSettingsChanged();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::variant<std::unique_ptr<Renderer>, std::pair<Error, std::string>>
    DirectXRenderer::create(GameManager* pGameManager, const std::vector<std::string>& vBlacklistedGpuNames) {
        // Create an empty (uninitialized) DirectX renderer.
        auto pRenderer = std::unique_ptr<DirectXRenderer>(new DirectXRenderer(pGameManager));

        // Initialize renderer.
        const auto optionalError = pRenderer->initialize(vBlacklistedGpuNames);
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return std::pair<Error, std::string>{error, pRenderer->sUsedVideoAdapter};
        }

        return pRenderer;
    }

#if defined(DEBUG)
    std::optional<Error> DirectXRenderer::enableDebugLayer() {
        HRESULT hResult = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        pDebugController->EnableDebugLayer();

        ComPtr<IDXGIInfoQueue> pDxgiInfoQueue;
        hResult = DXGIGetDebugInterface1(0, IID_PPV_ARGS(pDxgiInfoQueue.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, 1);
        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, 1);
        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, 1);

        Logger::get().info("D3D debug layer enabled");

        return {};
    }
#endif

    std::optional<Error> DirectXRenderer::createDepthStencilBuffer() {
        // Get render settings.
        const auto mtxRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(*mtxRenderSettings.first);

        const auto renderResolution = mtxRenderSettings.second->getRenderResolution();
        const auto iMsaaSampleCount =
            static_cast<unsigned int>(mtxRenderSettings.second->getAntialiasingQuality());

        // Prepare resource description.
        D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC(
            D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            0,
            renderResolution.first,
            renderResolution.second,
            1,
            1,
            depthStencilBufferFormat,
            iMsaaSampleCount,
            iMsaaSampleCount > 1 ? (iMsaaQualityLevelsCount - 1) : 0,
            D3D12_TEXTURE_LAYOUT_UNKNOWN,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        D3D12_CLEAR_VALUE depthClear;
        depthClear.Format = depthStencilBufferFormat;
        depthClear.DepthStencil.Depth = getMaxDepth();
        depthClear.DepthStencil.Stencil = 0;

        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        // Create resource.
        auto result =
            dynamic_cast<DirectXResourceManager*>(getResourceManager())
                ->createResource(
                    "renderer depth/stencil buffer",
                    allocationDesc,
                    depthStencilDesc,
                    D3D12_RESOURCE_STATE_DEPTH_READ, // will transition to WRITE (see `draw` function)
                    depthClear);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        pDepthStencilBuffer = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        // Bind DSV.
        auto optionalError = pDepthStencilBuffer->bindDescriptor(DirectXDescriptorType::DSV);
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Describe non-multisampled depth buffer.
        depthStencilDesc = CD3DX12_RESOURCE_DESC(
            D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            0,
            renderResolution.first,
            renderResolution.second,
            1,
            1,
            depthBufferNoMultisamplingFormat,
            1, // 1 sample
            0,
            D3D12_TEXTURE_LAYOUT_UNKNOWN,
            D3D12_RESOURCE_FLAG_NONE);

        // Create non-multisampled depth buffer.
        result = dynamic_cast<DirectXResourceManager*>(getResourceManager())
                     ->createResource(
                         "non-multisampled depth/stencil buffer",
                         allocationDesc,
                         depthStencilDesc,
                         D3D12_RESOURCE_STATE_GENERIC_READ,
                         {});
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        pDepthBufferNoMultisampling = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        return {};
    }

    std::optional<Error>
    DirectXRenderer::pickVideoAdapter(const std::vector<std::string>& vBlacklistedGpuNames) {
        // Prepare struct to store GPU info.
        struct GpuInfo {
            ComPtr<IDXGIAdapter3> pGpu;
            DXGI_ADAPTER_DESC1 desc;
        };
        std::vector<GpuInfo> vGpus;

        // Get information about the GPUs.
        for (UINT iAdapterIndex = 0;; iAdapterIndex++) {
            ComPtr<IDXGIAdapter3> pTestAdapter;

            if (pFactory->EnumAdapters(
                    iAdapterIndex, reinterpret_cast<IDXGIAdapter**>(pTestAdapter.GetAddressOf())) ==
                DXGI_ERROR_NOT_FOUND) {
                // No more adapters to enumerate.
                break;
            }

            // Check if the adapter supports used D3D version, but don't create the actual device yet.
            const HRESULT hResult = D3D12CreateDevice(
                pTestAdapter.Get(), rendererD3dFeatureLevel, _uuidof(ID3D12Device), nullptr);
            if (SUCCEEDED(hResult)) {
                // Found supported adapter.

                // Get its description.
                GpuInfo info;
                const auto hResult = pTestAdapter->GetDesc1(&info.desc);
                if (FAILED(hResult)) [[unlikely]] {
                    Logger::get().info("failed to get video adapter description, skipping this GPU");
                    continue;
                }

                // Save adapter pointer.
                info.pGpu = pTestAdapter;

                // Add to be considered.
                vGpus.push_back(std::move(info));
            }
        }

        // Make sure there is at least one GPU.
        if (vGpus.empty()) {
            return Error("could not find a GPU that supports used DirectX feature level");
        }

        // Prepare a struct for GPU suitability score.
        struct GpuScore {
            ComPtr<IDXGIAdapter3> pGpu;
            size_t iScore = 0;
            std::string sGpuName;
            bool bIsBlacklisted = false;
        };
        std::vector<GpuScore> vGpuScores;

        // Rate all GPUs.
        vSupportedGpuNames.clear();
        for (const auto& gpuInfo : vGpus) {
            // Rate GPU.
            GpuScore score;
            score.iScore = rateGpuSuitability(gpuInfo.desc);

            if (score.iScore == 0) {
                // Skip not suitable GPUs.
                continue;
            }

            // Save info.
            score.sGpuName = Globals::wstringToString(gpuInfo.desc.Description);
            score.pGpu = gpuInfo.pGpu;

            // Add to be considered.
            vGpuScores.push_back(score);

            // Save to the list of supported GPUs.
            vSupportedGpuNames.push_back(score.sGpuName);
        }

        // Make sure there is at least one GPU.
        if (vGpuScores.empty()) {
            return Error("failed to find a suitable GPU");
        }

        // Sort GPUs by score.
        std::sort(
            vGpuScores.begin(), vGpuScores.end(), [](const GpuScore& scoreA, const GpuScore& scoreB) -> bool {
                return scoreA.iScore > scoreB.iScore;
            });

        // Mark GPUs that are blacklisted and log scores.
        std::string sRating = std::format("found and rated {} suitable GPU(s):", vGpuScores.size());
        for (size_t i = 0; i < vGpuScores.size(); i++) {
            // See if this GPU is blacklisted.
            bool bIsBlacklisted = false;
            for (const auto& sBlacklistedGpuName : vBlacklistedGpuNames) {
                if (vGpuScores[i].sGpuName == sBlacklistedGpuName) {
                    bIsBlacklisted = true;
                    break;
                }
            }

            // Add new entry in rating log.
            sRating += std::format("\n{}. ", i + 1);

            // Process status.
            if (bIsBlacklisted) {
                sRating += "[blacklisted] ";
                vGpuScores[i].bIsBlacklisted = true;
            }

            // Append GPU name and score.
            sRating += std::format("{}, suitability score: {}", vGpuScores[i].sGpuName, vGpuScores[i].iScore);
        }

        // Print scores.
        Logger::get().info(sRating);

        // Get render settings.
        const auto mtxRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(*mtxRenderSettings.first);

        // Check if the GPU to use is set.
        auto sGpuNameToUse = mtxRenderSettings.second->getGpuToUse();
        if (!sGpuNameToUse.empty()) {
            // Find the GPU in the list of available GPUs.
            std::optional<size_t> iFoundIndex{};
            for (size_t i = 0; i < vGpuScores.size(); i++) {
                if (vGpuScores[i].sGpuName == sGpuNameToUse) {
                    iFoundIndex = i;
                    break;
                }
            }
            if (!iFoundIndex.has_value()) {
                // Not found. Log event.
                Logger::get().info(std::format(
                    "unable to find the GPU \"{}\" (that was specified in the renderer's "
                    "config file) in the list of available GPUs for this renderer",
                    sGpuNameToUse));
            } else if (iFoundIndex.value() > 0) {
                // Put found GPU in the first place.
                auto temp = vGpuScores[0];
                vGpuScores[0] = vGpuScores[iFoundIndex.value()];
                vGpuScores[iFoundIndex.value()] = temp;
            }
        }

        // Pick the best suiting GPU.
        for (size_t i = 0; i < vGpuScores.size(); i++) {
            const auto& currentGpuInfo = vGpuScores[i];

            // Skip this GPU is it's blacklisted.
            if (currentGpuInfo.bIsBlacklisted) {
                Logger::get().info(std::format(
                    "ignoring GPU \"{}\" because it's blacklisted (previously the renderer failed to "
                    "initialize using this GPU)",
                    currentGpuInfo.sGpuName));
                continue;
            }

            // Log used GPU.
            if (sGpuNameToUse == currentGpuInfo.sGpuName) {
                Logger::get().info(std::format(
                    "using the following GPU: \"{}\" (was specified as preferred in the renderer's "
                    "config file)",
                    currentGpuInfo.sGpuName));
            } else {
                Logger::get().info(std::format("using the following GPU: \"{}\"", currentGpuInfo.sGpuName));
            }

            // Select video adapter.
            pVideoAdapter = currentGpuInfo.pGpu;

            // Save GPU name in the settings.
            mtxRenderSettings.second->setGpuToUse(currentGpuInfo.sGpuName);

            // Save GPU name.
            sUsedVideoAdapter = currentGpuInfo.sGpuName;

            break;
        }

        // Make sure we picked a GPU.
        if (pVideoAdapter == nullptr) {
            return Error(std::format(
                "found {} suitable GPU(s) but no GPU was picked (see reason above)", vGpuScores.size()));
        }

        return {};
    }

    std::vector<std::string> DirectXRenderer::getSupportedGpuNames() const { return vSupportedGpuNames; }

    std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
    DirectXRenderer::getSupportedRenderResolutions() const {
        auto result = getSupportedDisplayModes();
        if (std::holds_alternative<Error>(result)) {
            Error error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const std::vector<DXGI_MODE_DESC> vRenderModes =
            std::get<std::vector<DXGI_MODE_DESC>>(std::move(result));

        std::set<std::pair<unsigned int, unsigned int>> foundResolutions;
        for (const auto& mode : vRenderModes) {
            foundResolutions.insert(std::make_pair(mode.Width, mode.Height));
        }

        return foundResolutions;
    }

    std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
    DirectXRenderer::getSupportedRefreshRates() const {
        auto result = getSupportedDisplayModes();
        if (std::holds_alternative<Error>(result)) {
            Error error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const std::vector<DXGI_MODE_DESC> vRenderModes =
            std::get<std::vector<DXGI_MODE_DESC>>(std::move(result));

        std::set<std::pair<unsigned int, unsigned int>> foundRefreshRates;
        for (const auto& mode : vRenderModes) {
            foundRefreshRates.insert(
                std::make_pair(mode.RefreshRate.Numerator, mode.RefreshRate.Denominator));
        }

        return foundRefreshRates;
    }

    std::string DirectXRenderer::getCurrentlyUsedGpuName() const { return sUsedVideoAdapter; }

    void DirectXRenderer::prepareForDrawingNextFrame(
        CameraProperties* pCameraProperties, FrameResource* pCurrentFrameResource) {
        PROFILE_FUNC;

        // Get command allocator.
        const auto pCommandAllocator =
            reinterpret_cast<DirectXFrameResource*>(pCurrentFrameResource)->pCommandAllocator.Get();

        PROFILE_SCOPE_START(ResetCommandAllocator);

        // Clear command allocator since the GPU is no longer using it.
        auto hResult = pCommandAllocator->Reset();
        if (FAILED(hResult)) [[unlikely]] {
            Error error(hResult);
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        PROFILE_SCOPE_END;
    }

    void DirectXRenderer::resetCommandListForGraphics(DirectXFrameResource* pCurrentFrameResource) {
        PROFILE_FUNC;

        PROFILE_SCOPE_START(ResetCommandList);

        // Open command list to record new commands.
        const auto hResult = pCommandList->Reset(pCurrentFrameResource->pCommandAllocator.Get(), nullptr);
        if (FAILED(hResult)) [[unlikely]] {
            Error error(hResult);
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        PROFILE_SCOPE_END;

        // Set CBV/SRV/UAV descriptor heap.
        const auto pResourceManager = reinterpret_cast<DirectXResourceManager*>(getResourceManager());
        ID3D12DescriptorHeap* pDescriptorHeaps[] = {pResourceManager->getCbvSrvUavHeap()->getInternalHeap()};
        pCommandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

        // Set viewport size and scissor rectangles.
        pCommandList->RSSetViewports(1, &screenViewport);
        pCommandList->RSSetScissorRects(1, &scissorRect);

        // Set topology type.
        // TODO: this will be moved in some other place later
        pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    void DirectXRenderer::drawMeshesDepthPrepass(
        FrameResource* pCurrentFrameResource,
        size_t iCurrentFrameResourceIndex,
        const std::vector<Renderer::MeshesInFrustum::PipelineInFrustumInfo>& vOpaquePipelines) {
        PROFILE_FUNC;
        GPU_MARK_FUNC;

        // Prepare command list.
        resetCommandListForGraphics(reinterpret_cast<DirectXFrameResource*>(pCurrentFrameResource));

        // Get DSV handle to be used.
        const auto depthStencilDescriptorHandle =
            *pDepthStencilBuffer->getBindedDescriptorCpuHandle(DirectXDescriptorType::DSV);

        // Transition depth buffer to write state.
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
            pDepthStencilBuffer->getInternalResource(),
            D3D12_RESOURCE_STATE_DEPTH_READ,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        pCommandList->ResourceBarrier(1, &transition);

        // Clear depth/stencil.
        // (some weird error in external code or clang-tidy's false-positive)
        // NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
        pCommandList->ClearDepthStencilView(
            depthStencilDescriptorHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            getMaxDepth(),
            0,
            0,
            nullptr);
        // NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)

        // Bind only DSV for depth prepass.
        pCommandList->OMSetRenderTargets(0, nullptr, FALSE, &depthStencilDescriptorHandle);

        // Prepare draw call counter to be used later.
        const auto pDrawCallCounter = getDrawCallCounter();

        for (const auto& pipelineInfo : vOpaquePipelines) {
            // Get depth only (vertex shader only) pipeline
            // (all materials that use the same opaque pipeline will use the same depth only pipeline).
            const auto pDirectXPso =
                reinterpret_cast<DirectXPso*>(pipelineInfo.vMaterials[0].pMaterial->getDepthOnlyPipeline());

            // Get pipeline resources and root constants.
            auto pMtxPsoResources = pDirectXPso->getInternalResources();
            const auto pMtxRootConstantsManager = pDirectXPso->getShaderConstants();

            // Lock both.
            std::scoped_lock guardPsoResources(pMtxPsoResources->first, pMtxRootConstantsManager->first);

            // Get root constants manager.
            const auto pRootConstantsManager = pMtxRootConstantsManager->second->pConstantsManager.get();

            // Set PSO and root signature.
            pCommandList->SetPipelineState(pMtxPsoResources->second.pPso.Get());
            pCommandList->SetGraphicsRootSignature(pMtxPsoResources->second.pRootSignature.Get());

            // After setting root signature we can set root parameters.

            // Bind global shader resources.
            pDirectXPso->bindGlobalShaderResourceViews(pCommandList, iCurrentFrameResourceIndex);

            // Set CBV to frame constant buffer.
            pCommandList->SetGraphicsRootConstantBufferView(
                pMtxPsoResources->second
                    .vSpecialRootParameterIndices[static_cast<size_t>(SpecialRootParameterSlot::FRAME_DATA)],
                reinterpret_cast<DirectXResource*>(
                    pCurrentFrameResource->pFrameConstantBuffer->getInternalResource())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());

            for (const auto& materialInfo : pipelineInfo.vMaterials) {
                // No need to bind material's shader resources since they are not used in vertex shader
                // (since we are in depth prepass).

                for (const auto& meshInfo : materialInfo.vMeshes) {
                    // Get mesh data.
                    auto pMtxMeshGpuResources = meshInfo.pMeshNode->getMeshGpuResources();
                    auto mtxMeshData = meshInfo.pMeshNode->getMeshData();

                    // Note: if you will ever need it - don't lock mesh node's spawning/despawning mutex
                    // here as it might cause a deadlock (see MeshNode::setMaterial for example).
                    std::scoped_lock geometryGuard(pMtxMeshGpuResources->first, *mtxMeshData.first);

                    // Find and bind mesh data resource since only it is used in vertex shader.
                    const auto& meshDataIt =
                        pMtxMeshGpuResources->second.shaderResources.shaderCpuWriteResourceBindings.find(
                            MeshNode::getMeshShaderConstantBufferName());
#if defined(DEBUG)
                    if (meshDataIt ==
                        pMtxMeshGpuResources->second.shaderResources.shaderCpuWriteResourceBindings.end())
                        [[unlikely]] {
                        Error error(std::format(
                            "expected to find \"{}\" shader resource",
                            MeshNode::getMeshShaderConstantBufferName()));
                        error.showError();
                        throw std::runtime_error(error.getFullErrorMessage());
                    }
#endif
                    meshDataIt->second.getResource()->copyResourceIndexToShaderConstants(
                        pRootConstantsManager, pDirectXPso, iCurrentFrameResourceIndex);

                    // Bind root constants.
                    pCommandList->SetGraphicsRoot32BitConstants(
                        pMtxPsoResources->second.vSpecialRootParameterIndices[static_cast<size_t>(
                            SpecialRootParameterSlot::ROOT_CONSTANTS)],
                        pRootConstantsManager->getVariableCount(),
                        pRootConstantsManager->getData(),
                        0);

                    // Prepare vertex buffer view.
                    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
                    vertexBufferView.BufferLocation =
                        reinterpret_cast<DirectXResource*>(
                            pMtxMeshGpuResources->second.mesh.pVertexBuffer.get())
                            ->getInternalResource()
                            ->GetGPUVirtualAddress();
                    vertexBufferView.StrideInBytes = sizeof(MeshVertex);
                    vertexBufferView.SizeInBytes = static_cast<UINT>(
                        mtxMeshData.second->getVertices()->size() * vertexBufferView.StrideInBytes);

                    // Set vertex buffer view.
                    pCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);

                    // Iterate over all index buffers of a specific mesh node that use this material.
                    for (const auto& indexBufferInfo : meshInfo.vIndexBuffers) {
                        // Prepare index buffer view.
                        static_assert(
                            sizeof(MeshData::meshindex_t) == sizeof(unsigned int), "change `Format`");
                        D3D12_INDEX_BUFFER_VIEW indexBufferView;
                        indexBufferView.BufferLocation =
                            reinterpret_cast<DirectXResource*>(indexBufferInfo.pIndexBuffer)
                                ->getInternalResource()
                                ->GetGPUVirtualAddress();
                        indexBufferView.Format = DXGI_FORMAT_R32_UINT;
                        indexBufferView.SizeInBytes =
                            static_cast<UINT>(indexBufferInfo.iIndexCount * sizeof(MeshData::meshindex_t));

                        // Set vertex/index buffer.
                        pCommandList->IASetIndexBuffer(&indexBufferView);

                        // Add a draw command.
                        pCommandList->DrawIndexedInstanced(indexBufferInfo.iIndexCount, 1, 0, 0, 0);

                        // Increment draw call counter.
                        pDrawCallCounter->fetch_add(1);
                    }
                }
            }
        }

        // Prepare array of depth buffer transitions that we will do slightly later.
        std::vector<CD3DX12_RESOURCE_BARRIER> vDepthTransitions;

        if (bIsUsingMsaaRenderTarget) {
            PROFILE_SCOPE(ResolveDepthStencilBufferForShaders);

            // Resolve multisampled depth buffer to a non-multisampled depth buffer for shaders.
            const std::array<CD3DX12_RESOURCE_BARRIER, 2> vBarriersToResolve = {
                CD3DX12_RESOURCE_BARRIER::Transition(
                    pDepthStencilBuffer->getInternalResource(),
                    D3D12_RESOURCE_STATE_DEPTH_WRITE,
                    D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(
                    pDepthBufferNoMultisampling->getInternalResource(),
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    D3D12_RESOURCE_STATE_RESOLVE_DEST)};

            // Record barrier.
            pCommandList->ResourceBarrier(
                static_cast<UINT>(vBarriersToResolve.size()), vBarriersToResolve.data());

            // Resolve.
            pCommandList->ResolveSubresource(
                pDepthBufferNoMultisampling->getInternalResource(),
                0,
                pDepthStencilBuffer->getInternalResource(),
                0,
                depthBufferNoMultisamplingFormat);

            // Transition depth buffer to depth read.
            vDepthTransitions.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                pDepthStencilBuffer->getInternalResource(),
                D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
                D3D12_RESOURCE_STATE_DEPTH_READ));

            // Transition non-multisampled depth buffer to generic read.
            vDepthTransitions.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                pDepthBufferNoMultisampling->getInternalResource(),
                D3D12_RESOURCE_STATE_RESOLVE_DEST,
                D3D12_RESOURCE_STATE_GENERIC_READ));
        } else {
            // Copying depth buffer to non-multisampled depth buffer is not needed since the depth buffer
            // does not use multisampling.
            // We will just use default depth buffer in shaders.

            // Transition depth buffer to depth read.
            vDepthTransitions.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                pDepthStencilBuffer->getInternalResource(),
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                D3D12_RESOURCE_STATE_DEPTH_READ));
        }

        // Record barriers.
        pCommandList->ResourceBarrier(static_cast<UINT>(vDepthTransitions.size()), vDepthTransitions.data());

        // Execute recorded commands.
        executeGraphicsCommandList(pCommandList.Get());
    }

    void DirectXRenderer::drawShadowMappingPass(
        FrameResource* pCurrentFrameResource,
        size_t iCurrentFrameResourceIndex,
        GraphicsPipelineRegistry* pGraphicsPipelines) {
        PROFILE_FUNC;
        GPU_MARK_FUNC;

        // Prepare command list.
        resetCommandListForGraphics(reinterpret_cast<DirectXFrameResource*>(pCurrentFrameResource));

        // Prepare draw call counter to be used later.
        const auto pDrawCallCounter = getDrawCallCounter();

        // Get pipelines to iterate over.
        const auto& shadowMappingDirectionalSpotPipelines = pGraphicsPipelines->vPipelineTypes.at(
            static_cast<size_t>(GraphicsPipelineType::PT_SHADOW_MAPPING_DIRECTIONAL_SPOT));
        const auto& shadowMappingPointPipelines = pGraphicsPipelines->vPipelineTypes.at(
            static_cast<size_t>(GraphicsPipelineType::PT_SHADOW_MAPPING_POINT));

        // Prepare lambda to set viewport size according to shadow map size.
        const auto setViewportSizeToShadowMap = [&](ShadowMapHandle* pShadowMapHandle) {
            const auto iShadowMapSize = pShadowMapHandle->getShadowMapSize();

            D3D12_VIEWPORT shadowMapViewport;
            D3D12_RECT shadowMapScissorRect;

            shadowMapViewport.TopLeftX = 0;
            shadowMapViewport.TopLeftY = 0;
            shadowMapViewport.Width = static_cast<float>(iShadowMapSize);
            shadowMapViewport.Height = static_cast<float>(iShadowMapSize);
            shadowMapViewport.MinDepth = getMinDepth();
            shadowMapViewport.MaxDepth = getMaxDepth();

            shadowMapScissorRect.left = 0;
            shadowMapScissorRect.top = 0;
            shadowMapScissorRect.right = static_cast<LONG>(iShadowMapSize);
            shadowMapScissorRect.bottom = static_cast<LONG>(iShadowMapSize);

            // Set shadow map size as viewport size.
            pCommandList->RSSetViewports(1, &shadowMapViewport);
            pCommandList->RSSetScissorRects(1, &shadowMapScissorRect);
        };

        // Prepare lambda for render targets.
        const auto transitionDirectionalSpotTargetsToWrite = [&](DirectXResource* pDepthTexture) {
            // Transition shadow map to write state.
            const auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
                pDepthTexture->getInternalResource(),
                D3D12_RESOURCE_STATE_DEPTH_READ,
                D3D12_RESOURCE_STATE_DEPTH_WRITE);
            pCommandList->ResourceBarrier(1, &transition);

            // Get DSV handle.
            const auto depthDescriptorHandle =
                *pDepthTexture->getBindedDescriptorCpuHandle(DirectXDescriptorType::DSV);

            // Clear depth.
            pCommandList->ClearDepthStencilView(
                depthDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH, getMaxDepth(), 0, 0, nullptr);

            // Bind only DSV.
            pCommandList->OMSetRenderTargets(0, nullptr, FALSE, &depthDescriptorHandle);
        };

        // Prepare lambda for render targets.
        const auto transitionDirectionalSpotTargetsToRead = [&](DirectXResource* pDepthTexture) {
            // Transition shadow map to read state.
            const auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
                pDepthTexture->getInternalResource(),
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                D3D12_RESOURCE_STATE_DEPTH_READ);
            pCommandList->ResourceBarrier(1, &transition);
        };

        // Prepare lambda for render targets.
        const auto transitionPointTargetsToWrite = [&](DirectXResource* pDepthTexture,
                                                       DirectXResource* pColorTexture) {
            // Transition shadow map to write state.
            std::array<CD3DX12_RESOURCE_BARRIER, 2> vBarriers;
            vBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
                pDepthTexture->getInternalResource(),
                D3D12_RESOURCE_STATE_DEPTH_READ,
                D3D12_RESOURCE_STATE_DEPTH_WRITE);
            vBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
                pColorTexture->getInternalResource(),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                D3D12_RESOURCE_STATE_RENDER_TARGET);
            pCommandList->ResourceBarrier(static_cast<UINT>(vBarriers.size()), vBarriers.data());
        };

        // Prepare lambda for render targets.
        const auto setPointLightTargets =
            [&](DirectXResource* pDepthTexture, DirectXResource* pColorTexture, size_t iCubemapFaceIndex) {
                // Get handles.
                const auto depthDescriptorHandle =
                    *pDepthTexture->getBindedDescriptorCpuHandle(DirectXDescriptorType::DSV);
                const auto renderDescriptorHandle = *pColorTexture->getBindedCubemapFaceDescriptorCpuHandle(
                    DirectXDescriptorType::RTV, iCubemapFaceIndex);

                // Clear depth.
                pCommandList->ClearDepthStencilView(
                    depthDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH, getMaxDepth(), 0, 0, nullptr);

                // Clear color.
                std::array<float, 1> vClearColor = {0.0F};
                pCommandList->ClearRenderTargetView(renderDescriptorHandle, vClearColor.data(), 0, nullptr);

                // Bind DSV and RTV.
                pCommandList->OMSetRenderTargets(1, &renderDescriptorHandle, FALSE, &depthDescriptorHandle);
            };

        // Prepare lambda for render targets.
        const auto transitionPointTargetsToRead = [&](DirectXResource* pDepthTexture,
                                                      DirectXResource* pColorTexture) {
            // Transition shadow map to read state.
            std::array<CD3DX12_RESOURCE_BARRIER, 2> vBarriers;
            vBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
                pDepthTexture->getInternalResource(),
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                D3D12_RESOURCE_STATE_DEPTH_READ);
            vBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
                pColorTexture->getInternalResource(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_GENERIC_READ);
            pCommandList->ResourceBarrier(static_cast<UINT>(vBarriers.size()), vBarriers.data());
        };

        // Prepare lambda to draw scene to shadow map.
        const auto drawSceneToShadowMap = [&](const std::unordered_map<std::string, ShaderPipelines>&
                                                  pipelinesToRender,
                                              ShadowMapHandle* pShadowMapHandle,
                                              unsigned int iIndexIntoShadowPassLightInfoArray,
                                              size_t iCubemapFaceIndex = 0) {
            // Get shadow map texture.
            const auto pMtxShadowResources = pShadowMapHandle->getResources();
            std::scoped_lock shadowMapGuard(pMtxShadowResources->first);

            // Set viewport size.
            setViewportSizeToShadowMap(pShadowMapHandle);

            // Iterate over all shadow mapping pipelines that have different vertex shaders.
            for (const auto& [sShaderNames, pipelines] : pipelinesToRender) {

                // Iterate over all shadow mapping pipelines that use the same vertex shader but different
                // shader macros (if there are different macro variations).
                for (const auto& [macros, pPipeline] : pipelines.shaderPipelines) {
                    // Convert pipeline type.
                    const auto pDirectXPso = reinterpret_cast<DirectXPso*>(pPipeline.get());

                    // Get pipeline resources and root constants.
                    auto pMtxPsoResources = pDirectXPso->getInternalResources();
                    const auto pMtxRootConstantsManager = pPipeline->getShaderConstants();

                    // Lock both.
                    std::scoped_lock guardPsoResources(
                        pMtxPsoResources->first, pMtxRootConstantsManager->first);

                    // Get root constants manager.
                    const auto pRootConstantsManager =
                        pMtxRootConstantsManager->second->pConstantsManager.get();

                    // Set PSO and root signature.
                    pCommandList->SetPipelineState(pMtxPsoResources->second.pPso.Get());
                    pCommandList->SetGraphicsRootSignature(pMtxPsoResources->second.pRootSignature.Get());

                    // After setting root signature we can set root parameters.

                    // NOTE: don't set frame data buffer because it's not used in shadow mapping.

                    // Bind global shader resources.
                    pDirectXPso->bindGlobalShaderResourceViews(pCommandList, iCurrentFrameResourceIndex);

                    // Bind array of viewProjection matrix array for lights.
                    getLightingShaderResourceManager()->setShadowPassLightInfoViewToCommandList(
                        pDirectXPso, pCommandList, iCurrentFrameResourceIndex);

#if defined(DEBUG)
                    // Self check: make sure root constants are used.
                    if (!pMtxRootConstantsManager->second.has_value()) [[unlikely]] {
                        Error error(std::format(
                            "expected root constants to be used on pipeline \"{}\"",
                            pPipeline->getPipelineIdentifier()));
                        error.showError();
                        throw std::runtime_error(error.getFullErrorMessage());
                    }
#endif

                    // Copy shadow pass info index to constants.
                    pMtxRootConstantsManager->second->findOffsetAndCopySpecialValueToConstant(
                        pPipeline.get(),
                        PipelineShaderConstantsManager::SpecialConstantsNames::pShadowPassLightInfoIndex,
                        iIndexIntoShadowPassLightInfoArray);

                    // Get materials.
                    const auto pMtxMaterials = pPipeline->getMaterialsThatUseThisPipeline();
                    std::scoped_lock materialsGuard(pMtxMaterials->first);

                    for (const auto& pMaterial : pMtxMaterials->second) {
                        // No need to bind material's shader resources since they are not used in vertex
                        // shader (since we are in shadow mapping pass).

                        // Get meshes.
                        const auto pMtxMeshNodes = pMaterial->getSpawnedMeshNodesThatUseThisMaterial();
                        std::scoped_lock meshNodesGuard(pMtxMeshNodes->first);

                        // Iterate over all visible mesh nodes that use this material.
                        for (const auto& [pMeshNode, vIndexBuffers] :
                             pMtxMeshNodes->second.visibleMeshNodes) {
                            // Get mesh data.
                            auto pMtxMeshGpuResources = pMeshNode->getMeshGpuResources();
                            auto mtxMeshData = pMeshNode->getMeshData();

                            // Note: if you will ever need it - don't lock mesh node's spawning/despawning
                            // mutex here as it might cause a deadlock (see MeshNode::setMaterial for
                            // example).
                            std::scoped_lock geometryGuard(pMtxMeshGpuResources->first, *mtxMeshData.first);

                            // Find and bind mesh data resource since only it is used in vertex shader.
                            const auto& meshDataIt =
                                pMtxMeshGpuResources->second.shaderResources.shaderCpuWriteResourceBindings
                                    .find(MeshNode::getMeshShaderConstantBufferName());
#if defined(DEBUG)
                            if (meshDataIt == pMtxMeshGpuResources->second.shaderResources
                                                  .shaderCpuWriteResourceBindings.end()) [[unlikely]] {
                                Error error(std::format(
                                    "expected to find \"{}\" shader resource",
                                    MeshNode::getMeshShaderConstantBufferName()));
                                error.showError();
                                throw std::runtime_error(error.getFullErrorMessage());
                            }
#endif
                            meshDataIt->second.getResource()->copyResourceIndexToShaderConstants(
                                pRootConstantsManager, pDirectXPso, iCurrentFrameResourceIndex);

                            // Bind root constants.
                            pCommandList->SetGraphicsRoot32BitConstants(
                                pMtxPsoResources->second.vSpecialRootParameterIndices[static_cast<size_t>(
                                    SpecialRootParameterSlot::ROOT_CONSTANTS)],
                                pRootConstantsManager->getVariableCount(),
                                pRootConstantsManager->getData(),
                                0);

                            // Prepare vertex buffer view.
                            D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
                            vertexBufferView.BufferLocation =
                                reinterpret_cast<DirectXResource*>(
                                    pMtxMeshGpuResources->second.mesh.pVertexBuffer.get())
                                    ->getInternalResource()
                                    ->GetGPUVirtualAddress();
                            vertexBufferView.StrideInBytes = sizeof(MeshVertex);
                            vertexBufferView.SizeInBytes = static_cast<UINT>(
                                mtxMeshData.second->getVertices()->size() * vertexBufferView.StrideInBytes);

                            // Set vertex buffer view.
                            pCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);

                            // Iterate over all index buffers of a specific mesh node that use this
                            // material.
                            for (const auto& indexBufferInfo : vIndexBuffers) {
                                // Prepare index buffer view.
                                static_assert(
                                    sizeof(MeshData::meshindex_t) == sizeof(unsigned int), "change `Format`");
                                D3D12_INDEX_BUFFER_VIEW indexBufferView;
                                indexBufferView.BufferLocation =
                                    reinterpret_cast<DirectXResource*>(indexBufferInfo.pIndexBuffer)
                                        ->getInternalResource()
                                        ->GetGPUVirtualAddress();
                                indexBufferView.Format = DXGI_FORMAT_R32_UINT;
                                indexBufferView.SizeInBytes = static_cast<UINT>(
                                    indexBufferInfo.iIndexCount * sizeof(MeshData::meshindex_t));

                                // Set vertex/index buffer.
                                pCommandList->IASetIndexBuffer(&indexBufferView);

                                // Add a draw command.
                                pCommandList->DrawIndexedInstanced(indexBufferInfo.iIndexCount, 1, 0, 0, 0);

                                // Increment draw call counter.
                                pDrawCallCounter->fetch_add(1);
                            }
                        }
                    }
                }
            }
        };

        {
            // Get directional lights.
            const auto pMtxDirectionalLights =
                getLightingShaderResourceManager()->getDirectionalLightDataArray()->getInternalResources();
            std::scoped_lock directionalLightsGuard(pMtxDirectionalLights->first);

            // Iterate over all directional lights (no culling).
            for (const auto& pLightNode :
                 pMtxDirectionalLights->second.lightsInFrustum.vShaderLightNodeArray) {
                // Convert node type.
                const auto pDirectionalLightNode = reinterpret_cast<DirectionalLightNode*>(pLightNode);

                // Get light info.
                ShadowMapHandle* pShadowMapHandle = nullptr;
                unsigned int iIndexIntoShadowPassInfo = 0;
                Renderer::getDirectionalLightNodeShadowMappingInfo(
                    pDirectionalLightNode, pShadowMapHandle, iIndexIntoShadowPassInfo);

                // Get shadow map texture.
                const auto pMtxShadowResources = pShadowMapHandle->getResources();
                std::scoped_lock shadowMapGuard(pMtxShadowResources->first);

                // Get resource.
                const auto pDepthTexture =
                    reinterpret_cast<DirectXResource*>(pMtxShadowResources->second.pDepthTexture);

                // Transition targets to write.
                transitionDirectionalSpotTargetsToWrite(pDepthTexture);

                // Draw to shadow map.
                drawSceneToShadowMap(
                    shadowMappingDirectionalSpotPipelines, pShadowMapHandle, iIndexIntoShadowPassInfo);

                // Transition targets to read.
                transitionDirectionalSpotTargetsToRead(pDepthTexture);
            }
        }

        {
            // Get spotlights.
            const auto pMtxSpotlights =
                getLightingShaderResourceManager()->getSpotlightDataArray()->getInternalResources();
            std::scoped_lock spotlightsGuard(pMtxSpotlights->first);

            // Iterate only over spotlights in camera's frustum.
            for (const auto& iSpotlightIndex :
                 pMtxSpotlights->second.lightsInFrustum.vLightIndicesInFrustum) {
                // Convert node type.
                const auto pSpotlightNode = reinterpret_cast<SpotlightNode*>(
                    pMtxSpotlights->second.lightsInFrustum.vShaderLightNodeArray[iSpotlightIndex]);

                // Get light info.
                ShadowMapHandle* pShadowMapHandle = nullptr;
                unsigned int iIndexIntoShadowPassInfo = 0;
                Renderer::getSpotlightNodeShadowMappingInfo(
                    pSpotlightNode, pShadowMapHandle, iIndexIntoShadowPassInfo);

                // Get shadow map texture.
                const auto pMtxShadowResources = pShadowMapHandle->getResources();
                std::scoped_lock shadowMapGuard(pMtxShadowResources->first);

                // Get resource.
                const auto pDepthTexture =
                    reinterpret_cast<DirectXResource*>(pMtxShadowResources->second.pDepthTexture);

                // Transition targets to write.
                transitionDirectionalSpotTargetsToWrite(pDepthTexture);

                // Draw to shadow map.
                drawSceneToShadowMap(
                    shadowMappingDirectionalSpotPipelines, pShadowMapHandle, iIndexIntoShadowPassInfo);

                // Transition targets to read.
                transitionDirectionalSpotTargetsToRead(pDepthTexture);
            }
        }

        {
            // Get point lights.
            const auto pMtxPointLights =
                getLightingShaderResourceManager()->getPointLightDataArray()->getInternalResources();
            std::scoped_lock pointLightsGuard(pMtxPointLights->first);

            // Iterate only over point lights in frustum.
            for (const auto& iPointLightIndex :
                 pMtxPointLights->second.lightsInFrustum.vLightIndicesInFrustum) {
                // Convert node type.
                const auto pPointLightNode = reinterpret_cast<PointLightNode*>(
                    pMtxPointLights->second.lightsInFrustum.vShaderLightNodeArray[iPointLightIndex]);

                // Get shadow handle.
                const auto pShadowMapHandle = Renderer::getPointLightNodeShadowMapHandle(pPointLightNode);

                // Get shadow map texture.
                const auto pMtxShadowResources = pShadowMapHandle->getResources();
                std::scoped_lock shadowMapGuard(pMtxShadowResources->first);

                // Get resources.
                const auto pDepthTexture =
                    reinterpret_cast<DirectXResource*>(pMtxShadowResources->second.pDepthTexture);
                const auto pColorTexture =
                    reinterpret_cast<DirectXResource*>(pMtxShadowResources->second.pColorTexture);

                // Transition targets to write.
                transitionPointTargetsToWrite(pDepthTexture, pColorTexture);

                // Draw to each cube shadow map face.
                for (size_t i = 0; i < 6; i++) { // NOLINT: cubemap has 6 faces
                    // Get index into shadow pass info.
                    const auto iIndexIntoShadowPassInfoArray =
                        Renderer::getPointLightShadowPassLightInfoArrayIndex(pPointLightNode, i);

                    // Set render targets.
                    setPointLightTargets(pDepthTexture, pColorTexture, i);

                    // Draw to cubemap's face.
                    drawSceneToShadowMap(
                        shadowMappingPointPipelines, pShadowMapHandle, iIndexIntoShadowPassInfoArray, i);
                }

                // Transitions targets to read.
                transitionPointTargetsToRead(pDepthTexture, pColorTexture);
            }
        }

        // Execute recorded commands.
        executeGraphicsCommandList(pCommandList.Get());
    }

    void DirectXRenderer::executeComputeShadersOnGraphicsQueue(
        FrameResource* pCurrentFrameResource,
        size_t iCurrentFrameResourceIndex,
        ComputeExecutionStage stage) {
        PROFILE_FUNC;

        // Convert frame resource.
        const auto pDirectXFrameResource = reinterpret_cast<DirectXFrameResource*>(pCurrentFrameResource);

        // Get shader groups.
        auto& computeShaderGroups = getPipelineManager()
                                        ->getComputeShadersForGraphicsQueueExecution()
                                        .second->vGraphicsQueueStagesGroups[static_cast<size_t>(stage)];

        // Dispatch compute shaders.
        for (auto& group : computeShaderGroups) {
            dispatchComputeShadersOnGraphicsQueue(pDirectXFrameResource->pCommandAllocator.Get(), group);
        }
    }

    void DirectXRenderer::drawMeshesMainPass(
        FrameResource* pCurrentFrameResource,
        size_t iCurrentFrameResourceIndex,
        const std::vector<MeshesInFrustum::PipelineInFrustumInfo>& vOpaquePipelines,
        const std::vector<MeshesInFrustum::PipelineInFrustumInfo>& vTransparentPipelines) {
        GPU_MARK_FUNC;

        // Convert frame resource.
        const auto pDirectXFrameResource = reinterpret_cast<DirectXFrameResource*>(pCurrentFrameResource);

        // Get shadow map manager.
        const auto pMtxShadowMaps = getResourceManager()->getShadowMapManager()->getInternalResources();
        std::scoped_lock shadowMaps(pMtxShadowMaps->first);

        // Get directional shadow map array.
        const auto pDirectionalShadowMapArray = reinterpret_cast<DirectXShadowMapArrayIndexManager*>(
            pMtxShadowMaps->second
                .vShadowMapArrayIndexManagers[static_cast<size_t>(ShadowMapType::DIRECTIONAL)]
                .get());

        // Get spot shadow map array.
        const auto pSpotShadowMapArray = reinterpret_cast<DirectXShadowMapArrayIndexManager*>(
            pMtxShadowMaps->second.vShadowMapArrayIndexManagers[static_cast<size_t>(ShadowMapType::SPOT)]
                .get());

        // Get point shadow map array.
        const auto pPointShadowMapArray = reinterpret_cast<DirectXShadowMapArrayIndexManager*>(
            pMtxShadowMaps->second.vShadowMapArrayIndexManagers[static_cast<size_t>(ShadowMapType::POINT)]
                .get());

        // Get GPU handles to shadow map arrays.
        // Handles will be valid because while we are inside the `draw` function descriptor heap won't be
        // re-created.
        const auto directionalShadowMapsGpuHandle =
            pDirectionalShadowMapArray->getSrvDescriptorRange()->getGpuDescriptorHandleToRangeStart();
        const auto spotShadowMapsGpuHandle =
            pSpotShadowMapArray->getSrvDescriptorRange()->getGpuDescriptorHandleToRangeStart();
        const auto pointShadowMapsGpuHandle =
            pPointShadowMapArray->getSrvDescriptorRange()->getGpuDescriptorHandleToRangeStart();

        // Prepare command list for main pass.
        resetCommandListForGraphics(pDirectXFrameResource);

        // Get RTV/DSV handles to be used later.
        const auto pCurrentBackBufferResource = getCurrentBackBufferResource();
        const auto renderTargetDescriptorHandle =
            *pCurrentBackBufferResource->getBindedDescriptorCpuHandle(DirectXDescriptorType::RTV);
        const auto depthStencilDescriptorHandle =
            *pDepthStencilBuffer->getBindedDescriptorCpuHandle(DirectXDescriptorType::DSV);

        // Transition render target resource from "present" to "render target".
        const auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
            pCurrentBackBufferResource->getInternalResource(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        pCommandList->ResourceBarrier(1, &transition);

        // Clear render target for main pass.
        pCommandList->ClearRenderTargetView(renderTargetDescriptorHandle, backBufferFillColor, 0, nullptr);

        // Bind both RTV and DSV for main pass.
        pCommandList->OMSetRenderTargets(
            1, &renderTargetDescriptorHandle, FALSE, &depthStencilDescriptorHandle);

        // Draw opaque meshes.
        drawMeshesMainPassSpecificPipelines(
            pDirectXFrameResource,
            iCurrentFrameResourceIndex,
            vOpaquePipelines,
            directionalShadowMapsGpuHandle,
            spotShadowMapsGpuHandle,
            pointShadowMapsGpuHandle,
            false);

        // Draw transparent meshes.
        drawMeshesMainPassSpecificPipelines(
            pDirectXFrameResource,
            iCurrentFrameResourceIndex,
            vTransparentPipelines,
            directionalShadowMapsGpuHandle,
            spotShadowMapsGpuHandle,
            pointShadowMapsGpuHandle,
            true);

        if (bIsUsingMsaaRenderTarget) {
            PROFILE_SCOPE(ResolveMsaaTarget);

            // Resolve MSAA render buffer to swap chain buffer.
            const auto pCurrentSwapChainBuffer =
                vSwapChainBuffers[pSwapChain->GetCurrentBackBufferIndex()].get();
            CD3DX12_RESOURCE_BARRIER barriersToResolve[] = {
                CD3DX12_RESOURCE_BARRIER::Transition(
                    pMsaaRenderBuffer->getInternalResource(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(
                    pCurrentSwapChainBuffer->getInternalResource(),
                    D3D12_RESOURCE_STATE_PRESENT,
                    D3D12_RESOURCE_STATE_RESOLVE_DEST)};

            // Record barriers.
            pCommandList->ResourceBarrier(2, barriersToResolve);

            // Resolve.
            pCommandList->ResolveSubresource(
                pCurrentSwapChainBuffer->getInternalResource(),
                0,
                pMsaaRenderBuffer->getInternalResource(),
                0,
                backBufferFormat);

            // Transition to present.
            CD3DX12_RESOURCE_BARRIER barriersToPresent[] = {
                CD3DX12_RESOURCE_BARRIER::Transition(
                    pMsaaRenderBuffer->getInternalResource(),
                    D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
                    D3D12_RESOURCE_STATE_PRESENT),
                CD3DX12_RESOURCE_BARRIER::Transition(
                    pCurrentSwapChainBuffer->getInternalResource(),
                    D3D12_RESOURCE_STATE_RESOLVE_DEST,
                    D3D12_RESOURCE_STATE_PRESENT)};

            // Record barriers.
            pCommandList->ResourceBarrier(2, barriersToPresent);
        } else {
            PROFILE_SCOPE(TransitionToPresent);

            // Transition render buffer from "render target" to "present".
            auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
                vSwapChainBuffers[pSwapChain->GetCurrentBackBufferIndex()]->getInternalResource(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT);

            // Record barriers.
            pCommandList->ResourceBarrier(1, &transition);
        }

        // Execute recorded commands.
        executeGraphicsCommandList(pCommandList.Get());
    }

    void DirectXRenderer::drawMeshesMainPassSpecificPipelines(
        DirectXFrameResource* pCurrentFrameResource,
        size_t iCurrentFrameResourceIndex,
        const std::vector<Renderer::MeshesInFrustum::PipelineInFrustumInfo>& pipelinesOfSpecificType,
        D3D12_GPU_DESCRIPTOR_HANDLE directionalShadowMapsHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE spotShadowMapsHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE pointShadowMapsGpuHandle,
        const bool bIsDrawingTransparentMeshes) {
        PROFILE_FUNC;

        // Prepare draw call counter to be used later.
        const auto pDrawCallCounter = getDrawCallCounter();

        for (const auto& pipelineInfo : pipelinesOfSpecificType) {
            // Convert pipeline.
            const auto pDirectXPso = reinterpret_cast<DirectXPso*>(pipelineInfo.pPipeline);

            // Get pipeline resources and root constants.
            auto pMtxPsoResources = pDirectXPso->getInternalResources();
            const auto pMtxRootConstantsManager = pDirectXPso->getShaderConstants();

            // Lock both.
            std::scoped_lock guardPsoResources(pMtxPsoResources->first, pMtxRootConstantsManager->first);

            auto& pipelineData = pMtxPsoResources->second;

            // Get root constants manager.
            const auto pRootConstantsManager = pMtxRootConstantsManager->second->pConstantsManager.get();

            // Set PSO and root signature.
            pCommandList->SetPipelineState(pipelineData.pPso.Get());
            pCommandList->SetGraphicsRootSignature(pipelineData.pRootSignature.Get());

            // After setting root signature we can set root parameters.

            // Bind global shader resources.
            pDirectXPso->bindGlobalShaderResourceViews(pCommandList, iCurrentFrameResourceIndex);

            // Set CBV to frame constant buffer.
            pCommandList->SetGraphicsRootConstantBufferView(
                pipelineData
                    .vSpecialRootParameterIndices[static_cast<size_t>(SpecialRootParameterSlot::FRAME_DATA)],
                reinterpret_cast<DirectXResource*>(
                    pCurrentFrameResource->pFrameConstantBuffer->getInternalResource())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());

            // Bind directional shadow maps.
            pCommandList->SetGraphicsRootDescriptorTable(
                pipelineData.vSpecialRootParameterIndices[static_cast<size_t>(
                    SpecialRootParameterSlot::DIRECTIONAL_SHADOW_MAPS)],
                directionalShadowMapsHandle);

            // Bind spot shadow maps.
            pCommandList->SetGraphicsRootDescriptorTable(
                pipelineData.vSpecialRootParameterIndices[static_cast<size_t>(
                    SpecialRootParameterSlot::SPOT_SHADOW_MAPS)],
                spotShadowMapsHandle);

            // Bind point shadow maps.
            pCommandList->SetGraphicsRootDescriptorTable(
                pipelineData.vSpecialRootParameterIndices[static_cast<size_t>(
                    SpecialRootParameterSlot::POINT_SHADOW_MAPS)],
                pointShadowMapsGpuHandle);

            // Bind lighting resources.
            getLightingShaderResourceManager()->setResourceViewToCommandList(
                pDirectXPso, pCommandList, iCurrentFrameResourceIndex);

            // Bind light grid.
            if (bIsDrawingTransparentMeshes) { // TODO: this is a const argument which is known at compile
                                               // time (see how we specify it) so the compiler will probably
                                               // remove this branch
                // Bind transparent light grid and index list.
                getLightingShaderResourceManager()->setTransparentLightGridResourcesViewToCommandList(
                    pDirectXPso, pCommandList);
            } else {
                // Bind opaque light grid and index list.
                getLightingShaderResourceManager()->setOpaqueLightGridResourcesViewToCommandList(
                    pDirectXPso, pCommandList);
            }

            // Bind pipeline's descriptor tables.
            for (const auto& [iRootParameterIndex, pDescriptorRange] : pipelineData.descriptorTablesToBind) {
                pCommandList->SetGraphicsRootDescriptorTable(
                    iRootParameterIndex, pDescriptorRange->getGpuDescriptorHandleToRangeStart());
            }

            for (const auto& materialInfo : pipelineInfo.vMaterials) {
                // Get material's GPU resources.
                const auto pMtxMaterialGpuResources = materialInfo.pMaterial->getMaterialGpuResources();

                // Note: if you will ever need it - don't lock material's internal resources mutex
                // here as it might cause a deadlock (see Material::setDiffuseTexture for example).
                std::scoped_lock materialGpuResourcesGuard(pMtxMaterialGpuResources->first);
                auto& materialResources = pMtxMaterialGpuResources->second.shaderResources;

                // Set material's CPU-write buffers.
                for (const auto& [sResourceName, pShaderCpuWriteResource] :
                     materialResources.shaderCpuWriteResources) {
                    pShaderCpuWriteResource.getResource()->copyResourceIndexToShaderConstants(
                        pRootConstantsManager, pDirectXPso, iCurrentFrameResourceIndex);
                }

                // Set material's textures.
                for (const auto& [sResourceName, pShaderTextureResource] :
                     materialResources.shaderTextureResources) {
                    reinterpret_cast<HlslShaderTextureResourceBinding*>(pShaderTextureResource.getResource())
                        ->copyResourceIndexToRootConstants(pRootConstantsManager, pDirectXPso);
                }

                for (const auto& meshInfo : materialInfo.vMeshes) {
                    // Get mesh data.
                    auto pMtxMeshGpuResources = meshInfo.pMeshNode->getMeshGpuResources();
                    auto mtxMeshData = meshInfo.pMeshNode->getMeshData();

                    // Note: if you will ever need it - don't lock mesh node's spawning/despawning mutex here
                    // as it might cause a deadlock (see MeshNode::setMaterial for example).
                    std::scoped_lock geometryGuard(pMtxMeshGpuResources->first, *mtxMeshData.first);

                    // Set mesh's CPU-write buffers.
                    for (const auto& [sResourceName, pShaderCpuWriteResource] :
                         pMtxMeshGpuResources->second.shaderResources.shaderCpuWriteResourceBindings) {
                        pShaderCpuWriteResource.getResource()->copyResourceIndexToShaderConstants(
                            pRootConstantsManager, pDirectXPso, iCurrentFrameResourceIndex);
                    }

                    // Set mesh's textures.
                    for (const auto& [sResourceName, pShaderTextureResource] :
                         pMtxMeshGpuResources->second.shaderResources.shaderTextureResources) {
                        reinterpret_cast<HlslShaderTextureResourceBinding*>(
                            pShaderTextureResource.getResource())
                            ->copyResourceIndexToRootConstants(pRootConstantsManager, pDirectXPso);
                    }

                    // Bind root constants.
                    pCommandList->SetGraphicsRoot32BitConstants(
                        pMtxPsoResources->second.vSpecialRootParameterIndices[static_cast<size_t>(
                            SpecialRootParameterSlot::ROOT_CONSTANTS)],
                        pRootConstantsManager->getVariableCount(),
                        pRootConstantsManager->getData(),
                        0);

                    // Prepare vertex buffer view.
                    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
                    vertexBufferView.BufferLocation =
                        reinterpret_cast<DirectXResource*>(
                            pMtxMeshGpuResources->second.mesh.pVertexBuffer.get())
                            ->getInternalResource()
                            ->GetGPUVirtualAddress();
                    vertexBufferView.StrideInBytes = sizeof(MeshVertex);
                    vertexBufferView.SizeInBytes = static_cast<UINT>(
                        mtxMeshData.second->getVertices()->size() * vertexBufferView.StrideInBytes);

                    // Set vertex buffer view.
                    pCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);

                    // Iterate over all index buffers of a specific mesh node that use this material.
                    for (const auto& indexBufferInfo : meshInfo.vIndexBuffers) {
                        // Prepare index buffer view.
                        static_assert(
                            sizeof(MeshData::meshindex_t) == sizeof(unsigned int), "change `Format`");
                        D3D12_INDEX_BUFFER_VIEW indexBufferView;
                        indexBufferView.BufferLocation =
                            reinterpret_cast<DirectXResource*>(indexBufferInfo.pIndexBuffer)
                                ->getInternalResource()
                                ->GetGPUVirtualAddress();
                        indexBufferView.Format = DXGI_FORMAT_R32_UINT;
                        indexBufferView.SizeInBytes =
                            static_cast<UINT>(indexBufferInfo.iIndexCount * sizeof(MeshData::meshindex_t));

                        // Set vertex/index buffer.
                        pCommandList->IASetIndexBuffer(&indexBufferView);

                        // Add a draw command.
                        pCommandList->DrawIndexedInstanced(indexBufferInfo.iIndexCount, 1, 0, 0, 0);

                        // Increment draw call counter.
                        pDrawCallCounter->fetch_add(1);
                    }
                }
            }
        }
    }

    void DirectXRenderer::present(FrameResource* pCurrentFrameResource, size_t iCurrentFrameResourceIndex) {
        PROFILE_FUNC;

        // Convert frame resource.
        const auto pDirectXFrameResource = reinterpret_cast<DirectXFrameResource*>(pCurrentFrameResource);

        PROFILE_SCOPE_START(Present);

        // Flip swap chain buffers.
        const auto hResult = pSwapChain->Present(iPresentSyncInterval, iPresentFlags);
        if (FAILED(hResult)) [[unlikely]] {
            Error error(hResult);
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        PROFILE_SCOPE_END;

        // Update fence value.
        UINT64 iNewFenceValue = 0;
        {
            PROFILE_SCOPE(UpdateFenceValue);

            std::scoped_lock fenceGuard(mtxCurrentFenceValue.first);
            mtxCurrentFenceValue.second += 1;
            iNewFenceValue = mtxCurrentFenceValue.second;
        }

        // Save new fence value in the current frame resource.
        pDirectXFrameResource->iFence = iNewFenceValue;

        // Add an instruction to the command queue to set a new fence point.
        // This fence point won't be set until the GPU finishes processing all the commands prior
        // to this `Signal` call.
        pCommandQueue->Signal(pFence.Get(), iNewFenceValue);
    }

    std::optional<Error> DirectXRenderer::updateMsaaQualityLevelCount() {
        // Make sure the device is valid.
        if (pDevice == nullptr) [[unlikely]] {
            return Error("expected device to be valid at this point");
        }

        // Get maximum supported sample count.
        auto result = getMaxSupportedAntialiasingQuality();
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto maxSampleCount = std::get<AntialiasingQuality>(result);

        // First check if AA is supported at all.
        if (maxSampleCount == AntialiasingQuality::DISABLED) {
            // AA is not supported.
            iMsaaQualityLevelsCount = 0;
            return {};
        }
        const auto iMaxSampleCount = static_cast<unsigned int>(maxSampleCount);

        // Get render setting.
        const auto mtxRenderSettings = getRenderSettings();
        std::scoped_lock guard(*mtxRenderSettings.first);

        // Get current AA sample count.
        auto sampleCount = mtxRenderSettings.second->getAntialiasingQuality();
        const auto iSampleCount = static_cast<unsigned int>(sampleCount);

        // Make sure this sample count is supported.
        if (iSampleCount > iMaxSampleCount) [[unlikely]] {
            return Error(std::format("expected the current AA quality {} to be supported", iSampleCount));
        }

        // Prepare to get quality levels for this sample count.
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
        msQualityLevels.Format = backBufferFormat;
        msQualityLevels.SampleCount = iSampleCount;
        msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        msQualityLevels.NumQualityLevels = 0;

        // Query available quality level count.
        auto hResult = pDevice->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels));
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        // Save quality level count.
        iMsaaQualityLevelsCount = msQualityLevels.NumQualityLevels;

        return {};
    }

    void DirectXRenderer::waitForFenceValue(UINT64 iFenceToWaitFor) {
        if (pFence->GetCompletedValue() < iFenceToWaitFor) {
            const auto pEvent = CreateEventEx(nullptr, nullptr, FALSE, EVENT_ALL_ACCESS);
            if (pEvent == nullptr) [[unlikely]] {
                auto error = Error(GetLastError());
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Fire event when the GPU hits current fence.
            const auto hResult = pFence->SetEventOnCompletion(iFenceToWaitFor, pEvent);
            if (FAILED(hResult)) [[unlikely]] {
                auto error = Error(hResult);
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            WaitForSingleObject(pEvent, INFINITE);
            CloseHandle(pEvent);
        }
    }

    void DirectXRenderer::dispatchComputeShadersOnGraphicsQueue(
        ID3D12CommandAllocator* pCommandAllocator,
        std::unordered_map<Pipeline*, std::unordered_set<ComputeShaderInterface*>>&
            computePipelinesToSubmit) {
        // Make sure we have shaders to dispatch.
        if (computePipelinesToSubmit.empty()) {
            return;
        }

        GPU_MARK_FUNC;

        // Open command list to record new commands.
        auto hResult = pComputeCommandList->Reset(pCommandAllocator, nullptr);
        if (FAILED(hResult)) [[unlikely]] {
            Error error(hResult);
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Set CBV/SRV/UAV descriptor heap.
        const auto pResourceManager = reinterpret_cast<DirectXResourceManager*>(getResourceManager());
        ID3D12DescriptorHeap* pDescriptorHeaps[] = {pResourceManager->getCbvSrvUavHeap()->getInternalHeap()};
        pComputeCommandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

        for (const auto& [pPipeline, computeShaders] : computePipelinesToSubmit) {
            // Get internal resources of this pipeline.
            const auto pDirectXPso = reinterpret_cast<DirectXPso*>(pPipeline);
            auto pMtxPsoResources = pDirectXPso->getInternalResources();
            std::scoped_lock guardPsoResources(pMtxPsoResources->first);

            // Set PSO and root signature.
            pComputeCommandList->SetPipelineState(pMtxPsoResources->second.pPso.Get());
            pComputeCommandList->SetComputeRootSignature(pMtxPsoResources->second.pRootSignature.Get());

            // Dispatch each compute shader.
            for (const auto& pComputeInterface : computeShaders) {
                reinterpret_cast<HlslComputeShaderInterface*>(pComputeInterface)
                    ->dispatchOnGraphicsQueue(pComputeCommandList.Get());
            }
        }

        // Execute recorded commands to make sure that compute shaders finish before other compute/rendering
        // work is started because `ExecuteCommandLists` seems to insert an implicit barrier (if I understood
        // the docs correctly).
        executeGraphicsCommandList(pComputeCommandList.Get());

        // Clear map because we submitted all shaders.
        computePipelinesToSubmit.clear();
    }

    size_t DirectXRenderer::rateGpuSuitability(DXGI_ADAPTER_DESC1 adapterDesc) {
        // Skip software adapters.
        if ((adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
            return 0;
        }

        // Prepare a variable for the final score.
        size_t iFinalScore = 0;

        // Add score for dedicated video memory.
        iFinalScore += adapterDesc.DedicatedVideoMemory;

        // Add score for shared video memory (to not skip some integrated GPUs).
        iFinalScore +=
            adapterDesc.SharedSystemMemory / 1024 / 1024; // NOLINT: shared memory is less important

        return iFinalScore;
    }

    std::optional<Error> DirectXRenderer::setOutputAdapter() {
        if (pOutputAdapter != nullptr) {
            return Error("output adapter already created");
        }

        ComPtr<IDXGIOutput> pTestOutput;

        const HRESULT hResult = pVideoAdapter->EnumOutputs(0, pTestOutput.GetAddressOf());
        if (hResult == DXGI_ERROR_NOT_FOUND) {
            return Error("no output adapter was found for current video adapter");
        }

        if (FAILED(hResult)) {
            return Error(hResult);
        }

        pOutputAdapter = std::move(pTestOutput);
        return {};
    }

    std::optional<Error> DirectXRenderer::createCommandQueue() {
        if (pCommandQueue != nullptr) {
            return Error("command queue already created");
        }

        // Create Command Queue.
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

        auto hResult = pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pCommandQueue));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        return {};
    }

    std::optional<Error> DirectXRenderer::createCommandList() {
        if (pCommandList != nullptr) {
            return Error("command list already created");
        }

        // Get command allocator from the current frame resource.
        const auto pFrameResourceManager = getFrameResourceManager();
        if (pFrameResourceManager == nullptr) {
            return Error("frame resource manager needs to be created at this point");
        }
        auto pMtxCurrentFrameResource = getFrameResourceManager()->getCurrentFrameResource();
        std::scoped_lock frameResourceGuard(pMtxCurrentFrameResource->first);

        // Convert frame resource.
        const auto pDirectXFrameResource =
            dynamic_cast<DirectXFrameResource*>(pMtxCurrentFrameResource->second.pResource);
        if (pDirectXFrameResource == nullptr) [[unlikely]] {
            return Error("expected a DirectX frame resource");
        }

        // Create command list using the current command allocator (and ignore others) since in
        // `drawNextFrame` we will change used allocator for the command list anyway.
        auto hResult = pDevice->CreateCommandList(
            0, // Create list for only one GPU. See pDevice->GetNodeCount().
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            pDirectXFrameResource->pCommandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(pCommandList.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Start in a closed state. This is because the first time we refer
        // to the command list (later) we will Reset() it (put in 'Open' state), and in order
        // for Reset() to work, command list should be in closed state.
        pCommandList->Close();

        // Create a separate command list to submit compute work without using the rendering command list.
        hResult = pDevice->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            pDirectXFrameResource->pCommandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(pComputeCommandList.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        pComputeCommandList->Close();

        return {};
    }

    std::optional<Error> DirectXRenderer::createSwapChain() {
        // Get render settings.
        const auto mtxRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(*mtxRenderSettings.first);

        const auto renderResolution = mtxRenderSettings.second->getRenderResolution();
        const auto refreshRate = mtxRenderSettings.second->getRefreshRate();
        const auto bIsVSyncEnabled = mtxRenderSettings.second->isVsyncEnabled();

        DXGI_SWAP_CHAIN_DESC1 desc;
        desc.Width = renderResolution.first;
        desc.Height = renderResolution.second;
        desc.Format = backBufferFormat;
        desc.Stereo = 0;

        // D3D12 apps must use DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL or DXGI_SWAP_EFFECT_FLIP_DISCARD for
        // better performance (from the docs).

        // Flip model swapchains (DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL and DXGI_SWAP_EFFECT_FLIP_DISCARD)
        // do not support multisampling.
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = getRecommendedSwapChainBufferCount();

        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        if (bIsVSyncEnabled) {
            desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        } else {
            desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        DXGI_RATIONAL refreshRateData;
        refreshRateData.Numerator = refreshRate.first;
        refreshRateData.Denominator = refreshRate.second;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc;
        fullscreenDesc.RefreshRate = refreshRateData;
        fullscreenDesc.Scaling = usedScaling;
        fullscreenDesc.ScanlineOrdering = usedScanlineOrdering;
        fullscreenDesc.Windowed = 1;

        const HRESULT hResult = pFactory->CreateSwapChainForHwnd(
            pCommandQueue.Get(),
            getWindow()->getWindowHandle(),
            &desc,
            &fullscreenDesc,
            pOutputAdapter.Get(),
            reinterpret_cast<IDXGISwapChain1**>(pSwapChain.ReleaseAndGetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        return {};
    }

    std::optional<Error>
    DirectXRenderer::initializeDirectX(const std::vector<std::string>& vBlacklistedGpuNames) {
        // Enable debug layer in DEBUG mode.
        DWORD debugFactoryFlags = 0;
#if defined(DEBUG)
        {
            auto error = enableDebugLayer();
            if (error.has_value()) {
                error->addCurrentLocationToErrorStack();
                return error;
            }
            debugFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
        }
#endif

        // Create DXGI factory.
        HRESULT hResult = CreateDXGIFactory2(debugFactoryFlags, IID_PPV_ARGS(&pFactory));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Get render settings.
        // Make sure render settings will not be changed from other frame during initialization.
        const auto mtxRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(*mtxRenderSettings.first);

        // Pick a GPU.
        auto optionalError = pickVideoAdapter(vBlacklistedGpuNames);
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Set output adapter (monitor) to use.
        optionalError = setOutputAdapter();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create device.
        hResult = D3D12CreateDevice(pVideoAdapter.Get(), rendererD3dFeatureLevel, IID_PPV_ARGS(&pDevice));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

#if defined(DEBUG)
        // Create debug message queue for message callback.
        // Apparently the ID3D12InfoQueue1 interface to register message callback is only
        // available on Windows 11 so we should just log the event here using the `info` category.
        hResult = pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
        if (FAILED(hResult)) {
            Logger::get().info("ID3D12InfoQueue1 does not seem to be available on this system, failed to "
                               "query the interface");
        } else {
            // Register debug message callback.
            DWORD unregisterCookie = 0;
            hResult = pInfoQueue->RegisterMessageCallback(
                d3dInfoQueueMessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &unregisterCookie);
            if (FAILED(hResult)) {
                return Error(hResult);
            }
        }
#endif

        // Update render settings.
        optionalError = clampSettingsToMaxSupported();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create fence.
        hResult = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Update supported AA quality level count.
        optionalError = updateMsaaQualityLevelCount();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create command queue.
        optionalError = createCommandQueue();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Create frame resources manager so that we can create a command list
        // that will use command allocator from frame resources.
        optionalError = initializeResourceManagers();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Self check: make sure allocated frame resource is of expected type
        // so we may just `reinterpret_cast` later because they won't change.
        {
            auto mtxAllFrameResource = getFrameResourceManager()->getAllFrameResources();
            std::scoped_lock frameResourceGuard(*mtxAllFrameResource.first);

            for (const auto& pFrameResource : mtxAllFrameResource.second) {
                // Convert frame resource.
                const auto pDirectXFrameResource = dynamic_cast<DirectXFrameResource*>(pFrameResource);
                if (pDirectXFrameResource == nullptr) [[unlikely]] {
                    return Error("expected a DirectX frame resource");
                }
            }
        }

        // Create command list.
        optionalError = createCommandList();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Determine display mode.
        auto videoModesResult = getSupportedDisplayModes();
        if (std::holds_alternative<Error>(videoModesResult)) {
            Error err = std::get<Error>(std::move(videoModesResult));
            err.addCurrentLocationToErrorStack();
            return err;
        }

        // Get preferred screen settings.
        const auto preferredRenderResolution = mtxRenderSettings.second->getRenderResolution();
        const auto preferredRefreshRate = mtxRenderSettings.second->getRefreshRate();

        // Setup video mode.
        DXGI_MODE_DESC pickedDisplayMode;
        if (preferredRenderResolution.first != 0 && preferredRenderResolution.second != 0 &&
            preferredRefreshRate.first != 0 && preferredRefreshRate.second != 0) {
            // Find the specified video mode.
            const auto vVideoModes = std::get<std::vector<DXGI_MODE_DESC>>(std::move(videoModesResult));

            std::vector<DXGI_MODE_DESC> vFilteredModes;

            for (const auto& mode : vVideoModes) {
                if (mode.Width == preferredRenderResolution.first &&
                    mode.Height == preferredRenderResolution.second &&
                    mode.RefreshRate.Numerator == preferredRefreshRate.first &&
                    mode.RefreshRate.Denominator == preferredRefreshRate.second) {
                    vFilteredModes.push_back(mode);
                }
            }

            if (vFilteredModes.empty()) {
                Logger::get().info(std::format(
                    "video mode with resolution {}x{} and refresh rate "
                    "{}/{} is not supported, using default video mode",
                    preferredRenderResolution.first,
                    preferredRenderResolution.second,
                    preferredRefreshRate.first,
                    preferredRefreshRate.second));
                // use last display mode
                pickedDisplayMode = vVideoModes.back();
            } else if (vFilteredModes.size() == 1) {
                // found specified display mode
                pickedDisplayMode = vFilteredModes[0];
            } else {
                std::string sErrorMessage = std::format(
                    "video mode with resolution {}x{} and refresh rate "
                    "{}/{} matched multiple supported modes:\n",
                    preferredRenderResolution.first,
                    preferredRenderResolution.second,
                    preferredRefreshRate.first,
                    preferredRefreshRate.second);
                for (const auto& mode : vFilteredModes) {
                    sErrorMessage += std::format(
                        "- resolution: {}x{}, refresh rate: {}/{}, format: {}, "
                        "scanline ordering: {}, scaling: {}",
                        mode.Width,
                        mode.Height,
                        mode.RefreshRate.Numerator,
                        mode.RefreshRate.Denominator,
                        static_cast<unsigned int>(mode.Format),
                        static_cast<int>(mode.ScanlineOrdering),
                        static_cast<int>(mode.Scaling));
                }
                Logger::get().info(std::format("{}\nusing default video mode", sErrorMessage));
                // use the last display mode
                pickedDisplayMode = vVideoModes.back();
            }
        } else {
            // use last display mode
            pickedDisplayMode = std::get<std::vector<DXGI_MODE_DESC>>(std::move(videoModesResult)).back();
        }

        // Save display settings.
        mtxRenderSettings.second->setRenderResolution(
            std::make_pair(pickedDisplayMode.Width, pickedDisplayMode.Height));
        mtxRenderSettings.second->setRefreshRate(std::make_pair(
            pickedDisplayMode.RefreshRate.Numerator, pickedDisplayMode.RefreshRate.Denominator));

        // Create swap chain.
        optionalError = createSwapChain();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Log used GPU name.
        Logger::get().info(std::format("using the following GPU: \"{}\"", getCurrentlyUsedGpuName()));

        bIsDirectXInitialized = true;

        return {};
    }

    IDXGIAdapter3* DirectXRenderer::getVideoAdapter() const { return pVideoAdapter.Get(); }

    UINT DirectXRenderer::getMsaaQualityLevel() const { return iMsaaQualityLevelsCount; }

    GpuResource* DirectXRenderer::getDepthTextureNoMultisampling() {
        std::scoped_lock guard(
            *getRenderResourcesMutex()); // `bIsUsingMsaaRenderTarget` is only changed under this mutex

        if (bIsUsingMsaaRenderTarget) {
            // Depth buffer uses multisampling so return a pointer to the resolved depth resource.
            return pDepthBufferNoMultisampling.get();
        }

        // Depth buffer does not use multisampling so just return it.
        return pDepthStencilBuffer.get();
    }

    std::pair<unsigned int, unsigned int> DirectXRenderer::getRenderTargetSize() const {
        return renderTargetSize;
    }

    std::optional<Error> DirectXRenderer::onRenderSettingsChangedDerived() {
        // Make sure no rendering is happening.
        std::scoped_lock guard(*getRenderResourcesMutex());
        waitForGpuToFinishWorkUpToThisPoint();

        // Get render settings.
        const auto mtxRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(*mtxRenderSettings.first);

        const auto renderResolution = mtxRenderSettings.second->getRenderResolution();
        const auto bIsVSyncEnabled = mtxRenderSettings.second->isVsyncEnabled();
        const auto iMsaaSampleCount =
            static_cast<unsigned int>(mtxRenderSettings.second->getAntialiasingQuality());

        // Update supported AA quality level count.
        auto optionalError = updateMsaaQualityLevelCount();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Swapchain cannot be resized unless all outstanding buffer references have been released.
        vSwapChainBuffers.clear();
        vSwapChainBuffers.resize(getRecommendedSwapChainBufferCount());

        // Apply VSync state for `Present` calls.
        const auto iOldPresentSyncInterval = iPresentSyncInterval;
        if (bIsVSyncEnabled) {
            iPresentSyncInterval = 1;
            iPresentFlags = 0; // prevent tearing
        } else {
            iPresentSyncInterval = 0;
            iPresentFlags = DXGI_PRESENT_ALLOW_TEARING;
        }
        if (iOldPresentSyncInterval != iPresentSyncInterval) {
            // VSync state changed, recreate the swap chain to allow/disallow tearing
            // (i.e. add/remove `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` flag because we can't
            // append/remove it using the `ResizeBuffers` function).
            const auto optionalError = createSwapChain();
            if (optionalError.has_value()) [[unlikely]] {
                auto err = optionalError.value();
                err.addCurrentLocationToErrorStack();
                return err;
            }
        }

        // Resize the swap chain.
        auto hResult = pSwapChain->ResizeBuffers(
            getRecommendedSwapChainBufferCount(),
            renderResolution.first,
            renderResolution.second,
            backBufferFormat,
            bIsVSyncEnabled ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
                            : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        // Save new render target size.
        renderTargetSize = renderResolution;

        // Create RTV to swap chain buffers.
        auto swapChainResult =
            dynamic_cast<DirectXResourceManager*>(getResourceManager())
                ->makeRtvResourcesFromSwapChainBuffer(pSwapChain.Get(), getRecommendedSwapChainBufferCount());
        if (std::holds_alternative<Error>(swapChainResult)) {
            auto error = std::get<Error>(std::move(swapChainResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        vSwapChainBuffers =
            std::get<std::vector<std::unique_ptr<DirectXResource>>>(std::move(swapChainResult));

        // Setup MSAA render target.
        bIsUsingMsaaRenderTarget = iMsaaSampleCount > 1;
        if (bIsUsingMsaaRenderTarget) {
            // Create MSAA render target.
            const auto msaaRenderTargetDesc = CD3DX12_RESOURCE_DESC(
                D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                0,
                renderResolution.first,
                renderResolution.second,
                1,
                1,
                backBufferFormat,
                iMsaaSampleCount,
                iMsaaQualityLevelsCount - 1,
                D3D12_TEXTURE_LAYOUT_UNKNOWN,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

            D3D12_CLEAR_VALUE msaaClear;
            msaaClear.Format = backBufferFormat;
            msaaClear.Color[0] = backBufferFillColor[0];
            msaaClear.Color[1] = backBufferFillColor[1];
            msaaClear.Color[2] = backBufferFillColor[2];
            msaaClear.Color[3] = backBufferFillColor[3];

            D3D12MA::ALLOCATION_DESC allocationDesc = {};
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

            auto result = dynamic_cast<DirectXResourceManager*>(getResourceManager())
                              ->createResource(
                                  "renderer MSAA render buffer",
                                  allocationDesc,
                                  msaaRenderTargetDesc,
                                  D3D12_RESOURCE_STATE_COMMON,
                                  msaaClear);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                return err;
            }
            pMsaaRenderBuffer = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            // Bind RTV.
            auto optionalError = pMsaaRenderBuffer->bindDescriptor(DirectXDescriptorType::RTV);
            if (optionalError.has_value()) {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }
        } else {
            pMsaaRenderBuffer = nullptr;
        }

        // Create depth/stencil buffer.
        optionalError = createDepthStencilBuffer();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        // Update the viewport transform to cover the new display size.
        screenViewport.TopLeftX = 0;
        screenViewport.TopLeftY = 0;
        screenViewport.Width = static_cast<float>(renderResolution.first);
        screenViewport.Height = static_cast<float>(renderResolution.second);
        screenViewport.MinDepth = getMinDepth();
        screenViewport.MaxDepth = getMaxDepth();

        scissorRect.left = 0;
        scissorRect.top = 0;
        scissorRect.right = static_cast<LONG>(renderResolution.first);
        scissorRect.bottom = static_cast<LONG>(renderResolution.second);

        // Recreate all pipelines' internal resources so they will now use new multisampling settings.
        optionalError = getPipelineManager()->recreateGraphicsPipelinesResources();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        // Update light culling resources.
        optionalError = recalculateLightTileFrustums();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return {};
    }

    void DirectXRenderer::waitForGpuToFinishUsingFrameResource(FrameResource* pFrameResource) {
        // Convert frame resource.
        const auto pDirectXFrameResource = reinterpret_cast<DirectXFrameResource*>(pFrameResource);

        // Wait for this frame resource to no longer be used by the GPU.
        waitForFenceValue(pDirectXFrameResource->iFence);
    }

    std::variant<AntialiasingQuality, Error> DirectXRenderer::getMaxSupportedAntialiasingQuality() const {
        if (pDevice == nullptr) [[unlikely]] {
            return Error("expected device to be valid at this point");
        }

        // Prepare info to check.
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
        msQualityLevels.Format = backBufferFormat;
        msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        msQualityLevels.NumQualityLevels = 0;

        const std::array<AntialiasingQuality, 2> vQualityToCheck = {
            AntialiasingQuality::HIGH, AntialiasingQuality::MEDIUM};

        // Find the maximum supported quality.
        for (const auto& quality : vQualityToCheck) {
            // Check hardware support for this sample count.
            msQualityLevels.SampleCount = static_cast<unsigned int>(quality);
            auto hResult = pDevice->CheckFeatureSupport(
                D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels));
            if (FAILED(hResult)) [[unlikely]] {
                // Something went wrong.
                return Error(hResult);
            }
            if (msQualityLevels.NumQualityLevels != 0) {
                return quality;
            }
        }

        return AntialiasingQuality::DISABLED;
    }

    bool DirectXRenderer::isInitialized() const { return bIsDirectXInitialized; }

    void DirectXRenderer::executeGraphicsCommandList(ID3D12GraphicsCommandList* pCommandListToExecute) {
        PROFILE_FUNC;

        // Close command list.
        const auto hResult = pCommandListToExecute->Close();
        if (FAILED(hResult)) [[unlikely]] {
            Error error(hResult);
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Execute recorded commands.
        const std::array<ID3D12CommandList*, 1> vCommandLists = {pCommandListToExecute};
        pCommandQueue->ExecuteCommandLists(static_cast<UINT>(vCommandLists.size()), vCommandLists.data());
    }

    void DirectXRenderer::waitForGpuToFinishWorkUpToThisPoint() {
        if (pCommandQueue == nullptr) {
            if (GameManager::get() == nullptr || GameManager::get()->isBeingDestroyed()) {
                // This might happen on destruction, it's fine.
                return;
            }

            // This is unexpected.
            Logger::get().error(
                "attempt to flush the command queue failed due to command queue being `nullptr`");

            return;
        }

        UINT64 iFenceToWait = 0;
        {
            // Make sure we are not submitting a new frame right now.
            std::scoped_lock guard(*getRenderResourcesMutex(), mtxCurrentFenceValue.first);

            mtxCurrentFenceValue.second += 1;
            iFenceToWait = mtxCurrentFenceValue.second;
        }

        // Add to queue.
        auto hResult = pCommandQueue->Signal(pFence.Get(), iFenceToWait);
        if (FAILED(hResult)) [[unlikely]] {
            auto error = Error(hResult);
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        waitForFenceValue(iFenceToWait);
    }

    RendererType DirectXRenderer::getType() const { return RendererType::DIRECTX; }

    std::string DirectXRenderer::getUsedApiVersion() const {
        static_assert(
            rendererD3dFeatureLevel == D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_12_1,
            "update returned version string");
        return "12.1";
    }

    ID3D12Device* DirectXRenderer::getD3dDevice() const { return pDevice.Get(); }

    ID3D12GraphicsCommandList* DirectXRenderer::getD3dCommandList() { return pCommandList.Get(); }

    ID3D12CommandQueue* DirectXRenderer::getD3dCommandQueue() { return pCommandQueue.Get(); }

    std::variant<std::vector<DXGI_MODE_DESC>, Error> DirectXRenderer::getSupportedDisplayModes() const {
        if (pOutputAdapter == nullptr) {
            return Error("output adapter is not created");
        }

        UINT iDisplayModeCount = 0;

        // Get amount of display modes first.
        HRESULT hResult =
            pOutputAdapter->GetDisplayModeList(backBufferFormat, 0, &iDisplayModeCount, nullptr);
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Get display modes.
        std::vector<DXGI_MODE_DESC> vDisplayModes(iDisplayModeCount);
        hResult =
            pOutputAdapter->GetDisplayModeList(backBufferFormat, 0, &iDisplayModeCount, vDisplayModes.data());
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Filter some modes.
        std::vector<DXGI_MODE_DESC> vFilteredModes;
        for (const auto& mode : vDisplayModes) {
            if (mode.Scaling != usedScaling) {
                continue;
            }
            if (mode.ScanlineOrdering != usedScanlineOrdering) {
                continue;
            }

            vFilteredModes.push_back(mode);
        }

        return vFilteredModes;
    }

    DirectXResource* DirectXRenderer::getCurrentBackBufferResource() {
        if (bIsUsingMsaaRenderTarget) {
            return pMsaaRenderBuffer.get();
        }

        return vSwapChainBuffers[pSwapChain->GetCurrentBackBufferIndex()].get();
    }
} // namespace ne
