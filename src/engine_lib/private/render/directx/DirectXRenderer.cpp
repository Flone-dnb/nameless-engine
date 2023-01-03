﻿#include "DirectXRenderer.h"

// Standard.
#include <format>
#include <filesystem>
#include <array>
#include <future>

// Custom.
#include "game/Window.h"
#include "io/Logger.h"
#include "misc/Globals.h"
#include "misc/MessageBox.h"
#include "render/directx/descriptors/DirectXDescriptorHeap.h"
#include "render/directx/resources/DirectXResource.h"
#include "render/directx/resources/DirectXResourceManager.h"
#include "materials/hlsl/HlslEngineShaders.hpp"
#include "render/pso/PsoManager.h"
#include "materials/ShaderFilesystemPaths.hpp"
#include "materials/Material.h"

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
    DirectXRenderer::DirectXRenderer(Game* pGame) : Renderer(pGame) {
        std::scoped_lock frameGuard(*getRenderResourcesMutex());

        initializeRenderSettings();

        // Initialize DirectX.
        std::optional<Error> error = initializeDirectX();
        if (error.has_value()) {
            error->addEntry();
            error->showError();
            throw std::runtime_error(error->getError());
        }

        // Disable Alt + Enter.
        const HRESULT hResult =
            pFactory->MakeWindowAssociation(getWindow()->getWindowHandle(), DXGI_MWA_NO_ALT_ENTER);
        if (FAILED(hResult)) {
            const Error error1(hResult);
            error1.showError();
            throw std::runtime_error(error1.getError());
        }

        // Set initial size for buffers.
        error = updateRenderBuffers();
        if (error.has_value()) {
            error->addEntry();
            error->showError();
            throw std::runtime_error(error->getError());
        }

        // Compile engine shaders.
        error = compileEngineShaders();
        if (error.has_value()) {
            error->addEntry();
            error->showError();
            throw std::runtime_error(error->getError());
        }

        // Create pipeline state objects.
        error = createPipelineStateObjects();
        if (error.has_value()) {
            error->addEntry();
            error->showError();
            throw std::runtime_error(error->getError());
        }

        // TODO: create frame resources.
    }

    std::optional<Error> DirectXRenderer::enableDebugLayer() const {
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

        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true);
