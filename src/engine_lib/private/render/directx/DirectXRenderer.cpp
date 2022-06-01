#include "DirectXRenderer.h"

// STL.
#include <format>
#include <filesystem>
#include <array>

// Custom.
#include "game/Window.h"
#include "io/ConfigManager.h"
#include "io/Logger.h"
#include "misc/Globals.h"

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
    DirectXRenderer::DirectXRenderer(Game* pGame) : IRenderer(pGame) {
        // Read configuration from config file (if exists).
        if (isConfigurationFileExists()) {
            DirectXRenderer::readConfigurationFromConfigFile();
            bStartedWithConfigurationFromDisk = true;
        }

        std::optional<Error> error = initializeDirectX();
        if (error.has_value()) {
            error->addEntry();
            error->showError();
            throw std::runtime_error(error->getError());
        }

        DirectXRenderer::writeConfigurationToConfigFile();

        // Disable Alt + Enter.
        const HRESULT hResult =
            pFactory->MakeWindowAssociation(getWindow()->getWindowHandle(), DXGI_MWA_NO_ALT_ENTER);
        if (FAILED(hResult)) {
            const Error error1(hResult);
            error1.showError();
            throw std::runtime_error(error1.getError());
        }

        // Set initial size for buffers.
        error = resizeRenderBuffers();
        if (error.has_value()) {
            error->addEntry();
            error->showError();
            throw std::runtime_error(error->getError());
        }

        // Create initial root signature.
        error = createRootSignature();
        if (error.has_value()) {
            error->addEntry();
            error->showError();
            throw std::runtime_error(error->getError());
        }

        // TODO
    }

    void DirectXRenderer::update() {}

    void DirectXRenderer::drawFrame() {}

    std::optional<Error> DirectXRenderer::enableDebugLayer() const {
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

        return {};
    }

    std::variant<std::vector<std::string>, Error> DirectXRenderer::getSupportedGpus() const {
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

    std::pair<int, int> DirectXRenderer::getRenderResolution() const {
        return std::pair(currentDisplayMode.Width, currentDisplayMode.Height);
    }

    std::variant<std::vector<RenderMode>, Error> DirectXRenderer::getSupportedRenderResolutions() const {
        auto result = getSupportedDisplayModes();
        if (std::holds_alternative<Error>(result)) {
            Error error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }

        const std::vector<DXGI_MODE_DESC> vRenderModes =
            std::get<std::vector<DXGI_MODE_DESC>>(std::move(result));

        std::vector<RenderMode> vOutRenderModes;
        for (const auto& mode : vRenderModes) {
            vOutRenderModes.push_back(RenderMode{
                static_cast<int>(mode.Width),
                static_cast<int>(mode.Height),
                static_cast<int>(mode.RefreshRate.Numerator),
                static_cast<int>(mode.RefreshRate.Denominator)});
        }

        return vOutRenderModes;
    }

    std::string DirectXRenderer::getCurrentlyUsedGpuName() const { return sUsedVideoAdapter; }

    Antialiasing DirectXRenderer::getCurrentAntialiasing() const {
        return Antialiasing{bIsMsaaEnabled, static_cast<int>(iMsaaSampleCount)};
    }

    size_t DirectXRenderer::getTotalVideoMemoryInMb() const {
        D3D12MA::Budget localBudget{};
        pMemoryAllocator->GetBudget(&localBudget, nullptr);

        return localBudget.BudgetBytes / 1024 / 1024;
    }

    size_t DirectXRenderer::getUsedVideoMemoryInMb() const {
        D3D12MA::Budget localBudget{};
        pMemoryAllocator->GetBudget(&localBudget, nullptr);

        return localBudget.UsageBytes / 1024 / 1024;
    }

    std::optional<Error> DirectXRenderer::setBackbufferFillColor(float fillColor[4]) {
        backBufferFillColor[0] = fillColor[0];
        backBufferFillColor[1] = fillColor[1];
        backBufferFillColor[2] = fillColor[2];
        backBufferFillColor[3] = fillColor[3];

        auto error = resizeRenderBuffers();
        if (error.has_value()) {
            error->addEntry();
            return error;
        }

        return {};
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

    std::optional<Error> DirectXRenderer::createMemoryAllocator() {
        D3D12MA::ALLOCATOR_DESC desc = {};
        desc.Flags = D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED;
        desc.pDevice = pDevice.Get();
        desc.pAdapter = pVideoAdapter.Get();

        const HRESULT hResult = D3D12MA::CreateAllocator(&desc, pMemoryAllocator.ReleaseAndGetAddressOf());
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        return {};
    }

    std::optional<Error> DirectXRenderer::createSwapChain() {
        DXGI_SWAP_CHAIN_DESC1 desc;
        desc.Width = currentDisplayMode.Width;
        desc.Height = currentDisplayMode.Height;
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
        if (!bIsVSyncEnabled)
            desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc;
        fullscreenDesc.RefreshRate = currentDisplayMode.RefreshRate;
        fullscreenDesc.Scaling = currentDisplayMode.Scaling;
        fullscreenDesc.ScanlineOrdering = currentDisplayMode.ScanlineOrdering;
        fullscreenDesc.Windowed = true;

        const HRESULT hResult = pFactory->CreateSwapChainForHwnd(
            pCommandQueue.Get(),
            getWindow()->getWindowHandle(),
            &desc,
            &fullscreenDesc,
            pOutputAdapter.Get(),
            pSwapChain.ReleaseAndGetAddressOf());
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        return {};
    }

    std::optional<Error> DirectXRenderer::createRtvAndDsvDescriptorHeaps() {
        // Create RTV heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
        rtvHeapDesc.NumDescriptors = getSwapChainBufferCount() + 1; // '+1' for MSAA render target.
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtvHeapDesc.NodeMask = 0;

        HRESULT hResult =
            pDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(pRtvHeap.ReleaseAndGetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Create DSV heap.
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dsvHeapDesc.NodeMask = 0;

        hResult =
            pDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(pDsvHeap.ReleaseAndGetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        return {};
    }

    std::optional<Error> DirectXRenderer::checkMsaaSupport() {
        if (pDevice == nullptr) {
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

        // Get supported video adapters.
        auto result = DirectXRenderer::getSupportedGpus();
        if (std::holds_alternative<Error>(result)) {
            Error error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }
        const auto vSupportedVideoAdapters = std::get<std::vector<std::string>>(std::move(result));
        if (iPreferredGpuIndex >= static_cast<long>(vSupportedVideoAdapters.size())) {
            Logger::get().error(
                std::format(
                    "preferred GPU index {} is out of range, supported GPUs in total: {}, using first found "
                    "GPU",
                    iPreferredGpuIndex,
                    vSupportedVideoAdapters.size()),
                sRendererLogCategory);
            iPreferredGpuIndex = 0; // use first found GPU
        }

        // Use video adapter.
        std::optional<Error> error = setVideoAdapter(vSupportedVideoAdapters[iPreferredGpuIndex]);
        if (error.has_value()) {
            error->addEntry();
            return error;
        }

        // Set output adapter (monitor) to use.
        error = setOutputAdapter();
        if (error.has_value()) {
            if (iPreferredGpuIndex == 0) {
                error->addEntry();
                return error;
            }
            Logger::get().error(
                std::format("{} ({}), using first found GPU", error->getInitialMessage(), iPreferredGpuIndex),
                sRendererLogCategory);

            // Try first found GPU.
            iPreferredGpuIndex = 0;
            error = setVideoAdapter(vSupportedVideoAdapters[iPreferredGpuIndex]);
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

        // Create memory allocation (external class).
        error = createMemoryAllocator();
        if (error.has_value()) {
            error->addEntry();
            return error;
        }

        // Create fence.
        hResult = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Get descriptor sizes.
        iRtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        iDsvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        iCbvSrvUavDescriptorSize =
            pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // Check MSAA support.
        std::optional<Error> error1 = checkMsaaSupport();
        if (error1.has_value()) {
            error1->addEntry();
            return error1;
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

        if (bStartedWithConfigurationFromDisk) {
            // Find the specified video mode.
            const auto vVideoModes = std::get<std::vector<DXGI_MODE_DESC>>(std::move(videoModesResult));

            std::vector<DXGI_MODE_DESC> vFilteredModes;

            for (const auto& mode : vVideoModes) {
                if (mode.Width == currentDisplayMode.Width && mode.Height == currentDisplayMode.Height &&
                    mode.RefreshRate.Numerator == currentDisplayMode.RefreshRate.Numerator &&
                    mode.RefreshRate.Denominator == currentDisplayMode.RefreshRate.Denominator) {
                    vFilteredModes.push_back(mode);
                }
            }

            if (vFilteredModes.empty()) {
                Logger::get().error(
                    std::format(
                        "video mode with resolution ({}x{}) and refresh rate "
                        "({}/{}) is not supported, using default video mode",
                        currentDisplayMode.Width,
                        currentDisplayMode.Height,
                        currentDisplayMode.RefreshRate.Numerator,
                        currentDisplayMode.RefreshRate.Denominator),
                    sRendererLogCategory);
                // use last display mode
                currentDisplayMode = vVideoModes.back();
            } else if (vFilteredModes.size() == 1) {
                // found specified display mode
                currentDisplayMode = vFilteredModes[0];
            } else {
                std::string sErrorMessage = std::format(
                    "video mode with resolution ({}x{}) and refresh rate "
                    "({}/{}) matched multiple supported modes:\n",
                    currentDisplayMode.Width,
                    currentDisplayMode.Height,
                    currentDisplayMode.RefreshRate.Numerator,
                    currentDisplayMode.RefreshRate.Denominator);
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
                    std::format("{}\nusing default video mode", sErrorMessage), sRendererLogCategory);
                // use last display mode
                currentDisplayMode = vVideoModes.back();
            }
        } else {
            // use last display mode
            currentDisplayMode = std::get<std::vector<DXGI_MODE_DESC>>(std::move(videoModesResult)).back();
        }

        // Create swap chain.
        error = createSwapChain();
        if (error.has_value()) {
            error->addEntry();
            return error;
        }

        // Create RTV and DSV heaps.
        error = createRtvAndDsvDescriptorHeaps();
        if (error.has_value()) {
            error->addEntry();
            return error;
        }

        return {};
    }

    std::optional<Error> DirectXRenderer::createRootSignature() {
        // Root parameter can be a table, root descriptor or root constants.
        std::array<CD3DX12_ROOT_PARAMETER, 2> vRootParameters;

        // Performance TIP: Order from most frequent to least frequent.
        vRootParameters[0].InitAsConstantBufferView(0); // frame data
        vRootParameters[1].InitAsConstantBufferView(1); // object data

        // new stuff goes here

        const auto vStaticSamplers = getStaticTextureSamplers();

        // Create root signature description.
        // A root signature is an array of root parameters.
        const CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(
            static_cast<UINT>(vRootParameters.size()),
            vRootParameters.data(),
            static_cast<UINT>(vStaticSamplers.size()),
            vStaticSamplers.data(),
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        // Serialize root signature in order to create it.
        ComPtr<ID3DBlob> pSerializedRootSignature = nullptr;
        ComPtr<ID3DBlob> pSerializerErrorMessage = nullptr;

        HRESULT hResult = D3D12SerializeRootSignature(
            &rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            pSerializedRootSignature.GetAddressOf(),
            pSerializerErrorMessage.GetAddressOf());
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        if (pSerializerErrorMessage) {
            return Error(std::string(
                static_cast<char*>(pSerializerErrorMessage->GetBufferPointer()),
                pSerializerErrorMessage->GetBufferSize()));
        }

        // Create root signature.
        hResult = pDevice->CreateRootSignature(
            0,
            pSerializedRootSignature->GetBufferPointer(),
            pSerializedRootSignature->GetBufferSize(),
            IID_PPV_ARGS(&pRootSignature));
        if (FAILED(hResult)) {
            return Error(hResult);
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
            1,
            D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);

        const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
            2,
            D3D12_FILTER_ANISOTROPIC,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);

        const CD3DX12_STATIC_SAMPLER_DESC shadow(
            3,
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

    std::optional<Error> DirectXRenderer::resizeRenderBuffers() {
        std::scoped_lock<std::recursive_mutex> guard(mtxRwRenderResources);

        std::optional<Error> error = flushCommandQueue();
        if (error.has_value()) {
            error->addEntry();
            return error;
        }

        // Release the previous resources because we will be recreating them.
        for (unsigned int i = 0; i < getSwapChainBufferCount(); i++) {
            pSwapChainBuffer[i].Reset();
        }
        pMsaaRenderTarget.Reset();
        pDepthStencilBuffer.Reset();

        // Resize the swap chain.
        HRESULT hResult = S_OK;
        if (bIsVSyncEnabled) {
            hResult = pSwapChain->ResizeBuffers(
                getSwapChainBufferCount(),
                currentDisplayMode.Width,
                currentDisplayMode.Height,
                backBufferFormat,
                0);
        } else {
            hResult = pSwapChain->ResizeBuffers(
                getSwapChainBufferCount(),
                currentDisplayMode.Width,
                currentDisplayMode.Height,
                backBufferFormat,
                DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
        }
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        iCurrentBackBufferIndex = 0;

        // Create RTV to swap chain buffers.
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(pRtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (unsigned int i = 0; i < getSwapChainBufferCount(); i++) {
            hResult = pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pSwapChainBuffer[i]));
            if (FAILED(hResult)) {
                return Error(hResult);
            }

            pDevice->CreateRenderTargetView(pSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
            rtvHeapHandle.Offset(1, iRtvDescriptorSize);
        }

        // Create MSAA render target.
        D3D12_RESOURCE_DESC msaaRenderTargetDesc;
        msaaRenderTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        msaaRenderTargetDesc.Alignment = 0;
        msaaRenderTargetDesc.Width = currentDisplayMode.Width;
        msaaRenderTargetDesc.Height = currentDisplayMode.Height;
        msaaRenderTargetDesc.DepthOrArraySize = 1;
        msaaRenderTargetDesc.MipLevels = 1;
        msaaRenderTargetDesc.Format = backBufferFormat;
        msaaRenderTargetDesc.SampleDesc.Count = bIsMsaaEnabled ? iMsaaSampleCount : 1;
        msaaRenderTargetDesc.SampleDesc.Quality = bIsMsaaEnabled ? (iMsaaQuality - 1) : 0;
        msaaRenderTargetDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        msaaRenderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE msaaClear;
        msaaClear.Format = backBufferFormat;
        msaaClear.Color[0] = backBufferFillColor[0];
        msaaClear.Color[1] = backBufferFillColor[1];
        msaaClear.Color[2] = backBufferFillColor[2];
        msaaClear.Color[3] = backBufferFillColor[3];

        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        hResult = pMemoryAllocator->CreateResource(
            &allocationDesc,
            &msaaRenderTargetDesc,
            D3D12_RESOURCE_STATE_COMMON,
            &msaaClear,
            pMsaaRenderTarget.ReleaseAndGetAddressOf(),
            IID_NULL,
            nullptr);
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        pDevice->CreateRenderTargetView(pMsaaRenderTarget->GetResource(), nullptr, rtvHeapHandle);

        // Create depth/stencil buffer.
        D3D12_RESOURCE_DESC depthStencilDesc;
        depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthStencilDesc.Alignment = 0;
        depthStencilDesc.Width = currentDisplayMode.Width;
        depthStencilDesc.Height = currentDisplayMode.Height;
        depthStencilDesc.DepthOrArraySize = 1;
        depthStencilDesc.MipLevels = 1;
        depthStencilDesc.Format = depthStencilFormat;
        depthStencilDesc.SampleDesc.Count = bIsMsaaEnabled ? iMsaaSampleCount : 1;
        depthStencilDesc.SampleDesc.Quality = bIsMsaaEnabled ? (iMsaaQuality - 1) : 0;
        depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE depthClear;
        depthClear.Format = depthStencilFormat;
        depthClear.DepthStencil.Depth = 1.0f;
        depthClear.DepthStencil.Stencil = 0;

        allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
        hResult = pMemoryAllocator->CreateResource(
            &allocationDesc,
            &depthStencilDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthClear,
            pDepthStencilBuffer.ReleaseAndGetAddressOf(),
            IID_NULL,
            nullptr);
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Create DSV.
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.ViewDimension =
            bIsMsaaEnabled ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Format = depthStencilFormat;
        dsvDesc.Texture2D.MipSlice = 0;

        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHeapHandle(pDsvHeap->GetCPUDescriptorHandleForHeapStart());
        pDevice->CreateDepthStencilView(pDepthStencilBuffer->GetResource(), &dsvDesc, dsvHeapHandle);

        // Update the viewport transform to cover the new display size.
        screenViewport.TopLeftX = 0;
        screenViewport.TopLeftY = 0;
        screenViewport.Width = static_cast<float>(currentDisplayMode.Width);
        screenViewport.Height = static_cast<float>(currentDisplayMode.Height);
        screenViewport.MinDepth = fMinDepth;
        screenViewport.MaxDepth = fMaxDepth;

        scissorRect = {
            0, 0, static_cast<LONG>(currentDisplayMode.Width), static_cast<LONG>(currentDisplayMode.Height)};

        return {};
    }

    std::optional<Error> DirectXRenderer::flushCommandQueue() {
        std::scoped_lock<std::recursive_mutex> guardFrame(mtxRwRenderResources);

        HRESULT hResult = S_OK;
        {
            std::scoped_lock<std::mutex> guardFence(mtxRwFence);
            iCurrentFence += 1;
            hResult = pCommandQueue->Signal(pFence.Get(), iCurrentFence);
        }

        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Wait until the GPU has completed commands up to this fence point.
        if (pFence->GetCompletedValue() < iCurrentFence) {
            const HANDLE hEvent = CreateEventEx(nullptr, nullptr, FALSE, EVENT_ALL_ACCESS);
            if (hEvent == nullptr) {
                return Error(GetLastError());
            }

            // Fire event when the GPU hits current fence.
            hResult = pFence->SetEventOnCompletion(iCurrentFence, hEvent);
            if (FAILED(hResult)) {
                return Error(hResult);
            }

            WaitForSingleObject(hEvent, INFINITE);
            CloseHandle(hEvent);
        }

        return {};
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

            vFilteredModes.push_back(mode);
        }

        return vFilteredModes;
    }

    void DirectXRenderer::writeConfigurationToConfigFile() {
        std::scoped_lock guard(mtxRwConfigFile);

        ConfigManager manager;

        if (isConfigurationFileExists()) {
            // Add existing values first.
            auto error = manager.loadFile(getRendererConfigurationFilePath());
            if (error.has_value()) {
                error->addEntry();
                // error->showError(); don't show on screen as it's not a critical error
                Logger::get().error(error->getError(), sRendererLogCategory);
                return;
            }
        }

        // Write GPU.
        manager.setValue<int>(
            getConfigurationSectionGpu(),
            "GPU",
            iPreferredGpuIndex,
            "index of the GPU to use, where '0' is the GPU with the most priority "
            "(first found GPU), '1' is the second found and etc.");

        // Write resolution.
        manager.setValue<unsigned int>(
            getConfigurationSectionResolution(), "width", currentDisplayMode.Width);
        manager.setValue<unsigned int>(
            getConfigurationSectionResolution(), "height", currentDisplayMode.Height);

        // Write refresh rate.
        manager.setValue<unsigned int>(
            getConfigurationSectionRefreshRate(), "numerator", currentDisplayMode.RefreshRate.Numerator);
        manager.setValue<unsigned int>(
            getConfigurationSectionRefreshRate(), "denominator", currentDisplayMode.RefreshRate.Denominator);

        // Write antialiasing.
        manager.setValue<bool>(getConfigurationSectionAntialiasing(), "enabled", bIsMsaaEnabled);
        manager.setValue<unsigned int>(
            getConfigurationSectionAntialiasing(),
            "sample_count",
            iMsaaSampleCount,
            "valid values for MSAA: 2 or 4");

        // Write VSync.
        manager.setValue<bool>(getConfigurationSectionVSync(), "enabled", bIsVSyncEnabled);

        // !!!
        // New settings go here!
        // !!!

        auto error = manager.saveFile(getRendererConfigurationFilePath(), false);
        if (error.has_value()) {
            error->addEntry();
            // error->showError(); don't show on screen as it's not a critical error
            Logger::get().error(error->getError(), sRendererLogCategory);
        }
    }

    void DirectXRenderer::readConfigurationFromConfigFile() {
        std::scoped_lock guard(mtxRwConfigFile);

        if (!isConfigurationFileExists()) {
            return;
        }

        ConfigManager manager;

        // Try loading file.
        auto loadError = manager.loadFile(getRendererConfigurationFilePath());
        if (loadError.has_value()) {
            loadError->addEntry();
            loadError->showError();
        }

        // Read GPU.
        iPreferredGpuIndex = manager.getValue<long>(getConfigurationSectionGpu(), "GPU", 0);

        // Read resolution.
        currentDisplayMode.Width =
            manager.getValue<unsigned int>(getConfigurationSectionResolution(), "width", 0);
        currentDisplayMode.Height =
            manager.getValue<unsigned int>(getConfigurationSectionResolution(), "height", 0);
        if (currentDisplayMode.Width == 0 || currentDisplayMode.Height == 0) {
            Error error(std::format(
                "failed to read valid resolution values from configuration file, "
                "either fix the values or delete this configuration file: \"{}\"",
                std::filesystem::path(manager.getFilePath()).string()));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getError());
        }

        // Read refresh rate.
        currentDisplayMode.RefreshRate.Numerator =
            manager.getValue<unsigned int>(getConfigurationSectionRefreshRate(), "numerator", 0);
        currentDisplayMode.RefreshRate.Denominator =
            manager.getValue<unsigned int>(getConfigurationSectionRefreshRate(), "denominator", 0);
        if (currentDisplayMode.RefreshRate.Numerator == 0 ||
            currentDisplayMode.RefreshRate.Denominator == 0) {
            Error error(std::format(
                "failed to read valid refresh rate values from configuration file, "
                "either fix the values or delete this configuration file: \"{}\"",
                std::filesystem::path(manager.getFilePath()).string()));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getError());
        }

        // Read antialiasing.
        bIsMsaaEnabled = manager.getValue<bool>(getConfigurationSectionAntialiasing(), "enabled", false);
        iMsaaSampleCount =
            manager.getValue<unsigned int>(getConfigurationSectionAntialiasing(), "sample_count", 0);
        if (iMsaaSampleCount != 2 && iMsaaSampleCount != 4) {
            Error error(std::format(
                "failed to read valid antialiasing values from configuration file, "
                "either fix the values or delete this configuration file: \"{}\"",
                std::filesystem::path(manager.getFilePath()).string()));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getError());
        }

        // Read VSync.
        bIsVSyncEnabled = manager.getValue<bool>(getConfigurationSectionVSync(), "enabled", false);

        // !!!
        // New settings go here!
        // !!!
    }
} // namespace ne
