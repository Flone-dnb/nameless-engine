#pragma once

namespace ne {
    class Renderer;

    /** Small render-independent command list wrapper. */
    class GpuCommandList {
    public:
        /**
         * Saves data to use.
         *
         * @param pRenderer Renderer that owns this command list.
         */
        GpuCommandList(Renderer* pRenderer);

        GpuCommandList() = delete;
        GpuCommandList(GpuCommandList&) = delete;
        GpuCommandList& operator=(GpuCommandList&) = delete;

    private:
        /** Do not delete (free) this pointer. Renderer that owns this command list. */
        Renderer* pRenderer = nullptr;
    };
} // namespace ne
