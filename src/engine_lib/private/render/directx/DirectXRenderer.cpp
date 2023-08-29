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
#include "materials/hlsl/HlslEngineShaders.hpp"
#include "render/general/pipeline/PipelineManager.h"
#include "render/directx/resources/DirectXResource.h"
#include "materials/Material.h"
#include "game/nodes/MeshNode.h"
#include "materials/hlsl/RootSignatureGenerator.h"
#include "render/general/resources/FrameResourcesManager.h"
#include "game/camera/CameraProperties.h"
#include "game/camera/CameraManager.h"
#include "game/nodes/CameraNode.h"
#include "game/camera/TransientCamera.h"
#include "materials/hlsl/HlslShaderResource.h"
#include "render/directx/resources/DirectXFrameResource.h"
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
        ComPtr<ID3D12Debug> pDebugController;
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
        auto result = dynamic_cast<DirectXResourceManager*>(getResourceManager())
                          ->createResource(
                              "renderer depth/stencil buffer",
                              allocationDesc,
                              depthStencilDesc,
                              D3D12_RESOURCE_STATE_DEPTH_WRITE,
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
        std::string sRating = fmt::format("found and rated {} suitable GPU(s):", vGpuScores.size());
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
            sRating += fmt::format("\n{}. ", i + 1);

            // Process status.
            if (bIsBlacklisted) {
                sRating += "[blacklisted] ";
                vGpuScores[i].bIsBlacklisted = true;
            }

            // Append GPU name and score.
            sRating += fmt::format("{}, suitability score: {}", vGpuScores[i].sGpuName, vGpuScores[i].iScore);
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
                Logger::get().info(fmt::format(
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
                Logger::get().info(fmt::format(
                    "ignoring GPU \"{}\" because it's blacklisted (previously the renderer failed to "
                    "initialize using this GPU)",
                    currentGpuInfo.sGpuName));
                continue;
            }

            // Log used GPU.
            if (sGpuNameToUse == currentGpuInfo.sGpuName) {
                Logger::get().info(fmt::format(
                    "using the following GPU: \"{}\" (was specified as preferred in the renderer's "
                    "config file)",
                    currentGpuInfo.sGpuName));
            } else {
                Logger::get().info(fmt::format("using the following GPU: \"{}\"", currentGpuInfo.sGpuName));
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
            return Error(fmt::format(
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

    std::optional<Error> DirectXRenderer::prepareForDrawingNextFrame(CameraProperties* pCameraProperties) {
        PROFILE_FUNC;

        std::scoped_lock frameGuard(*getRenderResourcesMutex());

        // Waits for frame resource to be no longer used by the GPU.
        updateResourcesForNextFrame(scissorRect.right, scissorRect.bottom, pCameraProperties);

        // Get command allocator to open command list.
        auto pMtxCurrentFrameResource = getFrameResourcesManager()->getCurrentFrameResource();
        std::scoped_lock frameResourceGuard(pMtxCurrentFrameResource->first);

        // Convert frame resource.
        const auto pDirectXFrameResource =
            reinterpret_cast<DirectXFrameResource*>(pMtxCurrentFrameResource->second.pResource);

        const auto pCommandAllocator = pDirectXFrameResource->pCommandAllocator.Get();

        PROFILE_SCOPE_START(ResetCommandAllocator);

        // Clear command allocator since the GPU is no longer using it.
        auto hResult = pCommandAllocator->Reset();
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        PROFILE_SCOPE_END;

        PROFILE_SCOPE_START(ResetCommandList);

        // Open command list to record new commands.
        hResult = pCommandList->Reset(pCommandAllocator, nullptr);
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        PROFILE_SCOPE_END;

        // Set CBV/SRV/UAV descriptor heap.
        const auto pResourceManager = reinterpret_cast<DirectXResourceManager*>(getResourceManager());
        ID3D12DescriptorHeap* pDescriptorHeaps[] = {pResourceManager->getCbvSrvUavHeap()->getInternalHeap()};
        pCommandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

        // Set viewport size and scissor rectangles.
        pCommandList->RSSetViewports(1, &screenViewport);
        pCommandList->RSSetScissorRects(1, &scissorRect);

        // Change render target buffer's state from "Present" to "render target.
        const auto pCurrentBackBufferResource = getCurrentBackBufferResource();
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
            pCurrentBackBufferResource->getInternalResource(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        pCommandList->ResourceBarrier(1, &transition);

        // Get render target resource descriptor handle.
        auto optionalRenderTargetDescritorHandle =
            pCurrentBackBufferResource->getBindedDescriptorHandle(DirectXDescriptorType::RTV);
        if (!optionalRenderTargetDescritorHandle.has_value()) [[unlikely]] {
            return Error(fmt::format(
                "render target resource \"{}\" has no RTV binded to it",
                pCurrentBackBufferResource->getResourceName()));
        }
        const auto renderTargetDescriptorHandle = optionalRenderTargetDescritorHandle.value();

        // Get depth stencil resource descriptor handle.
        auto optionalDepthStencilDescritorHandle =
            pDepthStencilBuffer->getBindedDescriptorHandle(DirectXDescriptorType::DSV);
        if (!optionalDepthStencilDescritorHandle.has_value()) [[unlikely]] {
            return Error(fmt::format(
                "depth stencil resource \"{}\" has no DSV binded to it",
                pDepthStencilBuffer->getResourceName()));
        }
        const auto depthStencilDescriptorHandle = optionalDepthStencilDescritorHandle.value();

        // Clear render target and depth stencil buffers using descriptor handles.
        pCommandList->ClearRenderTargetView(renderTargetDescriptorHandle, backBufferFillColor, 0, nullptr);
        pCommandList->ClearDepthStencilView(
            depthStencilDescriptorHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            getMaxDepth(),
            0,
            0,
            nullptr);

        // Bind RTV and DSV to use to pipeline.
        pCommandList->OMSetRenderTargets(
            1, &renderTargetDescriptorHandle, TRUE, &depthStencilDescriptorHandle);

        return {};
    }

    void DirectXRenderer::drawNextFrame() {
        PROFILE_FUNC;

        // Get active camera.
        const auto pMtxActiveCamera = getGameManager()->getCameraManager()->getActiveCamera();

        // Lock both camera and draw mutex.
        std::scoped_lock renderGuard(pMtxActiveCamera->first, *getRenderResourcesMutex());

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
        auto optionalError = prepareForDrawingNextFrame(pActiveCameraProperties);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Lock frame resources to use them (see below).
        auto pMtxCurrentFrameResource = getFrameResourcesManager()->getCurrentFrameResource();
        std::scoped_lock frameResourceGuard(pMtxCurrentFrameResource->first);

        // Set topology type.
        // TODO: this will be moved in some other place later
        pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Iterate over all PSOs.
        const auto pCreatedGraphicsPsos = getPipelineManager()->getGraphicsPipelines();
        for (auto& [mtx, map] : *pCreatedGraphicsPsos) {
            std::scoped_lock psoGuard(mtx);

            for (const auto& [sPsoId, pPso] : map) {
                auto pMtxPsoResources = reinterpret_cast<DirectXPso*>(pPso.get())->getInternalResources();

                std::scoped_lock guardPsoResources(pMtxPsoResources->first);

                // Set PSO and root signature.
                pCommandList->SetPipelineState(pMtxPsoResources->second.pGraphicsPso.Get());
                pCommandList->SetGraphicsRootSignature(pMtxPsoResources->second.pRootSignature.Get());

                // After setting root signature we can set root parameters.

                // Set CBV to frame constant buffer.
                pCommandList->SetGraphicsRootConstantBufferView(
                    RootSignatureGenerator::getFrameConstantBufferRootParameterIndex(),
                    reinterpret_cast<DirectXResource*>(pMtxCurrentFrameResource->second.pResource
                                                           ->pFrameConstantBuffer->getInternalResource())
                        ->getInternalResource()
                        ->GetGPUVirtualAddress());

                // Iterate over all materials that use this PSO.
                const auto pMtxMaterials = pPso->getMaterialsThatUseThisPipeline();
                std::scoped_lock materialsGuard(pMtxMaterials->first);

                for (const auto& pMaterial : pMtxMaterials->second) {
                    // Get material's GPU resources.
                    const auto pMtxMaterialGpuResources = pMaterial->getMaterialGpuResources();
                    std::scoped_lock materialGpuResourcesGuard(pMtxMaterialGpuResources->first);

                    // Set material's CPU write shader resources (`cbuffer`s for example).
                    for (const auto& [sResourceName, pShaderCpuWriteResource] :
                         pMtxMaterialGpuResources->second.shaderResources.shaderCpuWriteResources) {
                        reinterpret_cast<HlslShaderCpuWriteResource*>(pShaderCpuWriteResource.getResource())
                            ->setConstantBufferView(
                                pCommandList, pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex);
                    }

                    // Draw mesh nodes.
                    drawMeshNodes(pMaterial, pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex);
                }
            }
        }

        // Do finish logic.
        optionalError = finishDrawingNextFrame();
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Switch to the next frame resource.
        getFrameResourcesManager()->switchToNextFrameResource();
    }

    std::optional<Error> DirectXRenderer::finishDrawingNextFrame() {
        PROFILE_FUNC;

        std::scoped_lock guardFrame(*getRenderResourcesMutex());

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

        // Close command list.
        auto hResult = pCommandList->Close();
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        PROFILE_SCOPE_START(ExecuteCommandLists);

        // Add the command list to the command queue for execution.
        ID3D12CommandList* commandLists[] = {pCommandList.Get()};
        pCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

        PROFILE_SCOPE_END;

        PROFILE_SCOPE_START(Present);

        // Flip swap chain buffers.
        hResult = pSwapChain->Present(iPresentSyncInterval, iPresentFlags);
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

        // Get current frame resource.
        auto pMtxCurrentFrameResource = getFrameResourcesManager()->getCurrentFrameResource();
        std::scoped_lock frameResourceGuard(pMtxCurrentFrameResource->first);

        // Convert frame resource.
        const auto pDirectXFrameResource =
            reinterpret_cast<DirectXFrameResource*>(pMtxCurrentFrameResource->second.pResource);

        // Save new fence value in the current frame resource.
        pDirectXFrameResource->iFence = iNewFenceValue;

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
            return Error(fmt::format("expected the current AA quality {} to be supported", iSampleCount));
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

    void DirectXRenderer::drawMeshNodes(Material* pMaterial, size_t iCurrentFrameResourceIndex) {
        PROFILE_FUNC;

        const auto pMtxMeshNodes = pMaterial->getSpawnedMeshNodesThatUseThisMaterial();

        // Iterate over all visible mesh nodes that use this material.
        std::scoped_lock meshNodesGuard(pMtxMeshNodes->first);
        for (const auto& pMeshNode : pMtxMeshNodes->second.visibleMeshNodes) {
            // Get mesh data.
            auto pMtxMeshGpuResources = pMeshNode->getMeshGpuResources();
            auto mtxMeshData = pMeshNode->getMeshData();

            std::scoped_lock geometryGuard(pMtxMeshGpuResources->first, *mtxMeshData.first);

            // Create vertex buffer view.
            D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
            vertexBufferView.BufferLocation =
                reinterpret_cast<DirectXResource*>(pMtxMeshGpuResources->second.mesh.pVertexBuffer.get())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress();
            vertexBufferView.StrideInBytes = sizeof(MeshVertex);
            vertexBufferView.SizeInBytes =
                static_cast<UINT>(mtxMeshData.second->getVertices()->size() * vertexBufferView.StrideInBytes);

            // Create index buffer view.
            static_assert(sizeof(MeshData::meshindex_t) == sizeof(unsigned int), "change `Format`");
            D3D12_INDEX_BUFFER_VIEW indexBufferView;
            indexBufferView.BufferLocation =
                reinterpret_cast<DirectXResource*>(pMtxMeshGpuResources->second.mesh.pIndexBuffer.get())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress();
            indexBufferView.Format = DXGI_FORMAT_R32_UINT;
            indexBufferView.SizeInBytes =
                static_cast<UINT>(mtxMeshData.second->getIndices()->size() * sizeof(MeshData::meshindex_t));

            // Set vertex/index buffer.
            pCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
            pCommandList->IASetIndexBuffer(&indexBufferView);

            // Set CPU write shader resources (`cbuffer`s for example).
            for (const auto& [sResourceName, pShaderCpuWriteResource] :
                 pMtxMeshGpuResources->second.shaderResources.shaderCpuWriteResources) {
                reinterpret_cast<HlslShaderCpuWriteResource*>(pShaderCpuWriteResource.getResource())
                    ->setConstantBufferView(pCommandList, iCurrentFrameResourceIndex);
            }

            // Add a draw command.
            pCommandList->DrawIndexedInstanced(
                static_cast<UINT>(mtxMeshData.second->getIndices()->size()), 1, 0, 0, 0);
        }
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
        desc.BufferCount = getSwapChainBufferCount();

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
                Logger::get().info(fmt::format(
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
                std::string sErrorMessage = fmt::format(
                    "video mode with resolution {}x{} and refresh rate "
                    "{}/{} matched multiple supported modes:\n",
                    preferredRenderResolution.first,
                    preferredRenderResolution.second,
                    preferredRefreshRate.first,
                    preferredRefreshRate.second);
                for (const auto& mode : vFilteredModes) {
                    sErrorMessage += fmt::format(
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
                Logger::get().info(fmt::format("{}\nusing default video mode", sErrorMessage));
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
        Logger::get().info(fmt::format("using the following GPU: \"{}\"", getCurrentlyUsedGpuName()));

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

        // Get render settings.
        const auto pMtxRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(pMtxRenderSettings->first);
        const auto renderResolution = pMtxRenderSettings->second->getRenderResolution();
        const auto bIsVSyncEnabled = pMtxRenderSettings->second->isVsyncEnabled();
        const auto iMsaaSampleCount = static_cast<int>(pMtxRenderSettings->second->getAntialiasingState());

        // Update supported AA quality level count.
        auto optionalError = updateMsaaQualityLevelCount();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Swapchain cannot be resized unless all outstanding buffer references have been released.
        vSwapChainBuffers.clear();
        vSwapChainBuffers.resize(getSwapChainBufferCount());

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
            getSwapChainBufferCount(),
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
                ->makeRtvResourcesFromSwapChainBuffer(pSwapChain.Get(), getSwapChainBufferCount());
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
            // Update fence.
            std::scoped_lock fenceGuard(mtxCurrentFenceValue.first);
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