#endif
        return {};
    }

    std::optional<Error> DirectXRenderer::createDepthStencilBuffer() {
        // Get render settings.
        const auto pRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(pRenderSettings->first);
        const auto renderResolution = pRenderSettings->second->getScreenSettings()->getRenderResolution();
        const auto bIsMsaaEnabled = pRenderSettings->second->getAntialiasingSettings()->isEnabled();
        const auto iMsaaSampleCount =
            static_cast<int>(pRenderSettings->second->getAntialiasingSettings()->getQuality());

        const D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC(
            D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            0,
            renderResolution.first,
            renderResolution.second,
            1,
            1,
            depthStencilBufferFormat,
            bIsMsaaEnabled ? iMsaaSampleCount : 1,
            bIsMsaaEnabled ? (iMsaaQuality - 1) : 0,
            D3D12_TEXTURE_LAYOUT_UNKNOWN,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        D3D12_CLEAR_VALUE depthClear;
        depthClear.Format = depthStencilBufferFormat;
        depthClear.DepthStencil.Depth = 1.0f;
        depthClear.DepthStencil.Stencil = 0;

        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        auto result = pResourceManager->createDsvResource(
            "renderer depth/stencil buffer",
            allocationDesc,
            depthStencilDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            depthClear);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }
        pDepthStencilBuffer = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        return {};
    }

    std::variant<std::vector<std::string>, Error> DirectXRenderer::getSupportedGpuNames() const {
        std::vector<std::string> vAddedVideoAdapters;

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
                DXGI_ADAPTER_DESC desc;
                pTestAdapter->GetDesc(&desc);
                vAddedVideoAdapters.push_back(wstringToString(desc.Description));
            }
        }

        if (vAddedVideoAdapters.empty()) {
            return Error("could not find a GPU that supports used DirectX version and "
                         "feature level");
        }

        return vAddedVideoAdapters;
    }

    std::variant<std::vector<std::pair<unsigned int, unsigned int>>, Error>
    DirectXRenderer::getSupportedRenderResolutions() const {
        auto result = getSupportedDisplayModes();
        if (std::holds_alternative<Error>(result)) {
            Error error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }
        const std::vector<DXGI_MODE_DESC> vRenderModes =
            std::get<std::vector<DXGI_MODE_DESC>>(std::move(result));

        std::vector<std::pair<unsigned int, unsigned int>> vFoundResolutions;
        for (const auto& mode : vRenderModes) {
            vFoundResolutions.push_back(std::make_pair(mode.Width, mode.Height));
        }

        return vFoundResolutions;
    }

    std::string DirectXRenderer::getCurrentlyUsedGpuName() const { return sUsedVideoAdapter; }

    size_t DirectXRenderer::getTotalVideoMemoryInMb() const {
        return pResourceManager->getTotalVideoMemoryInMb();
    }

    size_t DirectXRenderer::getUsedVideoMemoryInMb() const {
        return pResourceManager->getUsedVideoMemoryInMb();
    }

    void DirectXRenderer::drawNextFrame() {
        std::scoped_lock frameGuard(*getRenderResourcesMutex());
        updateResourcesForNextFrame();

        // TODO: draw start logic

        const auto pCreatedGraphicsPsos = getPsoManager()->getGraphicsPsos();
        for (auto& [mtx, map] : *pCreatedGraphicsPsos) {
            std::scoped_lock psoGuard(mtx);

            for (const auto& [sPsoId, pPso] : map) {
                // TODO: set PSO

                const auto pMtxMaterials = pPso->getMaterialsThatUseThisPso();

                std::scoped_lock materialsGuard(pMtxMaterials->first);
                for (const auto& pMaterial : pMtxMaterials->second) {
                    // TODO: set material

                    const auto pMtxMeshNodes = pMaterial->getSpawnedMeshNodesThatUseThisMaterial();

                    std::scoped_lock meshNodesGuard(pMtxMeshNodes->first);
                    for (const auto& pMeshNodes : pMtxMeshNodes->second) {
                        // TODO: draw mesh
                    }
                }
            }
        }

        // TODO: draw end logic
    }

    std::optional<Error> DirectXRenderer::setVideoAdapter(const std::string& sVideoAdapterName) {
        for (UINT iAdapterIndex = 0;; iAdapterIndex++) {
            ComPtr<IDXGIAdapter3> pTestAdapter;
            if (pFactory->EnumAdapters(
                    iAdapterIndex, reinterpret_cast<IDXGIAdapter**>(pTestAdapter.GetAddressOf())) ==
                DXGI_ERROR_NOT_FOUND) {
                // No more adapters to enumerate.
                break;
            }

            DXGI_ADAPTER_DESC desc;
            pTestAdapter->GetDesc(&desc);
            if (wstringToString(desc.Description) == sVideoAdapterName) {
                // Check if the adapter supports used D3D version, but don't create the actual device yet.
                const HRESULT hResult = D3D12CreateDevice(
                    pTestAdapter.Get(), rendererD3dFeatureLevel, _uuidof(ID3D12Device), nullptr);
                if (SUCCEEDED(hResult)) {
                    // Found supported adapter.
                    pVideoAdapter = std::move(pTestAdapter);
                    sUsedVideoAdapter = sVideoAdapterName;

                    return {};
                } else {
                    return Error("the specified video adapter does not support used DirectX "
                                 "version or feature level");
                }
            }
        }

        return Error("the specified video adapter was not found");
    }

    std::optional<Error> DirectXRenderer::setOutputAdapter() {
        if (pOutputAdapter) {
            return Error("output adapter already created");
        }

        ComPtr<IDXGIOutput> pTestOutput;

        const HRESULT hResult = pVideoAdapter->EnumOutputs(0, pTestOutput.GetAddressOf());
        if (hResult == DXGI_ERROR_NOT_FOUND) {
            return Error("no output adapter was found for current video adapter");
        } else if (FAILED(hResult)) {
            return Error(hResult);
        }

        pOutputAdapter = std::move(pTestOutput);
        return {};
    }

    std::optional<Error> DirectXRenderer::createCommandObjects() {
        if (pCommandQueue || pCommandListAllocator || pCommandList) {
            return Error("command objects already created");
        }

        // Create Command Queue.
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

        HRESULT hResult = pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pCommandQueue));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Create Command Allocator.
        hResult = pDevice->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(pCommandListAllocator.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Create Command List.
        hResult = pDevice->CreateCommandList(
            0, // Create list for only one GPU. See pDevice->GetNodeCount()
               // and documentation for more info.
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            pCommandListAllocator.Get(), // Associated command allocator
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

    std::optional<Error> DirectXRenderer::createResourceManager() {
        auto result = DirectXResourceManager::create(this, pDevice.Get(), pVideoAdapter.Get());
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }

        pResourceManager = std::get<std::unique_ptr<DirectXResourceManager>>(std::move(result));

        return {};
    }

    std::optional<Error> DirectXRenderer::createSwapChain() {
        // Get render settings.
        const auto pRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(pRenderSettings->first);
        const auto renderResolution = pRenderSettings->second->getScreenSettings()->getRenderResolution();
        const auto refreshRate = pRenderSettings->second->getScreenSettings()->getRefreshRate();
        const auto bIsVSyncEnabled = pRenderSettings->second->getScreenSettings()->isVsyncEnabled();

        DXGI_SWAP_CHAIN_DESC1 desc;
        desc.Width = renderResolution.first;
        desc.Height = renderResolution.second;
        desc.Format = backBufferFormat;
        desc.Stereo = false;

        // Flip model swapchains (DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL and DXGI_SWAP_EFFECT_FLIP_DISCARD)
        // do not support multisampling.
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = getSwapChainBufferCount();

        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
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
        fullscreenDesc.Windowed = true;

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

    std::optional<Error> DirectXRenderer::createPipelineStateObjects() {
        // TODO: setup manager (rename this function)

        return {};
    }

    std::optional<Error> DirectXRenderer::checkMsaaSupport() {
        if (!pDevice) {
            return Error("device is not created");
        }

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
        msQualityLevels.Format = backBufferFormat;
        msQualityLevels.SampleCount = 4; // test max quality
        msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        msQualityLevels.NumQualityLevels = 0;

        const HRESULT hResult = pDevice->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        if (msQualityLevels.NumQualityLevels == 0) {
            return Error("received zero quality levels for MSAA");
        }

        iMsaaQuality = msQualityLevels.NumQualityLevels;

        return {};
    }

    std::optional<Error> DirectXRenderer::initializeDirectX() {
        // Enable debug layer in DEBUG mode.
        DWORD debugFactoryFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
        {
            std::optional<Error> error = enableDebugLayer();
            if (error.has_value()) {
                error->addEntry();
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
        const auto pRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(pRenderSettings->first);

        // Get supported video adapters.
        auto result = DirectXRenderer::getSupportedGpuNames();
        if (std::holds_alternative<Error>(result)) {
            Error error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }
        const auto vSupportedVideoAdapters = std::get<std::vector<std::string>>(std::move(result));

        // Make sure the GPU index is in range of supported GPUs.
        iPickedGpuIndex = pRenderSettings->second->getScreenSettings()->getUsedGpuIndex();
        if (iPickedGpuIndex >= static_cast<unsigned int>(vSupportedVideoAdapters.size())) {
            Logger::get().error(
                std::format(
                    "preferred GPU index {} is out of range, supported GPUs in total: {}, using first found "
                    "GPU",
                    iPickedGpuIndex,
                    vSupportedVideoAdapters.size()),
                sDirectXRendererLogCategory);

            // Just use first found GPU then.
            iPickedGpuIndex = 0;
            pRenderSettings->second->getScreenSettings()->setGpuToUse(iPickedGpuIndex);
        }

        // Use video adapter.
        std::optional<Error> error = setVideoAdapter(vSupportedVideoAdapters[iPickedGpuIndex]);
        if (error.has_value()) {
            error->addEntry();
            return error;
        }

        // Set output adapter (monitor) to use.
        error = setOutputAdapter();
        if (error.has_value()) {
            // This might happen for GPUs with non-zero index, see if we can pick something else.
            if (iPickedGpuIndex == 0) {
                error->addEntry();
                return error;
            }
            Logger::get().error(
                std::format(
                    "failed to set output adapter for the GPU \"{}\" (index {}), using first found GPU, "
                    "error: {}",
                    vSupportedVideoAdapters[iPickedGpuIndex],
                    iPickedGpuIndex,
                    error->getInitialMessage()),
                sDirectXRendererLogCategory);

            // Try first found GPU.
            iPickedGpuIndex = 0;
            pRenderSettings->second->getScreenSettings()->setGpuToUse(iPickedGpuIndex);

            error = setVideoAdapter(vSupportedVideoAdapters[iPickedGpuIndex]);
            if (error.has_value()) {
                error->addEntry();
                return error;
            }

            error = setOutputAdapter();
            if (error.has_value()) {
                error->addEntry();
                return error;
            }
        }

        // Create device.
        hResult = D3D12CreateDevice(pVideoAdapter.Get(), rendererD3dFeatureLevel, IID_PPV_ARGS(&pDevice));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Create fence.
        hResult = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Check MSAA support.
        error = checkMsaaSupport();
        if (error.has_value()) {
            error->addEntry();
            return error;
        }

        // Create command queue and command list.
        error = createCommandObjects();
        if (error.has_value()) {
            error->addEntry();
            return error;
        }

        // Determine display mode.
        auto videoModesResult = getSupportedDisplayModes();
        if (std::holds_alternative<Error>(videoModesResult)) {
            Error err = std::get<Error>(std::move(videoModesResult));
            err.addEntry();
            return err;
        }

        // Get preferred screen settings.
        const auto preferredRenderResolution =
            pRenderSettings->second->getScreenSettings()->getRenderResolution();
        const auto preferredRefreshRate = pRenderSettings->second->getScreenSettings()->getRefreshRate();

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
                Logger::get().error(
                    std::format(
                        "video mode with resolution {}x{} and refresh rate "
                        "{}/{} is not supported, using default video mode",
                        preferredRenderResolution.first,
                        preferredRenderResolution.second,
                        preferredRefreshRate.first,
                        preferredRefreshRate.second),
                    sDirectXRendererLogCategory);
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
                Logger::get().error(
                    std::format("{}\nusing default video mode", sErrorMessage), sDirectXRendererLogCategory);
                // use last display mode
                pickedDisplayMode = vVideoModes.back();
            }
        } else {
            // use last display mode
            pickedDisplayMode = std::get<std::vector<DXGI_MODE_DESC>>(std::move(videoModesResult)).back();
        }

        // Save display settings.
        pRenderSettings->second->getScreenSettings()->setRenderResolution(
            std::make_pair(pickedDisplayMode.Width, pickedDisplayMode.Height));
        pRenderSettings->second->getScreenSettings()->setRefreshRate(std::make_pair(
            pickedDisplayMode.RefreshRate.Numerator, pickedDisplayMode.RefreshRate.Denominator));

        // Create resource manager.
        error = createResourceManager();
        if (error.has_value()) {
            error->addEntry();
            return error;
        }

        // Create swap chain.
        error = createSwapChain();
        if (error.has_value()) {
            error->addEntry();
            return error;
        }

        // Log used GPU name.
        Logger::get().info(
            fmt::format("using the following GPU: \"{}\"", getCurrentlyUsedGpuName()),
            sDirectXRendererLogCategory);

        bIsDirectXInitialized = true;

        return {};
    }

    std::optional<Error> DirectXRenderer::compileEngineShaders() const {
        // Do this synchronously (before user can queue his shaders).
        std::vector vEngineShaders = {HlslEngineShaders::vertexShader, HlslEngineShaders::pixelShader};

        auto pPromiseFinish = std::make_shared<std::promise<bool>>();
        auto future = pPromiseFinish->get_future();

        auto onProgress = [](size_t iCompiledShaderCount, size_t iTotalShadersToCompile) {};
        auto onError = [](ShaderDescription shaderDescription, std::variant<std::string, Error> error) {
            if (std::holds_alternative<std::string>(error)) {
                const auto sErrorMessage = std::format(
                    "failed to compile shader \"{}\" due to compilation error:\n{}",
                    shaderDescription.sShaderName,
                    std::get<std::string>(std::move(error)));
                const Error err(sErrorMessage);
                err.showError();
                throw std::runtime_error(err.getError());
            } else {
                const auto sErrorMessage = std::format(
                    "failed to compile shader \"{}\" due to internal error:\n{}",
                    shaderDescription.sShaderName,
                    std::get<Error>(std::move(error)).getError());
                const Error err(sErrorMessage);
                err.showError();
                MessageBox::info(
                    "Info",
                    fmt::format(
                        "Try restarting the application or deleting the directory \"{}\", if this "
                        "does not help contact the developers.",
                        ShaderFilesystemPaths::getPathToShaderCacheDirectory().string()));
                throw std::runtime_error(err.getError());
            }
        };
        auto onCompleted = [pPromiseFinish]() { pPromiseFinish->set_value(false); };

        auto error = getShaderManager()->compileShaders(vEngineShaders, onProgress, onError, onCompleted);
        if (error.has_value()) {
            error->addEntry();
            error->showError();
            throw std::runtime_error(error->getError());
        }

        try {
            if (future.valid()) {
                future.get();
            }
        } catch (const std::exception& ex) {
            const Error err(ex.what());
            err.showError();
            throw std::runtime_error(err.getInitialMessage());
        }

        return {};
    }

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 4> DirectXRenderer::getStaticTextureSamplers() {
        const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
            0,
            D3D12_FILTER_MIN_MAG_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);

        const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
            0,
            D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);

        const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
            0,
            D3D12_FILTER_ANISOTROPIC,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);

        const CD3DX12_STATIC_SAMPLER_DESC shadow(
            1,
            D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            0.0f,
            16,
            D3D12_COMPARISON_FUNC_LESS_EQUAL,
            D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

        return {pointWrap, linearWrap, anisotropicWrap, shadow};
    }

    DirectXResourceManager* DirectXRenderer::getResourceManager() const { return pResourceManager.get(); }

    ID3D12Device* DirectXRenderer::getDevice() const { return pDevice.Get(); }

    DXGI_FORMAT DirectXRenderer::getBackBufferFormat() const { return backBufferFormat; }

    DXGI_FORMAT DirectXRenderer::getDepthStencilBufferFormat() const { return depthStencilBufferFormat; }

    UINT DirectXRenderer::getMsaaQualityLevel() const { return iMsaaQuality; }

    void DirectXRenderer::updateResourcesForNextFrame() {
        std::scoped_lock frameGuard(*getRenderResourcesMutex());
        // TODO: wait for frame resource to be free

        // TODO: updateMaterialConstantBuffersForNextFrame()
        // TODO: updateMeshConstantBuffersForNextFrame()
        // TODO: updateShadowMapConstantBuffersForNextFrame()
        // TODO: updateMainPassConstantBuffersForNextFrame()
    }

    std::optional<Error> DirectXRenderer::updateRenderBuffers() {
        std::scoped_lock guard(*getRenderResourcesMutex());

        flushCommandQueue();

        // Get render settings.
        const auto pRenderSettings = getRenderSettings();
        std::scoped_lock renderSettingsGuard(pRenderSettings->first);
        const auto renderResolution = pRenderSettings->second->getScreenSettings()->getRenderResolution();
        const auto bIsVSyncEnabled = pRenderSettings->second->getScreenSettings()->isVsyncEnabled();
        const auto bIsMsaaEnabled = pRenderSettings->second->getAntialiasingSettings()->isEnabled();
        const auto iMsaaSampleCount =
            static_cast<int>(pRenderSettings->second->getAntialiasingSettings()->getQuality());

        // Swapchain cannot be resized unless all outstanding buffer references have been released.
        vSwapChainBuffers.clear();
        vSwapChainBuffers.resize(getSwapChainBufferCount());

        // Resize the swap chain.
        HRESULT hResult = pSwapChain->ResizeBuffers(
            getSwapChainBufferCount(),
            renderResolution.first,
            renderResolution.second,
            backBufferFormat,
            bIsVSyncEnabled ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
                            : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Create RTV to swap chain buffers.
        auto swapChainResult = pResourceManager->makeRtvResourcesFromSwapChainBuffer(
            pSwapChain.Get(), getSwapChainBufferCount());
        if (std::holds_alternative<Error>(swapChainResult)) {
            auto err = std::get<Error>(std::move(swapChainResult));
            err.addEntry();
            return err;
        }
        vSwapChainBuffers =
            std::get<std::vector<std::unique_ptr<DirectXResource>>>(std::move(swapChainResult));

        // Create MSAA render target.
        const auto msaaRenderTargetDesc = CD3DX12_RESOURCE_DESC(
            D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            0,
            renderResolution.first,
            renderResolution.second,
            1,
            1,
            backBufferFormat,
            bIsMsaaEnabled ? iMsaaSampleCount : 1,
            bIsMsaaEnabled ? (iMsaaQuality - 1) : 0,
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

        auto result = pResourceManager->createRtvResource(
            "renderer render target buffer",
            allocationDesc,
            msaaRenderTargetDesc,
            D3D12_RESOURCE_STATE_COMMON,
            msaaClear);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }
        pMsaaRenderBuffer = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

        // Create depth/stencil buffer.
        auto optionalError = createDepthStencilBuffer();
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        // Update the viewport transform to cover the new display size.
        screenViewport.TopLeftX = 0;
        screenViewport.TopLeftY = 0;
        screenViewport.Width = static_cast<float>(renderResolution.first);
        screenViewport.Height = static_cast<float>(renderResolution.second);
        screenViewport.MinDepth = fMinDepth;
        screenViewport.MaxDepth = fMaxDepth;

        scissorRect = {
            0, 0, static_cast<LONG>(renderResolution.first), static_cast<LONG>(renderResolution.second)};

        return {};
    }

    bool DirectXRenderer::isInitialized() const { return bIsDirectXInitialized; }

    void DirectXRenderer::flushCommandQueue() {
        std::scoped_lock guardFrame(*getRenderResourcesMutex());

        const auto iFenceValue = iCurrentFence.fetch_add(1);
        HRESULT hResult = pCommandQueue->Signal(pFence.Get(), iFenceValue);

        if (FAILED(hResult)) {
            auto error = Error(hResult);
            error.showError();
            throw std::runtime_error(error.getError());
        }

        // Wait until the GPU has completed commands up to this fence point.
        if (pFence->GetCompletedValue() < iFenceValue) {
            const HANDLE hEvent = CreateEventEx(nullptr, nullptr, FALSE, EVENT_ALL_ACCESS);
            if (hEvent == nullptr) {
                auto error = Error(GetLastError());
                error.showError();
                throw std::runtime_error(error.getError());
            }

            // Fire event when the GPU hits current fence.
            hResult = pFence->SetEventOnCompletion(iFenceValue, hEvent);
            if (FAILED(hResult)) {
                auto error = Error(hResult);
                error.showError();
                throw std::runtime_error(error.getError());
            }

            WaitForSingleObject(hEvent, INFINITE);
            CloseHandle(hEvent);
        }
    }

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
} // namespace ne
