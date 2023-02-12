#pragma once

struct ID3D12GraphicsCommandList;

namespace ne {
    class Renderer;

    /**
     * Small render-independent non-owning command list wrapper.
     *
     * @remark Generally used for temporary render-independent command list access.
     */
    class GpuCommandList {
    public:
        GpuCommandList() = delete;
        GpuCommandList(GpuCommandList&) = delete;
        GpuCommandList& operator=(GpuCommandList&) = delete;

        /**
         * Creates a new non-owning command list wrapper.
         *
         * @param pCommandList DirectX renderer's command list.
         */
        GpuCommandList(ID3D12GraphicsCommandList* pCommandList);

    private:
#if defined(WIN32)
        /** Contains commands for the GPU. */
        ID3D12GraphicsCommandList* pCommandList = nullptr;
#endif
    };
} // namespace ne
