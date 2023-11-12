#include "DirectXRenderer.h"

// Standard.
#include <format>
#include <filesystem>
#include <array>
#include <future>

// Custom.
#include "game/Window.h"
#include "io/Logger.h"
#include "misc/Globals.h"
#include "render/RenderSettings.h"
#include "render/directx/pipeline/DirectXPso.h"
#include "render/directx/resources/DirectXResourceManager.h"
#include "shader/hlsl/HlslEngineShaders.hpp"
#include "render/general/pipeline/PipelineManager.h"
#include "render/directx/resources/DirectXResource.h"
#include "material/Material.h"
#include "game/nodes/MeshNode.h"
#include "shader/hlsl/RootSignatureGenerator.h"
#include "render/general/resources/FrameResourcesManager.h"
#include "game/camera/CameraProperties.h"
#include "game/camera/CameraManager.h"
#include "game/nodes/CameraNode.h"
#include "game/camera/TransientCamera.h"
#include "shader/hlsl/resources/HlslShaderCpuWriteResource.h"
#include "shader/hlsl/resources/HlslShaderTextureResource.h"
#include "render/directx/resources/DirectXFrameResource.h"
#include "shader/hlsl/HlslComputeShaderInterface.h"
#include "misc/Profiler.hpp"

// DirectX.
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#if defined(DEBUG) || defined(_DEBUG)
#include <dxgidebug.h>
#include <InitGuid.h>
#pragma comment(lib, "dxguid.lib")
#endif

