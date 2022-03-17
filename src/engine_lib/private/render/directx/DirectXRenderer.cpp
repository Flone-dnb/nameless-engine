#include "DirectXRenderer.h"

// STL.
#include <format>

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
    DirectXRenderer::DirectXRenderer() {
        DWORD debugFactoryFlags = 0;

#if defined(DEBUG) || defined(_DEBUG)
        {
            std::optional<Error> error = enableDebugLayer();
            if (error.has_value()) {
                error->addEntry();
                error->showError();
                throw std::runtime_error(error->getError());
            }
            debugFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
        }
#endif

        // Create DXGI factory.
        HRESULT hResult = CreateDXGIFactory2(debugFactoryFlags, IID_PPV_ARGS(&pFactory));
        if (FAILED(hResult)) {
            const Error error(hResult);
            error.showError();
            throw std::runtime_error(error.getError());
        }

        // Get supported video adapters.
        auto result = DirectXRenderer::getSupportedVideoAdapters();
        if (std::holds_alternative<Error>(result)) {
            Error error = std::get<Error>(std::move(result));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getError());
        }
        vSupportedVideoAdapters = std::get<std::vector<std::wstring>>(std::move(result));

        // Use first video adapter.
        setVideoAdapter(vSupportedVideoAdapters[0]);

        // Create device.
        hResult = D3D12CreateDevice(pVideoAdapter.Get(), rendererD3dFeatureLevel, IID_PPV_ARGS(&pDevice));
        if (FAILED(hResult)) {
            const Error error(hResult);
            error.showError();
            throw std::runtime_error(error.getError());
        }

        // Create fence.
        hResult = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence));
        if (FAILED(hResult)) {
            const Error error(hResult);
            error.showError();
            throw std::runtime_error(error.getError());
        }

        // Get descriptor sizes.
        iRtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        iDsvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        iCbvSrvUavDescriptorSize =
            pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // Check MSAA support.
        std::optional<Error> error = checkMsaaSupport();
        if (error.has_value()) {
            error->addEntry();
            error->showError();
            throw std::runtime_error(error->getError());
        }

        // Create command queue and command list.
        error = createCommandObjects();
        if (error.has_value()) {
            error->addEntry();
            error->showError();
            throw std::runtime_error(error->getError());
        }

        // Set output adapter (monitor) to use.
        error = setOutputAdapter();
        if (error.has_value()) {
            error->addEntry();
            error->showError();
            throw std::runtime_error(error->getError());
        }

        auto videoModesResult = getSupportedDisplayModes();
        if (std::holds_alternative<Error>(videoModesResult)) {
            Error error1 = std::get<Error>(std::move(videoModesResult));
            error1.addEntry();
            error1.showError();
            throw std::runtime_error(error1.getError());
        }
        currentDisplayMode = std::get<std::vector<DXGI_MODE_DESC>>(std::move(videoModesResult)).back();
    }

    void DirectXRenderer::update() {}

    void DirectXRenderer::drawFrame() {}

    std::optional<Error> DirectXRenderer::enableDebugLayer() const {
        Microsoft::WRL::ComPtr<ID3D12Debug> pDebugController;
        HRESULT hResult = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        pDebugController->EnableDebugLayer();

        Microsoft::WRL::ComPtr<IDXGIInfoQueue> pDxgiInfoQueue;
        hResult = DXGIGetDebugInterface1(0, IID_PPV_ARGS(pDxgiInfoQueue.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true);

        return {};
    }

    std::variant<std::vector<std::wstring>, Error> DirectXRenderer::getSupportedVideoAdapters() const {
        std::vector<std::wstring> vAddedVideoAdapters;

        for (UINT iAdapterIndex = 0;; iAdapterIndex++) {
            Microsoft::WRL::ComPtr<IDXGIAdapter3> pTestAdapter;

            if (pFactory->EnumAdapters(iAdapterIndex,
                                       reinterpret_cast<IDXGIAdapter **>(pTestAdapter.GetAddressOf())) ==
                DXGI_ERROR_NOT_FOUND) {
                // No more adapters to enumerate.
                break;
            }

            // Check if the adapter supports used D3D version, but don't create the actual device yet.
            const HRESULT hResult = D3D12CreateDevice(pTestAdapter.Get(), rendererD3dFeatureLevel,
                                                      _uuidof(ID3D12Device), nullptr);
            if (SUCCEEDED(hResult)) {
                // Found supported adapter.
                DXGI_ADAPTER_DESC desc;
                pTestAdapter->GetDesc(&desc);
                vAddedVideoAdapters.emplace_back(desc.Description);
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
        for (const auto &mode : vRenderModes) {
            vOutRenderModes.push_back(RenderMode{static_cast<int>(mode.Width), static_cast<int>(mode.Height),
                                                 static_cast<int>(mode.RefreshRate.Numerator),
                                                 static_cast<int>(mode.RefreshRate.Denominator)});
        }

        return vOutRenderModes;
    }

    std::optional<Error> DirectXRenderer::setVideoAdapter(const std::wstring &sVideoAdapterName) {
        if (pVideoAdapter) {
            return Error("another video adapter already set");
        }

        for (UINT iAdapterIndex = 0;; iAdapterIndex++) {
            Microsoft::WRL::ComPtr<IDXGIAdapter3> pTestAdapter;
            if (pFactory->EnumAdapters(iAdapterIndex,
                                       reinterpret_cast<IDXGIAdapter **>(pTestAdapter.GetAddressOf())) ==
                DXGI_ERROR_NOT_FOUND) {
                // No more adapters to enumerate.
                break;
            }

            DXGI_ADAPTER_DESC desc;
            pTestAdapter->GetDesc(&desc);
            if (desc.Description == sVideoAdapterName) {
                // Check if the adapter supports used D3D version, but don't create the actual device yet.
                const HRESULT hResult = D3D12CreateDevice(pTestAdapter.Get(), rendererD3dFeatureLevel,
                                                          _uuidof(ID3D12Device), nullptr);
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

        Microsoft::WRL::ComPtr<IDXGIOutput> pTestOutput;

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
        hResult = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                  IID_PPV_ARGS(pCommandListAllocator.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Create Command List.
        hResult = pDevice->CreateCommandList(
            0, // Create list for only one GPU. See pDevice->GetNodeCount() and documentation for more info.
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            pCommandListAllocator.Get(), // Associated command allocator
            nullptr, IID_PPV_ARGS(pCommandList.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Start in a closed state. This is because the first time we refer
        // to the command list (later) we will Reset() it (put in 'Open' state), and in order
        // for Reset() to work, command list should be in closed state.
        pCommandList->Close();

        return {};
    }

    std::optional<Error> DirectXRenderer::checkMsaaSupport() {
        if (pDevice == nullptr) {
            return Error("device is not created");
        }

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
        msQualityLevels.Format = backBufferFormat;
        msQualityLevels.SampleCount = 4; // check for highest level support
        msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        msQualityLevels.NumQualityLevels = 0;

        const HRESULT hResult = pDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
                                                             &msQualityLevels, sizeof(msQualityLevels));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        if (msQualityLevels.NumQualityLevels == 0) {
            return Error("zero quality levels returned for MSAA support");
        }

        iMsaaQuality = msQualityLevels.NumQualityLevels;

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
            pOutputAdapter->GetDisplayModeList(backBufferFormat, 0, &iDisplayModeCount, &vDisplayModes[0]);
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        return vDisplayModes;
    }
} // namespace ne
