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

        // Get supported display adapter.
        getFirstSupportedAdapter(*pAdapter.GetAddressOf());

        // Create device.
        hResult = D3D12CreateDevice(pAdapter.Get(), rendererD3dFeatureLevel, IID_PPV_ARGS(&pDevice));
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

        // TODO.
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

    std::optional<Error> DirectXRenderer::getFirstSupportedAdapter(IDXGIAdapter3 *&pOutAdapter) const {
        for (UINT iAdapterIndex = 0;; iAdapterIndex++) {
            IDXGIAdapter3 *pCurrentAdapter = nullptr;

            if (pFactory->EnumAdapters(iAdapterIndex, reinterpret_cast<IDXGIAdapter **>(&pCurrentAdapter)) ==
                DXGI_ERROR_NOT_FOUND) {
                // No more adapters to enumerate.
                break;
            }

            // Check if the adapter supports used D3D version, but don't create the actual device yet.
            const HRESULT hResult =
                D3D12CreateDevice(pCurrentAdapter, rendererD3dFeatureLevel, _uuidof(ID3D12Device), nullptr);
            if (SUCCEEDED(hResult)) {
                // Found supported adapter.
                pOutAdapter = pCurrentAdapter;
                return {};
            }

            pCurrentAdapter->Release();
            pCurrentAdapter = nullptr;
        }

        return Error("could not find a supported display adapter");
    }

    std::optional<Error> DirectXRenderer::checkMsaaSupport() {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
        msQualityLevels.Format = backBufferFormat;
        msQualityLevels.SampleCount = 4;
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
} // namespace ne