namespace ne {
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
        static_assert(
            backBufferFormat == DXGI_FORMAT_R8G8B8A8_UNORM,
            "also change format in Vulkan renderer for (visual) consistency");
        static_assert(
            depthStencilBufferFormat == DXGI_FORMAT_D24_UNORM_S8_UINT,
            "also change format in Vulkan renderer for (visual) consistency");
    }

    std::vector<ShaderDescription> DirectXRenderer::getEngineShadersToCompile() const {
        return {HlslEngineShaders::meshNodeVertexShader, HlslEngineShaders::meshNodePixelShader};
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

    std::optional<Error> DirectXRenderer::enableDebugLayer() {
#if defined(DEBUG)
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
#endif
        return {};
    }

    std::optional<Error> DirectXRenderer::createDepthStencilBuffer() {
        // Get render settings.
        const auto pMtxRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(pMtxRenderSettings->first);
        const auto renderResolution = pMtxRenderSettings->second->getRenderResolution();
        const auto iMsaaSampleCount = static_cast<int>(pMtxRenderSettings->second->getAntialiasingState());

        // Prepare resource description.
        const D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC(
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
        depthClear.DepthStencil.Depth = 1.0F;
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
        const auto pMtxRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(pMtxRenderSettings->first);

        // Check if the GPU to use is set.
        auto sGpuNameToUse = pMtxRenderSettings->second->getGpuToUse();
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
            pMtxRenderSettings->second->setGpuToUse(currentGpuInfo.sGpuName);

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

    std::optional<Error> DirectXRenderer::prepareForDrawingNextFrame(
        CameraProperties* pCameraProperties, DirectXFrameResource* pCurrentFrameResource) {
        PROFILE_FUNC;

        // Waits for frame resource to be no longer used by the GPU.
        updateResourcesForNextFrame(scissorRect.right, scissorRect.bottom, pCameraProperties);

        // Get command allocator.
        const auto pCommandAllocator = pCurrentFrameResource->pCommandAllocator.Get();

        PROFILE_SCOPE_START(ResetCommandAllocator);

        // Clear command allocator since the GPU is no longer using it.
        auto hResult = pCommandAllocator->Reset();
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        PROFILE_SCOPE_END;

        return {};
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

    void DirectXRenderer::drawNextFrame() {
        PROFILE_FUNC;

        // Get pipeline manager and compute shaders to dispatch.
        const auto pPipelineManager = getPipelineManager();
        auto mtxQueuedComputeShader = pPipelineManager->getComputeShadersForGraphicsQueueExecution();

        // Get active camera.
        const auto pMtxActiveCamera = getGameManager()->getCameraManager()->getActiveCamera();

        // Get current frame resource.
        auto pMtxCurrentFrameResource = getFrameResourcesManager()->getCurrentFrameResource();
        const auto pCurrentFrameResource =
            reinterpret_cast<DirectXFrameResource*>(pMtxCurrentFrameResource->second.pResource);

        // Lock mutexes together to minimize deadlocks.
        std::scoped_lock renderGuard(
            pMtxActiveCamera->first,
            *getRenderResourcesMutex(),
            pMtxCurrentFrameResource->first,
            *mtxQueuedComputeShader.first);

        // Get camera properties of the active camera.
        CameraProperties* pActiveCameraProperties = nullptr;
        if (pMtxActiveCamera->second.pCameraNode != nullptr) {
            pActiveCameraProperties = pMtxActiveCamera->second.pCameraNode->getCameraProperties();
        } else if (pMtxActiveCamera->second.pTransientCamera != nullptr) {
            pActiveCameraProperties = pMtxActiveCamera->second.pTransientCamera->getCameraProperties();
        } else {
            // No active camera.
            return;
        }

        // don't unlock active camera mutex until finished submitting the next frame for drawing

        // Setup.
        auto optionalError = prepareForDrawingNextFrame(pActiveCameraProperties, pCurrentFrameResource);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Prepare command list.
        resetCommandListForGraphics(pCurrentFrameResource);

        // Get RTV/DSV handles to be used later.
        const auto pCurrentBackBufferResource = getCurrentBackBufferResource();
        const auto renderTargetDescriptorHandle =
            *pCurrentBackBufferResource->getBindedDescriptorHandle(DirectXDescriptorType::RTV);
        const auto depthStencilDescriptorHandle =
            *pDepthStencilBuffer->getBindedDescriptorHandle(DirectXDescriptorType::DSV);

        // Get graphics pipelines.
        const auto pMtxGraphicsPipelines = pPipelineManager->getGraphicsPipelines();
        std::scoped_lock pipelinesGuard(pMtxGraphicsPipelines->first);

        // Do frustum culling.
        const auto pMeshesInFrustum =
            getMeshesInFrustum(pActiveCameraProperties, &pMtxGraphicsPipelines->second);

        // Transition depth buffer to write state.
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
            pDepthStencilBuffer->getInternalResource(),
            D3D12_RESOURCE_STATE_DEPTH_READ,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        pCommandList->ResourceBarrier(1, &transition);

        // Clear depth stencil for depth pass.
        pCommandList->ClearDepthStencilView(
            depthStencilDescriptorHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            getMaxDepth(),
            0,
            0,
            nullptr);

        // Bind only DSV for depth prepass.
        pCommandList->OMSetRenderTargets(0, nullptr, FALSE, &depthStencilDescriptorHandle);

        // Draw depth prepass.
        drawMeshesDepthPrepass(
            pCurrentFrameResource,
            pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex,
            pMeshesInFrustum->vOpaquePipelines);

        // Transition depth buffer to read only state.
        transition = CD3DX12_RESOURCE_BARRIER::Transition(
            pDepthStencilBuffer->getInternalResource(),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_DEPTH_READ);
        pCommandList->ResourceBarrier(1, &transition);

        // Execute recorded commands.
        executeGraphicsCommandList(pCommandList.Get());

        PROFILE_SCOPE_START(DispatchPreFrameComputeShaders);

        // Dispatch pre-frame compute shaders.
        for (auto& group : mtxQueuedComputeShader.second->graphicsQueuePreFrameShadersGroups) {
            dispatchComputeShadersOnGraphicsQueue(pCurrentFrameResource->pCommandAllocator.Get(), group);
        }

        PROFILE_SCOPE_END;

        // Prepare command list for main pass.
        resetCommandListForGraphics(pCurrentFrameResource);

        // Transition render target resource from "present" to "render target".
        transition = CD3DX12_RESOURCE_BARRIER::Transition(
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
        drawMeshesMainPass(
            pCurrentFrameResource,
            pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex,
            pMeshesInFrustum->vOpaquePipelines);

        // Draw transparent meshes.
        drawMeshesMainPass(
            pCurrentFrameResource,
            pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex,
            pMeshesInFrustum->vTransparentPipelines);

        // Do frame end logic.
        optionalError = finishDrawingNextFrame(pCurrentFrameResource, mtxQueuedComputeShader.second);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Switch to the next frame resource.
        getFrameResourcesManager()->switchToNextFrameResource();
    }

    void DirectXRenderer::drawMeshesDepthPrepass(
        DirectXFrameResource* pCurrentFrameResource,
        size_t iCurrentFrameResourceIndex,
        const std::vector<Renderer::MeshesInFrustum::PipelineInFrustumInfo>& opaquePipelines) {
        PROFILE_FUNC;

        // Prepare draw call counter to be used later.
        const auto pDrawCallCounter = getDrawCallCounter();

        for (const auto& pipelineInfo : opaquePipelines) {
            // Get depth only (vertex shader only) pipeline
            // (all materials that use the same opaque pipeline will use the same depth only pipeline).
            const auto pDepthOnlyPipeline = pipelineInfo.vMaterials[0].pMaterial->getDepthOnlyPipeline();

            // Get internal resources of this pipeline.
            const auto pDirectXPso = reinterpret_cast<DirectXPso*>(pDepthOnlyPipeline);
            auto pMtxPsoResources = pDirectXPso->getInternalResources();
            std::scoped_lock guardPsoResources(pMtxPsoResources->first);

            // Set PSO and root signature.
            pCommandList->SetPipelineState(pMtxPsoResources->second.pPso.Get());
            pCommandList->SetGraphicsRootSignature(pMtxPsoResources->second.pRootSignature.Get());

            // After setting root signature we can set root parameters.

            // Set CBV to frame constant buffer.
            pCommandList->SetGraphicsRootConstantBufferView(
                RootSignatureGenerator::getFrameConstantBufferRootParameterIndex(),
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
                        pMtxMeshGpuResources->second.shaderResources.shaderCpuWriteResources.find(
                            MeshNode::getMeshShaderConstantBufferName());
#if defined(DEBUG)
                    if (meshDataIt ==
                        pMtxMeshGpuResources->second.shaderResources.shaderCpuWriteResources.end())
                        [[unlikely]] {
                        Error error(std::format(
                            "expected to find \"{}\" shader resource",
                            MeshNode::getMeshShaderConstantBufferName()));
                        error.showError();
                        throw std::runtime_error(error.getFullErrorMessage());
                    }
#endif
                    // Set resource.
                    reinterpret_cast<HlslShaderCpuWriteResource*>(meshDataIt->second.getResource())
                        ->setConstantBufferViewOfPipeline(
                            pCommandList, pDirectXPso, iCurrentFrameResourceIndex);

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
                        *pDrawCallCounter += 1;
                    }
                }
            }
        }
    }

    void DirectXRenderer::drawMeshesMainPass(
        DirectXFrameResource* pCurrentFrameResource,
        size_t iCurrentFrameResourceIndex,
        const std::vector<Renderer::MeshesInFrustum::PipelineInFrustumInfo>& pipelinesOfSpecificType) {
        PROFILE_FUNC;

        // Prepare draw call counter to be used later.
        const auto pDrawCallCounter = getDrawCallCounter();

        for (const auto& pipelineInfo : pipelinesOfSpecificType) {
            // Get internal resources of this pipeline.
            const auto pDirectXPso = reinterpret_cast<DirectXPso*>(pipelineInfo.pPipeline);
            auto pMtxPsoResources = pDirectXPso->getInternalResources();
            std::scoped_lock guardPsoResources(pMtxPsoResources->first);

            // Set PSO and root signature.
            pCommandList->SetPipelineState(pMtxPsoResources->second.pPso.Get());
            pCommandList->SetGraphicsRootSignature(pMtxPsoResources->second.pRootSignature.Get());

            // After setting root signature we can set root parameters.

            // Set CBV to frame constant buffer.
            pCommandList->SetGraphicsRootConstantBufferView(
                RootSignatureGenerator::getFrameConstantBufferRootParameterIndex(),
                reinterpret_cast<DirectXResource*>(
                    pCurrentFrameResource->pFrameConstantBuffer->getInternalResource())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());

            // Bind lighting resources.
            getLightingShaderResourceManager()->setResourceViewToCommandList(
                pDirectXPso, pCommandList, iCurrentFrameResourceIndex);

            for (const auto& materialInfo : pipelineInfo.vMaterials) {
                // Get material's GPU resources.
                const auto pMtxMaterialGpuResources = materialInfo.pMaterial->getMaterialGpuResources();

                // Note: if you will ever need it - don't lock material's internal resources mutex
                // here as it might cause a deadlock (see Material::setDiffuseTexture for example).
                std::scoped_lock materialGpuResourcesGuard(pMtxMaterialGpuResources->first);
                auto& materialResources = pMtxMaterialGpuResources->second.shaderResources;

                // Set material's CPU write shader resources (`cbuffer`s for example).
                for (const auto& [sResourceName, pShaderCpuWriteResource] :
                     materialResources.shaderCpuWriteResources) {
                    reinterpret_cast<HlslShaderCpuWriteResource*>(pShaderCpuWriteResource.getResource())
                        ->setConstantBufferViewOfOnlyPipeline(pCommandList, iCurrentFrameResourceIndex);
                }

                // Set material's texture shader resources (`Texture2D`s for example).
                for (const auto& [sResourceName, pShaderTextureResource] :
                     materialResources.shaderTextureResources) {
                    reinterpret_cast<HlslShaderTextureResource*>(pShaderTextureResource.getResource())
                        ->setGraphicsRootDescriptorTableOfOnlyPipeline(pCommandList);
                }

                for (const auto& meshInfo : materialInfo.vMeshes) {
                    // Get mesh data.
                    auto pMtxMeshGpuResources = meshInfo.pMeshNode->getMeshGpuResources();
                    auto mtxMeshData = meshInfo.pMeshNode->getMeshData();

                    // Note: if you will ever need it - don't lock mesh node's spawning/despawning mutex here
                    // as it might cause a deadlock (see MeshNode::setMaterial for example).
                    std::scoped_lock geometryGuard(pMtxMeshGpuResources->first, *mtxMeshData.first);

                    // Set mesh's shader CPU write resources (`cbuffer`s for example).
                    for (const auto& [sResourceName, pShaderCpuWriteResource] :
                         pMtxMeshGpuResources->second.shaderResources.shaderCpuWriteResources) {
                        reinterpret_cast<HlslShaderCpuWriteResource*>(pShaderCpuWriteResource.getResource())
                            ->setConstantBufferViewOfPipeline(
                                pCommandList, pDirectXPso, iCurrentFrameResourceIndex);
                    }

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
                        *pDrawCallCounter += 1;
                    }
                }
            }
        }
    }

    std::optional<Error> DirectXRenderer::finishDrawingNextFrame(
        DirectXFrameResource* pCurrentFrameResource,
        QueuedForExecutionComputeShaders* pQueuedComputeShaders) {
        PROFILE_FUNC;

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

            CD3DX12_RESOURCE_BARRIER barriersToPresent[] = {
                CD3DX12_RESOURCE_BARRIER::Transition(
                    pMsaaRenderBuffer->getInternalResource(),
                    D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
                    D3D12_RESOURCE_STATE_PRESENT),
                CD3DX12_RESOURCE_BARRIER::Transition(
                    pCurrentSwapChainBuffer->getInternalResource(),
                    D3D12_RESOURCE_STATE_RESOLVE_DEST,
                    D3D12_RESOURCE_STATE_PRESENT)};

            pCommandList->ResourceBarrier(2, barriersToResolve);

            pCommandList->ResolveSubresource(
                pCurrentSwapChainBuffer->getInternalResource(),
                0,
                pMsaaRenderBuffer->getInternalResource(),
                0,
                backBufferFormat);

            pCommandList->ResourceBarrier(2, barriersToPresent);
        } else {
            PROFILE_SCOPE(TransitionToPresent);

            // Transition render buffer from "render target" to "present".
            auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
                vSwapChainBuffers[pSwapChain->GetCurrentBackBufferIndex()]->getInternalResource(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT);
            pCommandList->ResourceBarrier(1, &transition);
        }

        // Execute recorded commands.
        executeGraphicsCommandList(pCommandList.Get());

        PROFILE_SCOPE_START(DispatchPostFrameComputeShaders);

        // Dispatch all post-frame compute shaders.
        for (auto& group : pQueuedComputeShaders->graphicsQueuePostFrameShadersGroups) {
            dispatchComputeShadersOnGraphicsQueue(pCurrentFrameResource->pCommandAllocator.Get(), group);
        }

        PROFILE_SCOPE_END;

        PROFILE_SCOPE_START(Present);

        // Flip swap chain buffers.
        const auto hResult = pSwapChain->Present(iPresentSyncInterval, iPresentFlags);
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
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
        pCurrentFrameResource->iFence = iNewFenceValue;

        // Add an instruction to the command queue to set a new fence point.
        // This fence point won't be set until the GPU finishes processing all the commands prior
        // to this `Signal` call.
        pCommandQueue->Signal(pFence.Get(), iNewFenceValue);

        // Calculate frame-related statistics.
        calculateFrameStatistics();

        return {};
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
        auto maxSampleCount = std::get<MsaaState>(result);

        // First check if AA is supported at all.
        if (maxSampleCount == MsaaState::DISABLED) {
            // AA is not supported.
            iMsaaQualityLevelsCount = 0;
            return {};
        }
        const auto iMaxSampleCount = static_cast<int>(maxSampleCount);

        // Get render setting.
        const auto pMtxRenderSettings = getRenderSettings();
        std::scoped_lock guard(pMtxRenderSettings->first);

        // Get current AA sample count.
        auto sampleCount = pMtxRenderSettings->second->getAntialiasingState();
        const auto iSampleCount = static_cast<int>(sampleCount);

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

        // Execute recorded commands.
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
        const auto pFrameResourcesManager = getFrameResourcesManager();
        if (pFrameResourcesManager == nullptr) {
            return Error("frame resources manager needs to be created at this point");
        }
        auto pMtxCurrentFrameResource = getFrameResourcesManager()->getCurrentFrameResource();
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
        const auto pMtxRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(pMtxRenderSettings->first);
        const auto renderResolution = pMtxRenderSettings->second->getRenderResolution();
        const auto refreshRate = pMtxRenderSettings->second->getRefreshRate();
        const auto bIsVSyncEnabled = pMtxRenderSettings->second->isVsyncEnabled();

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
        const auto pMtxRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(pMtxRenderSettings->first);

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
        // available on Windows 11 so we should just log the error here using the `info` category.
        hResult = pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
        if (FAILED(hResult)) {
            const auto error = Error(hResult);
            Logger::get().info(std::format(
                "ID3D12InfoQueue1 does not seem to be available on this system, failed to query the "
                "interface: {}",
                error.getInitialMessage()));
        } else {
            // Register debug message callback.
            DWORD* pUnregisterCookie = nullptr;
            hResult = pInfoQueue->RegisterMessageCallback(
                d3dInfoQueueMessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, pUnregisterCookie);
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
            auto mtxAllFrameResource = getFrameResourcesManager()->getAllFrameResources();
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
        const auto preferredRenderResolution = pMtxRenderSettings->second->getRenderResolution();
        const auto preferredRefreshRate = pMtxRenderSettings->second->getRefreshRate();

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
        pMtxRenderSettings->second->setRenderResolution(
            std::make_pair(pickedDisplayMode.Width, pickedDisplayMode.Height));
        pMtxRenderSettings->second->setRefreshRate(std::make_pair(
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

    DXGI_FORMAT DirectXRenderer::getBackBufferFormat() { return backBufferFormat; }

    DXGI_FORMAT DirectXRenderer::getDepthStencilBufferFormat() { return depthStencilBufferFormat; }

    UINT DirectXRenderer::getMsaaQualityLevel() const { return iMsaaQualityLevelsCount; }

    std::optional<Error> DirectXRenderer::onRenderSettingsChanged() {
        // Make sure no rendering is happening.
        std::scoped_lock guard(*getRenderResourcesMutex());
        waitForGpuToFinishWorkUpToThisPoint();

        // Call parent's version.
        auto optionalError = Renderer::onRenderSettingsChanged();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Get render settings.
        const auto pMtxRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(pMtxRenderSettings->first);
        const auto renderResolution = pMtxRenderSettings->second->getRenderResolution();
        const auto bIsVSyncEnabled = pMtxRenderSettings->second->isVsyncEnabled();
        const auto iMsaaSampleCount = static_cast<int>(pMtxRenderSettings->second->getAntialiasingState());

        // Update supported AA quality level count.
        optionalError = updateMsaaQualityLevelCount();
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

        // Create RTV to swap chain buffers.
        auto swapChainResult =
            dynamic_cast<DirectXResourceManager*>(getResourceManager())
                ->makeRtvResourcesFromSwapChainBuffer(pSwapChain.Get(), getRecommendedSwapChainBufferCount());
        if (std::holds_alternative<Error>(swapChainResult)) {
            auto err = std::get<Error>(std::move(swapChainResult));
            err.addCurrentLocationToErrorStack();
            return err;
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
        {
            const auto psoGuard =
                getPipelineManager()->clearGraphicsPipelinesInternalResourcesAndDelayRestoring();
        }

        return {};
    }

    void DirectXRenderer::waitForGpuToFinishUsingFrameResource(FrameResource* pFrameResource) {
        // Convert frame resource.
        const auto pDirectXFrameResource = reinterpret_cast<DirectXFrameResource*>(pFrameResource);

        // Wait for this frame resource to no longer be used by the GPU.
        waitForFenceValue(pDirectXFrameResource->iFence);
    }

    std::variant<MsaaState, Error> DirectXRenderer::getMaxSupportedAntialiasingQuality() const {
        if (pDevice == nullptr) [[unlikely]] {
            return Error("expected device to be valid at this point");
        }

        // Prepare info to check.
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
        msQualityLevels.Format = backBufferFormat;
        msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        msQualityLevels.NumQualityLevels = 0;

        const std::array<MsaaState, 3> vQualityToCheck = {
            MsaaState::VERY_HIGH, MsaaState::HIGH, MsaaState::MEDIUM};

        // Find the maximum supported quality.
        for (const auto& quality : vQualityToCheck) {
            // Check hardware support for this sample count.
            msQualityLevels.SampleCount = static_cast<int>(quality);
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

        return MsaaState::DISABLED;
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
