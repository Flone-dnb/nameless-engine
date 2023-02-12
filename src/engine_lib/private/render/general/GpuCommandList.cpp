// Custom.
#include "GpuCommandList.h"
#include "render/Renderer.h"

namespace ne {
    GpuCommandList::GpuCommandList(ID3D12GraphicsCommandList* pCommandList) {
        this->pCommandList = pCommandList;
    }
} // namespace ne
