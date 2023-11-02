#include "HlslComputeShaderInterface.h"

// Custom.
#include "render/Renderer.h"

namespace ne {
    HlslComputeShaderInterface::HlslComputeShaderInterface(
        Renderer* pRenderer, const std::string& sComputeShaderName, bool bRunBeforeFrameRendering)
        : ComputeShaderInterface(pRenderer, sComputeShaderName, bRunBeforeFrameRendering) {}

}
