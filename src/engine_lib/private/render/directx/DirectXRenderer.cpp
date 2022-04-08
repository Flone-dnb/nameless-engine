#include "DirectXRenderer.h"

// STL.
#include <format>
#include <filesystem>

// Custom.
#include "io/ConfigManager.h"
#include "io/Logger.h"

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
        if (isConfigurationFileExists()) {
            Logger::get().info("renderer found configuration file, using configuration from this file");
            DirectXRenderer::readConfigurationFromConfigFile();
            bStartedWithConfigurationFromDisk = true;
        }

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
        auto result = DirectXRenderer::getSupportedGpus();
        if (std::holds_alternative<Error>(result)) {
            Error error = std::get<Error>(std::move(result));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getError());
        }
        vSupportedVideoAdapters = std::get<std::vector<std::wstring>>(std::move(result));
        if (iPreferredGpuIndex >= static_cast<long>(vSupportedVideoAdapters.size())) {
            Error error(std::format("preferred GPU index {} is out of range, supported GPUs in total: {}\n",
                                    iPreferredGpuIndex, vSupportedVideoAdapters.size()));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getError());
        }

        // Use video adapter.
        setVideoAdapter(vSupportedVideoAdapters[iPreferredGpuIndex]);

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

        if (bIsMsaaEnabled) {
            // Check MSAA support.
            std::optional<Error> error = checkMsaaSupport();
            if (error.has_value()) {
                error->addEntry();
                error->showError();
                throw std::runtime_error(error->getError());
            }
        }

        // Create command queue and command list.
        std::optional<Error> error = createCommandObjects();
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

        // Determine display mode.
        auto videoModesResult = getSupportedDisplayModes();
        if (std::holds_alternative<Error>(videoModesResult)) {
            Error error1 = std::get<Error>(std::move(videoModesResult));
            error1.addEntry();
            error1.showError();
            throw std::runtime_error(error1.getError());
        }

        if (bStartedWithConfigurationFromDisk) {
            // Find specified video mode.
            const auto vVideoModes = std::get<std::vector<DXGI_MODE_DESC>>(std::move(videoModesResult));

            std::vector<DXGI_MODE_DESC> vFilteredModes;

            for (const auto &mode : vVideoModes) {
                if (mode.Width == currentDisplayMode.Width && mode.Height == currentDisplayMode.Height &&
                    mode.RefreshRate.Numerator == currentDisplayMode.RefreshRate.Numerator &&
                    mode.RefreshRate.Denominator == currentDisplayMode.RefreshRate.Denominator) {
                    vFilteredModes.push_back(mode);
                }
            }

            if (vFilteredModes.empty()) {
                Error error1(std::format("video mode with resolution ({}x{}) and refresh rate "
                                         "({}/{}) is not supported",
                                         currentDisplayMode.Width, currentDisplayMode.Height,
                                         currentDisplayMode.RefreshRate.Numerator,
                                         currentDisplayMode.RefreshRate.Denominator));
                error1.addEntry();
                error1.showError();
                throw std::runtime_error(error1.getError());
            } else if (vFilteredModes.size() == 1) {
                currentDisplayMode = vFilteredModes[0];
            } else {
                std::string sErrorMessage = std::format("video mode with resolution ({}x{}) and refresh rate "
                                                        "({}/{}) matched multiple supported modes, "
                                                        "please pick one of the following:\n",
                                                        currentDisplayMode.Width, currentDisplayMode.Height,
                                                        currentDisplayMode.RefreshRate.Numerator,
                                                        currentDisplayMode.RefreshRate.Denominator);
                for (const auto &mode : vFilteredModes) {
                    sErrorMessage +=
                        std::format("- resolution: {}x{}, refresh rate: {}/{}, format: {}, "
                                    "scanline ordering: {}, scaling: {}",
                                    mode.Width, mode.Height, mode.RefreshRate.Numerator,
                                    mode.RefreshRate.Denominator, static_cast<unsigned int>(mode.Format),
                                    static_cast<int>(mode.ScanlineOrdering), static_cast<int>(mode.Scaling));
                }
                Error error1(sErrorMessage);
                error1.addEntry();
                error1.showError();
                throw std::runtime_error(error1.getError());
            }
        } else {
            // use last display mode
            currentDisplayMode = std::get<std::vector<DXGI_MODE_DESC>>(std::move(videoModesResult)).back();
        }

        if (!isConfigurationFileExists()) {
            DirectXRenderer::writeConfigurationToConfigFile();
        }
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

    std::variant<std::vector<std::wstring>, Error> DirectXRenderer::getSupportedGpus() const {
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
                vAddedVideoAdapters.push_back(desc.Description);
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

    std::wstring DirectXRenderer::getCurrentlyUsedGpuName() const { return sUsedVideoAdapter; }

    Antialiasing DirectXRenderer::getCurrentAntialiasing() const {
        return Antialiasing{bIsMsaaEnabled, static_cast<int>(iMsaaSampleCount)};
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
        msQualityLevels.SampleCount = iMsaaSampleCount;
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

        // Filter some modes.
        std::vector<DXGI_MODE_DESC> vFilteredModes;
        for (const auto &mode : vDisplayModes) {
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
                Logger::get().error(error->getError());
                return;
            }
        }

        // Write GPU.
        manager.setLongValue(getConfigurationSectionGpu(), "GPU", 0,
                             "index of the GPU to use, where '0' is the GPU with the most priority "
                             "(first found GPU), '1' is the second found and etc.");

        // Write resolution.
        manager.setLongValue(getConfigurationSectionResolution(), "x",
                             static_cast<long>(currentDisplayMode.Width));
        manager.setLongValue(getConfigurationSectionResolution(), "y",
                             static_cast<long>(currentDisplayMode.Height));

        // Write refresh rate.
        manager.setLongValue(getConfigurationSectionRefreshRate(), "numerator",
                             static_cast<long>(currentDisplayMode.RefreshRate.Numerator));
        manager.setLongValue(getConfigurationSectionRefreshRate(), "denominator",
                             static_cast<long>(currentDisplayMode.RefreshRate.Denominator));

        // Write antialiasing.
        manager.setBoolValue(getConfigurationSectionAntialiasing(), "enabled", bIsMsaaEnabled);
        manager.setLongValue(getConfigurationSectionAntialiasing(), "sample count",
                             static_cast<long>(iMsaaSampleCount), "valid values for MSAA: 2 or 4");

        // !!!
        // New settings go here!
        // !!!

        auto error = manager.saveFile(getRendererConfigurationFilePath(), false);
        if (error.has_value()) {
            error->addEntry();
            // error->showError(); don't show on screen as it's not a critical error
            Logger::get().error(error->getError());
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
        iPreferredGpuIndex = manager.getLongValue(getConfigurationSectionGpu(), "GPU", 0);

        // Read resolution.
        currentDisplayMode.Width = manager.getLongValue(getConfigurationSectionResolution(), "x", 0);
        currentDisplayMode.Height = manager.getLongValue(getConfigurationSectionResolution(), "y", 0);
        if (currentDisplayMode.Width == 0 || currentDisplayMode.Height == 0) {
            Error error(std::format("failed to read valid resolution values from configuration file, "
                                    "either fix the values or delete this configuration file: \"{}\"",
                                    std::filesystem::path(manager.getFilePath()).string()));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getError());
        }

        // Read refresh rate.
        currentDisplayMode.RefreshRate.Numerator =
            manager.getLongValue(getConfigurationSectionRefreshRate(), "numerator", 0);
        currentDisplayMode.RefreshRate.Denominator =
            manager.getLongValue(getConfigurationSectionRefreshRate(), "denominator", 0);
        if (currentDisplayMode.RefreshRate.Numerator == 0 ||
            currentDisplayMode.RefreshRate.Denominator == 0) {
            Error error(std::format("failed to read valid refresh rate values from configuration file, "
                                    "either fix the values or delete this configuration file: \"{}\"",
                                    std::filesystem::path(manager.getFilePath()).string()));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getError());
        }

        // Read antialiasing.
        bIsMsaaEnabled = manager.getBoolValue(getConfigurationSectionAntialiasing(), "enabled", false);
        iMsaaSampleCount = manager.getLongValue(getConfigurationSectionAntialiasing(), "sample count", 0);
        if (iMsaaSampleCount != 2 && iMsaaSampleCount != 4) {
            Error error(std::format("failed to read valid antialiasing values from configuration file, "
                                    "either fix the values or delete this configuration file: \"{}\"",
                                    std::filesystem::path(manager.getFilePath()).string()));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getError());
        }

        // !!!
        // New settings go here!
        // !!!
    }
} // namespace ne
